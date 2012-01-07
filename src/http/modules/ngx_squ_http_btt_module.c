
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_lua_btt.h>
#include <ngx_lua_http_module.h>


static int ngx_lua_http_btt_announce(lua_State *l);
static ngx_int_t ngx_lua_http_btt_parse_args(ngx_http_request_t *r,
    ngx_btt_ctx_t *ctx);
static ngx_int_t ngx_lua_http_btt_check_args(ngx_http_request_t *r,
    ngx_btt_ctx_t *ctx);

static ngx_int_t ngx_lua_http_btt_module_init(ngx_cycle_t *cycle);


ngx_module_t  ngx_lua_http_btt_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_lua_http_btt_module_init,          /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


#if (NGX_LUA_DLL)
ngx_module_t **
ngx_lua_get_modules(void)
{
    static ngx_module_t  *modules[] = {
        &ngx_lua_http_btt_module,
        NULL
    };

    return modules;
}
#endif


static int
ngx_lua_http_btt_announce(lua_State *l)
{
    size_t                size;
    u_char               *p, *last, *buf, ip[NGX_INET_ADDRSTRLEN];
    uint32_t              n;
    ngx_int_t             rc;
    ngx_btt_ctx_t        *ctx;
    ngx_btt_conf_t       *bcf;
    ngx_lua_thread_t     *thr;
    ngx_lua_http_ctx_t   *hctx;
    ngx_http_request_t   *r;
    ngx_btt_peer_info_t  *pi;

    thr = ngx_lua_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, thr->log, 0, "lua http btt announce");

    ctx = ngx_pcalloc(thr->pool, sizeof(ngx_btt_ctx_t));
    if (ctx == NULL) {
        lua_pushboolean(l, 0);
        return 1;
    }

    ctx->pool = thr->pool;
    ctx->log = thr->log;
    hctx = thr->module_ctx;
    r = hctx->r;

    if (ngx_lua_http_btt_parse_args(r, ctx) != NGX_OK) {
        lua_pushboolean(l, 0);
        return 1;
    }

    /* Checking the value of the uri arguments */

    if (ngx_lua_http_btt_check_args(r, ctx) != NGX_OK) {
        lua_pushboolean(l, 0);
        return 1;
    }

    bcf = (ngx_btt_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                          ngx_lua_btt_module);

    /* Insert or update peer information in peer list */

    /* TODO: Checking ctx->event */

    rc = ngx_btt_update_peer(bcf, ctx);
    if (rc != NGX_OK) {
        lua_pushboolean(l, 0);
        return 1;
    }

    /* Querying the peer list */

    if (ctx->event != NGX_BTT_EVENT_STOPPED) {
        ctx->peers = ngx_palloc(thr->pool,
                                sizeof(ngx_btt_peer_info_t) * ctx->numwant);
        if (ctx->peers == NULL) {
            lua_pushboolean(l, 0);
            return 1;
        }

        rc = ngx_btt_query_peers(bcf, ctx);
        if (rc != NGX_OK) {
            lua_pushboolean(l, 0);
            return 1;
        }
    }

    /* Calculating the size of the response */

    /* TODO:
     *
     * 8:completei2e
     * 10:downloadedi4e
     * 10:incompletei1e
     *
     * 12:min intervali811e
     */

    size = sizeof("d8:intervalie5:peerse") - 1 + NGX_TIME_T_LEN;

    if (ctx->compact) {
        size += NGX_INT_T_LEN + 1 + 6 * ctx->peers_n;

    } else {
        size += sizeof("le") - 1;

        if (!ctx->no_peer_id) {
            size += (sizeof("7:peer_id20:") - 1 + 20) * ctx->peers_n;
        }

        size += (sizeof("d2:ip4:portiee") - 1
                 + NGX_INT_T_LEN + 1 + NGX_INET_ADDRSTRLEN + NGX_INT_T_LEN)
                * ctx->peers_n;
    }

    buf = ngx_palloc(thr->pool, size);
    if (buf == NULL) {
        lua_pushboolean(l, 0);
        return 1;
    }

    p = buf;
    last = p + size;

    /* Building the response */

    p = ngx_slprintf(p, last, "d8:intervali%Te5:peers", bcf->interval);

    if (ctx->compact) {
        p = ngx_slprintf(p, last, "%uz:", 6 * ctx->peers_n);
    } else {
        *p++ = 'l';
    }

    for (n = 0; n < ctx->peers_n; n++) {
        pi = &ctx->peers[n];

        if (ctx->compact) {
            p = ngx_cpymem(p, &pi->external_ip, sizeof(pi->external_ip));

            pi->internal_port = htons(pi->internal_port);
            p = ngx_cpymem(p, &pi->internal_port, sizeof(pi->internal_port));

        } else {
            *p++ = 'd';

            if (!ctx->no_peer_id) {
                p = ngx_slprintf(p, last, "7:peer_id20:%*s",
                                 sizeof(pi->peer_id), pi->peer_id);
            }

            size = ngx_inet_ntop(AF_INET, &pi->external_ip, ip, sizeof(ip));
            p = ngx_slprintf(p, last, "2:ip%uz:%*s4:porti%uDe",
                             size, size, ip, (uint32_t) pi->internal_port);

            *p++ = 'e';
        }
    }

    if (!ctx->compact) {
        *p++ = 'e';
    }
    *p++ = 'e';

    ngx_lua_output(thr, buf, p - buf);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "lua http btt announce response:%N%*s", p - buf, buf);

    lua_pushboolean(l, 1);

    return 1;
}


