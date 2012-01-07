
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_tcp_module.h>


static SQInteger ngx_squ_tcp_request_recv(HSQUIRRELVM v);
static void ngx_squ_tcp_request_read_handler(ngx_event_t *rev);
static void ngx_squ_tcp_request_dummy_handler(ngx_event_t *ev);

static ngx_int_t ngx_squ_tcp_request_module_init(ngx_cycle_t *cycle);


static SQRegFunction  ngx_squ_tcp_request_methods[] = {
    { "recv", ngx_squ_tcp_request_recv },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_tcp_request_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_TCP_MODULE,                        /* module type */
    NULL,                                  /* init master */
    ngx_squ_tcp_request_module_init,       /* init module */
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
        &ngx_squ_tcp_request_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_tcp_request_recv(HSQUIRRELVM v)
{
    char               *errstr;
    size_t              size;
    SQRESULT            rc;
    ngx_buf_t          *b;
    ngx_connection_t   *c;
    ngx_squ_thread_t   *thr;
    ngx_squ_tcp_ctx_t  *ctx;
    ngx_tcp_session_t  *s;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ tcp request recv");

    ctx = thr->module_ctx;
    s = ctx->s;
    c = s->connection;

    /* TODO: the arguments in squ stack */

    rc = sq_getinteger(v, 2, (SQInteger *) &size);

    b = s->buffer;

    if (b == NULL || (size_t) (b->end - b->start) < size) {
        if (b != NULL && (size_t) (b->end - b->start) > c->pool->max) {
            ngx_pfree(c->pool, b->start);
        }

        size = ngx_max(ngx_pagesize, size);

        b = ngx_create_temp_buf(c->pool, size);
        if (b == NULL) {
            errstr = "ngx_create_temp_buf() failed";
            goto error;
        }

        s->buffer = b;
    }

    b->last = b->pos;

    c->read->handler = ngx_squ_tcp_request_read_handler;
    c->write->handler = ngx_squ_tcp_request_dummy_handler;

    ctx->rc = 0;
    ctx->not_event = 1;

    ngx_squ_tcp_request_read_handler(c->read);

    ctx->not_event = 0;

    if (ctx->rc != NGX_AGAIN) {
        return ctx->rc;
    }

    return sq_suspendvm(v);

error:

    sq_pushbool(v, SQFalse);
#if 0
    sq_pushstring(, (SQChar *) errstr, -1);
#endif

    return 1;
}


static void
ngx_squ_tcp_request_read_handler(ngx_event_t *rev)
{
    char               *errstr;
    ssize_t             n;
    ngx_int_t           rc;
    ngx_buf_t          *b;
    ngx_connection_t   *c;
    ngx_squ_thread_t   *thr;
    ngx_squ_tcp_ctx_t  *ctx;
    ngx_tcp_session_t  *s;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, rev->log, 0,
                   "squ tcp request read handler");

    c = rev->data;
    s = c->data;

    thr = ngx_tcp_get_module_ctx(s, ngx_squ_tcp_module);

    ctx = thr->module_ctx;
    b = s->buffer;
    errstr = NULL;

    if (rev->timedout) {
        ngx_log_error(NGX_LOG_ERR, rev->log, NGX_ETIMEDOUT,
                      "squ tcp request read %V timed out", &c->addr_text);
        errstr = "ngx_squ_tcp_request_read_handler() timed out";
        n = NGX_ERROR;
        goto done;
    }

    if (rev->timer_set) {
        ngx_del_timer(rev);
    }

    while (1) {

        n = ngx_recv(c, b->last, b->end - b->last);

        if (n > 0) {
            b->last += n;
            break;
        }

        if (n == NGX_AGAIN) {
            /* TODO */
            ngx_add_timer(rev, 60000);

            if (ngx_handle_read_event(rev, 0) != NGX_OK) {
                errstr = "ngx_handle_read_event() failed";
                n = NGX_ERROR;
                goto done;
            }

            ctx->rc = NGX_AGAIN;
            return;
        }

        /* n == NGX_ERROR || n == 0 */

        break;
    }

done:

    rev->handler = ngx_squ_tcp_request_dummy_handler;

    ctx->rc = 1;

    if (n > 0) {
        sq_pushstring(thr->v, (SQChar *) b->pos, n);

    } else {
        sq_pushbool(thr->v, SQFalse);

#if 0
        if (errstr != NULL) {
            sq_pushstring(thr->v, errstr, -1);

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
ngx_squ_tcp_request_dummy_handler(ngx_event_t *ev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0,
                   "squ tcp request dummy handler");
}


static ngx_int_t
ngx_squ_tcp_request_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ tcp request module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_TCP_TABLE, sizeof(NGX_SQU_TCP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_tcp_request_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "request", sizeof("request") - 1);
    sq_newtableex(scf->v, n);

    for (n = 0; ngx_squ_tcp_request_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_tcp_request_methods[n].name, -1);
	sq_newclosure(scf->v, ngx_squ_tcp_request_methods[n].f, 0);
	rc = sq_newslot(scf->v, -3, SQFalse);
    }

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
