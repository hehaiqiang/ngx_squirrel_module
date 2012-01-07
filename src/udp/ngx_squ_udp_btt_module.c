
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_lua_btt.h>
#include <ngx_lua_udp_module.h>


static int ngx_lua_udp_btt(lua_State *l);

static ngx_int_t ngx_lua_udp_btt_module_init(ngx_cycle_t *cycle);


ngx_module_t  ngx_lua_udp_btt_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_UDP_MODULE,                        /* module type */
    NULL,                                  /* init master */
    ngx_lua_udp_btt_module_init,           /* init module */
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
        &ngx_lua_udp_btt_module,
        NULL
    };

    return modules;
}
#endif


static int
ngx_lua_udp_btt(lua_State *l)
{
    u_char             *p, *last;
#if 0
    ngx_str_t           key, *addr;
#endif
    ngx_btt_ctx_t      *ctx;
    ngx_lua_thread_t   *thr;
    ngx_lua_udp_ctx_t  *uctx;
    ngx_udp_session_t  *s;

    thr = ngx_lua_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "lua udp btt");

    ctx = ngx_pcalloc(thr->pool, sizeof(ngx_btt_ctx_t));
    if (ctx == NULL) {
        lua_pushboolean(l, 0);
        return 1;
    }

    uctx = thr->module_ctx;
    s = uctx->s;

    p = s->buffer->pos;
    last = s->buffer->last;

    if (last - p < 16) {
        goto invalid;
    }

    ctx->connection_id = (uint64_t) p[0] << 56 |
                         (uint64_t) p[1] << 48 |
                         (uint64_t) p[2] << 40 |
                         (uint64_t) p[3] << 32 |
                         (uint64_t) p[4] << 24 |
                         (uint64_t) p[5] << 16 |
                         (uint64_t) p[6] << 8 |
                         (uint64_t) p[7];
    p += sizeof(uint64_t);

    ctx->action = ntohl(*(uint32_t *) p);
    p += sizeof(uint32_t);

    ctx->transaction_id = ntohl(*(uint32_t *) p);
    p += sizeof(uint32_t);

    switch (ctx->action) {

    case NGX_BTT_ACTION_CONNECT:
        if (ctx->connection_id != 0x41727101980) {
            goto invalid;
        }

        /* TODO */

        ctx->connection_id = 1;

        /* TODO */

#if 0
        b->last = ngx_cpymem(b->last, &ctx->action, sizeof(uint32_t));
        b->last = ngx_cpymem(b->last, &ctx->transaction_id, sizeof(uint32_t));
        b->last = ngx_cpymem(b->last, &ctx->connection_id, sizeof(uint64_t));
#endif

        break;

    case NGX_BTT_ACTION_ANNOUNCE:
        /* TODO */
        break;

    case NGX_BTT_ACTION_SCRAPE:
        /* TODO */
        break;
    }

    /* TODO */

    lua_pushboolean(l, 1);

    return 1;

invalid:

    ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "invalid request");

    lua_pushboolean(l, 0);

    return 1;
}


static ngx_int_t
ngx_lua_udp_btt_module_init(ngx_cycle_t *cycle)
{
    ngx_lua_conf_t  *lcf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "lua udp btt module init");

    lcf = (ngx_lua_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_lua_module);

    lua_getglobal(lcf->l, NGX_LUA_TABLE);
    lua_getfield(lcf->l, -1, NGX_LUA_UDP_TABLE);

    lua_pushcfunction(lcf->l, ngx_lua_udp_btt);
    lua_setfield(lcf->l, -2, "btt");

    lua_pop(lcf->l, 2);

    return NGX_OK;
}