static ngx_int_t
ngx_lua_http_btt_parse_args(ngx_http_request_t *r, ngx_btt_ctx_t *ctx)
{
    size_t   nlen, vlen;
    u_char  *p, *last, *name, *val, *dst, *src;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "lua http btt parse args");

    if (r->args.len == 0) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "invalid uri \"%V\"", &r->args);
        return NGX_ERROR;
    }

    p = r->args.data;
    last = p + r->args.len;

    for ( /* void */ ; p < last; p++) {

        name = p;

        p = ngx_strlchr(p, last, '&');

        if (p == NULL) {
            p = last;
        }

        val = ngx_strlchr(name, p, '=');

        if (val == NULL) {
            val = p;
        }

        nlen = val - name;
        vlen = (++val < p) ? p - val : 0;

        dst = ngx_pnalloc(r->pool, vlen);
        if (dst == NULL) {
            return NGX_ERROR;
        }

        src = val;
        val = dst;
        ngx_unescape_uri(&dst, &src, vlen, 0);
        vlen = dst - val;

        ngx_log_debug4(NGX_LOG_ALERT, r->connection->log, 0,
                       "%*s=%*s", nlen, name, vlen, val);

        /*
         * BT Client:
         *
         *   BitComet: localip, natmapped, port_type
         *   NetTransport: supportcrypto
         *   uTorrent: corrupt
         *   XunLei: ip
         */

        switch (nlen) {

        case 2:

            if (ngx_strncmp(name, "ip", nlen) == 0) {
                ctx->internal_ip = ngx_inet_addr(val, vlen);
                if (ctx->internal_ip == INADDR_NONE) {
                    goto invalid;
                }
                break;
            }

            break;

        case 3:

            if (ngx_strncmp(name, "key", nlen) == 0) {
                ngx_memcpy(ctx->key, val, vlen);
                break;
            }

            break;

        case 4:

            if (ngx_strncmp(name, "port", nlen) == 0) {
                ctx->internal_port = (in_port_t) ngx_atoi(val, vlen);
                if (ctx->internal_port == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            if (ngx_strncmp(name, "left", nlen) == 0) {
                ctx->left = ngx_atoof(val, vlen);
                if (ctx->left == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            break;

        case 5:

            if (ngx_strncmp(name, "event", nlen) == 0) {

                if (ngx_strncmp(val, "none", sizeof("none") - 1) == 0) {
                    ctx->event = NGX_BTT_EVENT_NONE;

                } else if (ngx_strncmp(val, "completed",
                                       sizeof("completed") - 1)
                           == 0)
                {
                    ctx->event = NGX_BTT_EVENT_COMPLETED;

                } else if (ngx_strncmp(val, "started", sizeof("started") - 1)
                           == 0)
                {
                    ctx->event = NGX_BTT_EVENT_STARTED;

                } else if (ngx_strncmp(val, "stopped", sizeof("stopped") - 1)
                           == 0)
                {
                    ctx->event = NGX_BTT_EVENT_STOPPED;

                } else {
                    goto invalid;
                }

                break;
            }

            break;

        case 7:

            if (ngx_strncmp(name, "peer_id", nlen) == 0) {
                ngx_memcpy(ctx->peer_id, val, vlen);
                break;
            }

            if (ngx_strncmp(name, "localip", nlen) == 0) {
                ctx->internal_ip = ngx_inet_addr(val, vlen);
                if (ctx->internal_ip == INADDR_NONE) {
                    goto invalid;
                }
                break;
            }

            if (ngx_strncmp(name, "numwant", nlen) == 0) {
                ctx->numwant = ngx_atoi(val, vlen);
                if (ctx->numwant == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            if (ngx_strncmp(name, "compact", nlen) == 0) {
                ctx->compact = ngx_atoi(val, vlen);
                if (ctx->compact == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            if (ngx_strncmp(name, "corrupt", nlen) == 0) {
                ctx->corrupt = ngx_atoi(val, vlen);
                if (ctx->corrupt == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            break;

        case 8:

            if (ngx_strncmp(name, "uploaded", nlen) == 0) {
                ctx->uploaded = ngx_atoof(val, vlen);
                if (ctx->uploaded == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            break;

        case 9:

            if (ngx_strncmp(name, "info_hash ", nlen) == 0) {
                ngx_memcpy(ctx->info_hash, val, vlen);
                break;
            }

            if (ngx_strncmp(name, "natmapped ", nlen) == 0) {
                ctx->natmapped = ngx_atoi(val, vlen);
                if (ctx->natmapped == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            if (ngx_strncmp(name, "port_type", nlen) == 0) {
                ngx_memcpy(ctx->port_type, val, vlen);
                break;
            }

            break;

        case 10:

            if (ngx_strncmp(name, "downloaded", nlen) == 0) {
                ctx->downloaded = ngx_atoof(val, vlen);
                if (ctx->downloaded == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            if (ngx_strncmp(name, "no_peer_id ", nlen) == 0) {
                ctx->no_peer_id = ngx_atoi(val, vlen);
                if (ctx->no_peer_id == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            break;

        case 13:

            if (ngx_strncmp(name, "supportcrypto", nlen) == 0) {
                ctx->supportcrypto = ngx_atoi(val, vlen);
                if (ctx->supportcrypto == NGX_ERROR) {
                    goto invalid;
                }
                break;
            }

            break;

        default:
            break;
        }
    }

    return NGX_OK;

invalid:

    ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                  "invalid value \"%*s\" of the key \"%*s\"",
                  nlen, name, vlen, val);

    return NGX_ERROR;
}


static ngx_int_t
ngx_lua_http_btt_check_args(ngx_http_request_t *r, ngx_btt_ctx_t *ctx)
{
    u_char              *p;
    struct sockaddr_in  *sin;

    /* TODO: IPv6 */

    ctx->external_ip = ngx_inet_addr(r->connection->addr_text.data,
                                     r->connection->addr_text.len);
    if (ctx->external_ip == INADDR_NONE) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "invalid external ip");
        return NGX_ERROR;
    }

    sin = (struct sockaddr_in *) r->connection->sockaddr;
    ctx->external_port = ntohs(sin->sin_port);

    if (ctx->internal_ip == 0) {
        ctx->internal_ip = ctx->external_ip;
    }

    /* TODO: connection_id */

    p = ngx_cpymem(&ctx->connection_id, &ctx->external_ip,
                   sizeof(ctx->external_ip));
    ngx_memcpy(p, &ctx->internal_port, sizeof(ctx->internal_port));

    return NGX_OK;
}


static ngx_int_t
ngx_lua_http_btt_module_init(ngx_cycle_t *cycle)
{
    ngx_lua_conf_t  *lcf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "lua http btt module init");

    lcf = (ngx_lua_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_lua_module);

    lua_getglobal(lcf->l, NGX_LUA_TABLE);
    lua_getfield(lcf->l, -1, NGX_LUA_HTTP_TABLE);

    lua_createtable(lcf->l, 0, 1);
    lua_pushcfunction(lcf->l, ngx_lua_http_btt_announce);
    lua_setfield(lcf->l, -2, "announce");
    lua_setfield(lcf->l, -2, "btt");

    lua_pop(lcf->l, 2);

    return NGX_OK;
}
