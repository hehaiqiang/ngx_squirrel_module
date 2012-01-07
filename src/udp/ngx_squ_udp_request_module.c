
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_udp_module.h>


static SQInteger ngx_squ_udp_request_get(HSQUIRRELVM v);

static ngx_int_t ngx_squ_udp_request_module_init(ngx_cycle_t *cycle);


ngx_module_t  ngx_squ_udp_request_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_UDP_MODULE,                        /* module type */
    NULL,                                  /* init master */
    ngx_squ_udp_request_module_init,       /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


#if (NGX_SQU_DLL)
ngx_module_t **
ngx_squ_get_modules(void)
{
    static ngx_module_t  *modules[] = {
        &ngx_squ_udp_request_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_udp_request_get(HSQUIRRELVM v)
{
    u_char             *p;
    SQRESULT            rc;
    ngx_str_t           key, *addr;
    ngx_squ_thread_t   *thr;
    ngx_squ_udp_ctx_t  *ctx;
    ngx_udp_session_t  *s;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ udp request get");

    ctx = thr->module_ctx;
    s = ctx->s;

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    switch (key.len) {

    case 4:

        if (ngx_strncmp(key.data, "data", 4) == 0) {
            sq_pushstring(v, (SQChar *) s->buffer->pos,
                          s->buffer->last - s->buffer->pos);
            return 1;
        }

        break;

    case 11:

        addr = &s->connection->addr_text;

        if (ngx_strncmp(key.data, "remote_addr", 11) == 0) {
            p = ngx_strlchr(addr->data, addr->data + addr->len, ':');
            if (p == NULL) {
                /* TODO: error handling */
                break;
            }

            sq_pushstring(v, (SQChar *) addr->data, p - addr->data);
            return 1;
        }

        if (ngx_strncmp(key.data, "remote_port", 11) == 0) {
            p = ngx_strlchr(addr->data, addr->data + addr->len, ':');
            if (p == NULL) {
                /* TODO: error handling */
                break;
            }

            p++;

            sq_pushstring(v, (SQChar *) p, addr->data + addr->len - p);
            return 1;
        }

        addr = s->addr_text;

        if (ngx_strncmp(key.data, "server_addr", 11) == 0) {
            p = ngx_strlchr(addr->data, addr->data + addr->len, ':');
            if (p == NULL) {
                /* TODO: error handling */
                break;
            }

            sq_pushstring(v, (SQChar *) addr->data, p - addr->data);
            return 1;
        }

        if (ngx_strncmp(key.data, "server_port", 11) == 0) {
            p = ngx_strlchr(addr->data, addr->data + addr->len, ':');
            if (p == NULL) {
                /* TODO: error handling */
                break;
            }

            p++;

            sq_pushstring(v, (SQChar *) p, addr->data + addr->len - p);
            return 1;
        }

        break;

    default:
        break;
    }

    sq_pushnull(v);
    sq_throwobject(v);

    return 0;
}


static ngx_int_t
ngx_squ_udp_request_module_init(ngx_cycle_t *cycle)
{
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ udp request module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_UDP_TABLE, sizeof(NGX_SQU_UDP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    sq_pushstring(scf->v, "request", sizeof("request") - 1);
    sq_newtableex(scf->v, 1);

    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_udp_request_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
