
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_tcp_module.h>


static SQInteger ngx_squ_tcp_response_send(HSQUIRRELVM v);
static void ngx_squ_tcp_response_write_handler(ngx_event_t *wev);
static void ngx_squ_tcp_response_dummy_handler(ngx_event_t *ev);

static ngx_int_t ngx_squ_tcp_response_module_init(ngx_cycle_t *cycle);


static SQRegFunction  ngx_squ_tcp_response_methods[] = {
    { "send", ngx_squ_tcp_response_send },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_tcp_response_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_TCP_MODULE,                        /* module type */
    NULL,                                  /* init master */
    ngx_squ_tcp_response_module_init,      /* init module */
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
        &ngx_squ_tcp_response_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_tcp_response_send(HSQUIRRELVM v)
{
    char               *errstr;
    ngx_chain_t        *cl;
    ngx_connection_t   *c;
    ngx_squ_thread_t   *thr;
    ngx_squ_tcp_ctx_t  *ctx;
    ngx_tcp_session_t  *s;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ tcp response send");

    ctx = thr->module_ctx;
    s = ctx->s;
    c = s->connection;

    /* TODO: the arguments in squ stack */

    cl = ctx->out;

    if (cl == NULL || cl->buf->last - cl->buf->pos == 0) {
        errstr = "no data";
        goto error;
    }

    c->read->handler = ngx_squ_tcp_response_dummy_handler;
    c->write->handler = ngx_squ_tcp_response_write_handler;

    c->sent = 0;

    ctx->rc = 0;
    ctx->not_event = 1;

    ngx_squ_tcp_response_write_handler(c->write);

    ctx->not_event = 0;

    if (ctx->rc != NGX_AGAIN) {
        return ctx->rc;
    }

    return sq_suspendvm(v);

error:

    sq_pushbool(v, SQFalse);
#if 0
    sq_pushstring(v, (SQChar *) errstr, -1);
#endif

    return 1;
}


static void
ngx_squ_tcp_response_write_handler(ngx_event_t *wev)
{
    char               *errstr;
    ssize_t             n;
    ngx_int_t           rc;
    ngx_chain_t        *cl;
    ngx_connection_t   *c;
    ngx_squ_thread_t   *thr;
    ngx_squ_tcp_ctx_t  *ctx;
    ngx_tcp_session_t  *s;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, wev->log, 0,
                   "squ tcp response write handler");

    c = wev->data;
    s = c->data;

    thr = ngx_tcp_get_module_ctx(s, ngx_squ_tcp_module);

    ctx = thr->module_ctx;
    errstr = NULL;

    if (wev->timedout) {
        ngx_log_error(NGX_LOG_ERR, wev->log, NGX_ETIMEDOUT,
                      "squ tcp response write %V timed out", &c->addr_text);
        errstr = "ngx_squ_tcp_response_write_handler() timed out";
        n = NGX_ERROR;
        goto done;
    }

    if (wev->timer_set) {
        ngx_del_timer(wev);
    }

    while (1) {

        cl = c->send_chain(c, ctx->out, 0);

        if (cl == NGX_CHAIN_ERROR) {
            n = NGX_ERROR;
            break;
        }

        if (cl != NULL) {
            /* TODO */
            ngx_add_timer(wev, 60000);

            if (ngx_handle_write_event(wev, 0) != NGX_OK) {
                errstr = "ngx_handle_write_event() failed";
                n = NGX_ERROR;
                goto done;
            }

            ctx->rc = NGX_AGAIN;
            return;
        }

        for (cl = ctx->out; cl != NULL; cl = cl->next) {
            cl->buf->pos = cl->buf->start;
            cl->buf->last = cl->buf->pos;
        }

        ctx->last = ctx->out;

        n = (ssize_t) c->sent;

        break;
    }

done:

    wev->handler = ngx_squ_tcp_response_dummy_handler;

    ctx->rc = 1;

    if (n > 0) {
        sq_pushinteger(thr->v, (SQInteger) c->sent);

    } else {
        sq_pushbool(thr->v, SQFalse);

#if 0
        if (errstr != NULL) {
            sq_pushstring(thr->v, (SQChar *) errstr, -1);

            ctx->rc++;
        }
#endif
    }

    if (ctx->not_event) {
        return;
    }

    rc = ngx_squ_thread_run(thr, ctx->rc);
    if (rc == NGX_AGAIN) {
        return;
    }

    ngx_squ_finalize(thr, rc);
}


static void
ngx_squ_tcp_response_dummy_handler(ngx_event_t *ev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0,
                   "squ tcp response dummy handler");
}


static ngx_int_t
ngx_squ_tcp_response_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ tcp response module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_TCP_TABLE, sizeof(NGX_SQU_TCP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_tcp_response_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "response", sizeof("response") - 1);
    sq_newtableex(scf->v, n);

    for (n = 0; ngx_squ_tcp_response_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_tcp_response_methods[n].name, -1);
        sq_newclosure(scf->v, ngx_squ_tcp_response_methods[n].f, 0);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
