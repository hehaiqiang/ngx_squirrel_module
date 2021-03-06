
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ.h>


typedef struct ngx_squ_smtp_cleanup_ctx_s  ngx_squ_smtp_cleanup_ctx_t;


typedef struct {
    ngx_url_t                      u;
    ngx_str_t                      user;
    ngx_str_t                      passwd;
    ngx_str_t                      from;
    ngx_array_t                    to;
    ngx_str_t                      subject;
    ngx_str_t                      content;
    ngx_msec_t                     connect_timeout;
    ngx_msec_t                     send_timeout;
    ngx_msec_t                     read_timeout;
    ngx_pool_t                    *pool;
    ngx_peer_connection_t          peer;
    ngx_buf_t                     *request;
    ngx_buf_t                     *response;
    ngx_int_t                      rc;
    ngx_uint_t                     not_event;
    ngx_uint_t                     state;
    ngx_uint_t                     n;
    ngx_squ_thread_t              *thr;
    ngx_squ_smtp_cleanup_ctx_t    *cln_ctx;
} ngx_squ_smtp_ctx_t;


struct ngx_squ_smtp_cleanup_ctx_s {
    ngx_squ_smtp_ctx_t            *ctx;
};


static int ngx_squ_smtp_check(squ_State *l);
static int ngx_squ_smtp_send(squ_State *l);

static ngx_int_t ngx_squ_smtp_parse_args(squ_State *l, ngx_squ_thread_t *thr,
    ngx_squ_smtp_ctx_t *ctx);

static void ngx_squ_smtp_connect_handler(ngx_event_t *wev);
static void ngx_squ_smtp_write_handler(ngx_event_t *wev);
static void ngx_squ_smtp_read_handler(ngx_event_t *rev);
static void ngx_squ_smtp_dummy_handler(ngx_event_t *ev);

static ngx_int_t ngx_squ_smtp_handle_response(ngx_squ_smtp_ctx_t *ctx);

static void ngx_squ_smtp_finalize(ngx_squ_smtp_ctx_t *ctx, char *errstr);
static void ngx_squ_smtp_cleanup(void *data);

static ngx_int_t ngx_squ_smtp_module_init(ngx_cycle_t *cycle);


static squL_Reg  ngx_squ_smtp_methods[] = {
    { "check", ngx_squ_smtp_check },
    { "send", ngx_squ_smtp_send },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_smtp_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_smtp_module_init,              /* init module */
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
        &ngx_squ_smtp_module,
        NULL
    };

    return modules;
}
#endif


static int
ngx_squ_smtp_check(squ_State *l)
{
    /* TODO */

    return 0;
}


static int
ngx_squ_smtp_send(squ_State *l)
{
    char                        *errstr;
    ngx_int_t                    rc;
    ngx_pool_t                  *pool;
    ngx_squ_thread_t            *thr;
    ngx_pool_cleanup_t          *cln;
    ngx_squ_smtp_ctx_t          *ctx;
    ngx_peer_connection_t       *peer;
    ngx_squ_smtp_cleanup_ctx_t  *cln_ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ smtp send");

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
        errstr = "ngx_create_pool() failed";
        goto error;
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_squ_smtp_ctx_t));
    if (ctx == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pcalloc() failed";
        goto error;
    }

    ctx->pool = pool;

    if (ngx_array_init(&ctx->to, pool, 16, sizeof(ngx_str_t)) == NGX_ERROR) {
        ngx_destroy_pool(pool);
        errstr = "ngx_array_init() failed";
        goto error;
    }

    cln_ctx = ngx_pcalloc(thr->pool, sizeof(ngx_squ_smtp_cleanup_ctx_t));
    if (cln_ctx == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pcalloc() failed";
        goto error;
    }

    cln_ctx->ctx = ctx;

    cln = ngx_pool_cleanup_add(thr->pool, 0);
    if (cln == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pool_cleanup_add() failed";
        goto error;
    }

    cln->handler = ngx_squ_smtp_cleanup;
    cln->data = cln_ctx;

    ctx->thr = thr;
    ctx->cln_ctx = cln_ctx;

    if (ngx_squ_smtp_parse_args(l, thr, ctx) == NGX_ERROR) {
        return 2;
    }

    ctx->u.default_port = 25;
    ctx->u.one_addr = 1;

    if (ngx_parse_url(pool, &ctx->u) != NGX_OK) {
        if (ctx->u.err) {
            ngx_log_error(NGX_LOG_EMERG, thr->log, 0,
                          "%s in url \"%V\"", ctx->u.err, &ctx->u.url);
        }

        errstr = ctx->u.err;
        goto error;
    }

    peer = &ctx->peer;

#if (NGX_UDT)
    peer->type = SOCK_STREAM;
#endif
    peer->sockaddr = ctx->u.addrs->sockaddr;
    peer->socklen = ctx->u.addrs->socklen;
    peer->name = &ctx->u.addrs->name;
    peer->get = ngx_event_get_peer;
    peer->log = ngx_cycle->log;
    peer->log_error = NGX_ERROR_ERR;
#if (NGX_THREADS)
    peer->lock = &thr->c->lock;
#endif

    rc = ngx_event_connect_peer(peer);

    ngx_log_debug1(NGX_LOG_DEBUG_CORE, thr->log, 0,
                   "squ smtp connecting to server: %i", rc);

    if (rc == NGX_ERROR || rc == NGX_BUSY || rc == NGX_DECLINED) {
        errstr = "ngx_event_connect_peer() failed";
        goto error;
    }

    peer->connection->data = ctx;
    peer->connection->pool = pool;

    peer->connection->read->handler = ngx_squ_smtp_dummy_handler;
    peer->connection->write->handler = ngx_squ_smtp_connect_handler;

    if (rc == NGX_AGAIN) {
        ngx_add_timer(peer->connection->write, ctx->connect_timeout);
        return squ_yield(l, 0);
    }

    /* rc == NGX_OK */

    ctx->rc = 0;
    ctx->not_event = 1;

    ngx_squ_smtp_connect_handler(peer->connection->write);

    ctx->not_event = 0;

    rc = ctx->rc;

    if (rc == NGX_AGAIN) {
        return squ_yield(l, 0);
    }

    cln_ctx->ctx = NULL;

    ngx_destroy_pool(ctx->pool);

    return rc;

error:

    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return 2;
}


static ngx_int_t
ngx_squ_smtp_parse_args(squ_State *l, ngx_squ_thread_t *thr,
    ngx_squ_smtp_ctx_t *ctx)
{
    int         top;
    char       *errstr;
    size_t      n, i;
    ngx_str_t   str, *to;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ smtp parse args");

    if (!squ_istable(l, 1)) {
        return squL_error(l, "invalid the first argument, must be a table");
    }

    top = squ_gettop(l);

    squ_getfield(l, 1, "host");
    str.data = (u_char *) squL_checklstring(l, -1, &str.len);

    ctx->u.url.len = str.len;
    ctx->u.url.data = ngx_pstrdup(ctx->pool, &str);
    if (ctx->u.url.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    squ_getfield(l, 1, "user");
    str.data = (u_char *) squL_checklstring(l, -1, &str.len);

    ctx->user.len = str.len;
    ctx->user.data = ngx_pstrdup(ctx->pool, &str);
    if (ctx->user.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    squ_getfield(l, 1, "password");
    str.data = (u_char *) squL_checklstring(l, -1, &str.len);

    ctx->passwd.len = str.len;
    ctx->passwd.data = ngx_pstrdup(ctx->pool, &str);
    if (ctx->passwd.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    squ_getfield(l, 1, "from");
    str.data = (u_char *) squL_checklstring(l, -1, &str.len);

    ctx->from.len = str.len;
    ctx->from.data = ngx_pstrdup(ctx->pool, &str);
    if (ctx->from.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    squ_getfield(l, 1, "to");
    if (!squ_istable(l, -1)) {
        return squL_error(l,
                          "invalid value of the argument \"to\""
                          ", must be a table");
    }

    n = squ_objlen(l, -1);
    if (n == 0) {
        return squL_error(l, "the argument \"to\" is an empty table");
    }

    for (i = 1; i <= n; i++) {
        to = ngx_array_push(&ctx->to);
        if (to == NULL) {
            errstr = "ngx_array_push() failed";
            goto error;
        }

        squ_rawgeti(l, -1, i);
        str.data = (u_char *) squL_checklstring(l, -1, &str.len);

        to->len = str.len;
        to->data = ngx_pstrdup(ctx->pool, &str);
        if (to->data == NULL) {
            errstr = "ngx_pstrdup() failed";
            goto error;
        }

        squ_pop(l, 1);
    }

    squ_getfield(l, 1, "subject");
    str.data = (u_char *) squL_checklstring(l, -1, &str.len);

    ctx->subject.len = str.len;
    ctx->subject.data = ngx_pstrdup(ctx->pool, &str);
    if (ctx->subject.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    squ_getfield(l, 1, "content");
    str.data = (u_char *) squL_checklstring(l, -1, &str.len);

    ctx->content.len = str.len;
    ctx->content.data = ngx_pstrdup(ctx->pool, &str);
    if (ctx->content.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    squ_settop(l, top);

    ctx->connect_timeout = (ngx_msec_t) squL_optnumber(l, 2, 60000);
    ctx->send_timeout = (ngx_msec_t) squL_optnumber(l, 3, 60000);
    ctx->read_timeout = (ngx_msec_t) squL_optnumber(l, 4, 60000);

    return NGX_OK;

error:

    squ_settop(l, top);
    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return NGX_ERROR;
}


static void
ngx_squ_smtp_connect_handler(ngx_event_t *wev)
{
    size_t               size;
    ngx_connection_t    *c;
    ngx_squ_smtp_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, wev->log, 0, "squ smtp connect handler");

    c = wev->data;
    ctx = c->data;

    if (wev->timedout) {
        ngx_log_error(NGX_LOG_ERR, wev->log, NGX_ETIMEDOUT,
                      "squ smtp connecting %V timed out", ctx->peer.name);
        ngx_squ_smtp_finalize(ctx, "ngx_squ_smtp_connect_handler() timed out");
        return;
    }

    if (wev->timer_set) {
        ngx_del_timer(wev);
    }

    c->read->handler = ngx_squ_smtp_read_handler;
    wev->handler = ngx_squ_smtp_dummy_handler;

    size = ngx_pagesize + ctx->subject.len + ctx->content.len;

    ctx->request = ngx_create_temp_buf(ctx->pool, size);
    if (ctx->request == NULL) {
        ngx_squ_smtp_finalize(ctx, "ngx_create_temp_buf() failed");
        return;
    }

    ctx->response = ngx_create_temp_buf(ctx->pool, ngx_pagesize);
    if (ctx->response == NULL) {
        ngx_squ_smtp_finalize(ctx, "ngx_create_temp_buf() failed");
        return;
    }

    ngx_squ_smtp_read_handler(wev);
}


static void
ngx_squ_smtp_write_handler(ngx_event_t *wev)
{
    ssize_t              n, size;
    ngx_buf_t           *b;
    ngx_connection_t    *c;
    ngx_squ_smtp_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, wev->log, 0, "squ smtp write handler");

    c = wev->data;
    ctx = c->data;

    if (wev->timedout) {
        ngx_log_error(NGX_LOG_ERR, wev->log, NGX_ETIMEDOUT,
                      "squ smtp write %V timed out", ctx->peer.name);
        ngx_squ_smtp_finalize(ctx, "ngx_squ_smtp_write_handler() failed");
        return;
    }

    if (wev->timer_set) {
        ngx_del_timer(wev);
    }

    b = ctx->request;

    while (1) {

        size = b->last - b->pos;

        n = ngx_send(c, b->pos, size);

        if (n > 0) {
            b->pos += n;

            if (n < size) {
                continue;
            }

            /* n == size */

            c->read->handler = ngx_squ_smtp_read_handler;
            wev->handler = ngx_squ_smtp_dummy_handler;

            ctx->response->last = ctx->response->pos;

            ngx_squ_smtp_read_handler(c->read);
            return;
        }

        if (n == NGX_AGAIN) {
            ngx_add_timer(wev, ctx->send_timeout);
            ctx->rc = NGX_AGAIN;
            return;
        }

        /* n == NGX_ERROR || n == 0 */

        ngx_squ_smtp_finalize(ctx, "ngx_send() failed");
        return;
    }
}


static void
ngx_squ_smtp_read_handler(ngx_event_t *rev)
{
    ssize_t              n;
    ngx_int_t            rc;
    ngx_buf_t           *b;
    ngx_connection_t    *c;
    ngx_squ_smtp_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, rev->log, 0, "squ smtp read handler");

    c = rev->data;
    ctx = c->data;

    if (rev->timedout) {
        ngx_log_error(NGX_LOG_ERR, rev->log, NGX_ETIMEDOUT,
                      "squ smtp read %V timed out", ctx->peer.name);
        ngx_squ_smtp_finalize(ctx, "ngx_squ_smtp_read_handler() timed out");
        return;
    }

    if (rev->timer_set) {
        ngx_del_timer(rev);
    }

    b = ctx->response;

    while (1) {

        n = ngx_recv(c, b->last, b->end - b->last);

        if (n > 0) {
            b->last += n;

            rc = ngx_squ_smtp_handle_response(ctx);

            if (rc == NGX_OK) {
                return;
            }

            if (rc == NGX_AGAIN) {
                continue;
            }

            if (rc == NGX_DONE) {
                ngx_squ_smtp_finalize(ctx, NULL);
                return;
            }

            /* rc == NGX_ERROR */

            ngx_squ_smtp_finalize(ctx, "ngx_squ_smtp_handle_response() failed");
            return;
        }

        if (n == NGX_AGAIN) {
            ngx_add_timer(rev, ctx->read_timeout);
            ctx->rc = NGX_AGAIN;
            return;
        }

        /* n == NGX_ERROR || n == 0 */

        ngx_squ_smtp_finalize(ctx, "ngx_recv() failed");
        return;
    }
}


static void
ngx_squ_smtp_dummy_handler(ngx_event_t *ev)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "squ smtp dummy handler");
}


static ngx_int_t
ngx_squ_smtp_handle_response(ngx_squ_smtp_ctx_t *ctx)
{
    u_char      *p, *last;
    ngx_str_t    dst, src, *to;
    ngx_buf_t   *b;
    ngx_uint_t   i;
    enum {
        sw_start = 0,
        sw_helo,
        sw_login,
        sw_user,
        sw_passwd,
        sw_from,
        sw_to,
        sw_data,
        sw_quit,
        sw_done
    } state;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0,
                   "squ smtp handle response");

    b = ctx->response;

    if (b->last - b->pos < 4) {
        return NGX_AGAIN;
    }

    if (*(b->last - 2) != CR || *(b->last - 1) != LF) {
        if (b->last == b->end) {
            *(b->last - 1) = '\0';
            ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                          "stmp server sent too long response line: \"%s\"",
                          b->pos);
            return NGX_ERROR;
        }

        return NGX_AGAIN;
    }

    p = b->pos;
    last = b->last;

    b = ctx->request;
    b->pos = b->start;

    state = ctx->state;

    switch (state) {

    case sw_start:
        if (p[0] != '2' || p[1] != '2' || p[2] != '0') {
            return NGX_ERROR;
        }

        b->last = ngx_slprintf(b->pos, b->end, "HELO %V" CRLF, &ctx->u.host);

        state = sw_helo;
        break;

    case sw_helo:
        if (p[0] != '2' || p[1] != '5' || p[2] != '0') {
            return NGX_ERROR;
        }

        b->last = ngx_cpymem(b->pos, "AUTH LOGIN" CRLF,
                             sizeof("AUTH LOGIN" CRLF) - 1);

        state = sw_login;
        break;

    case sw_login:
        if (p[0] != '3' || p[1] != '3' || p[2] != '4') {
            return NGX_ERROR;
        }

        src = ctx->user;

        dst.len = ngx_base64_encoded_length(src.len);
        dst.data = ngx_pnalloc(ctx->pool, dst.len);
        if (dst.data == NULL) {
            return NGX_ERROR;
        }

        ngx_encode_base64(&dst, &src);

        b->last = ngx_slprintf(b->pos, b->end, "%V" CRLF, &dst);

        state = sw_user;
        break;

    case sw_user:
        if (p[0] != '3' || p[1] != '3' || p[2] != '4') {
            return NGX_ERROR;
        }

        src = ctx->passwd;

        dst.len = ngx_base64_encoded_length(src.len);
        dst.data = ngx_pnalloc(ctx->pool, dst.len);
        if (dst.data == NULL) {
            return NGX_ERROR;
        }

        ngx_encode_base64(&dst, &src);

        b->last = ngx_slprintf(b->pos, b->end, "%V" CRLF, &dst);

        state = sw_passwd;
        break;

    case sw_passwd:
        if (p[0] != '2' || p[1] != '3' || p[2] != '5') {
            return NGX_ERROR;
        }

        b->last = ngx_slprintf(b->pos, b->end, "MAIL FROM:<%V>" CRLF,
                               &ctx->from);

        state = sw_from;
        break;

    case sw_from:
        if (p[0] != '2' || p[1] != '5' || p[2] != '0') {
            return NGX_ERROR;
        }

        to = ctx->to.elts;
        b->last = ngx_slprintf(b->pos, b->end, "RCPT TO:<%V>" CRLF,
                               &to[ctx->n++]);

        state = sw_to;
        break;

    case sw_to:
        if (p[0] != '2' || p[1] != '5' || p[2] != '0') {
            return NGX_ERROR;
        }

        if (ctx->n < ctx->to.nelts) {
            to = ctx->to.elts;
            b->last = ngx_slprintf(b->pos, b->end, "RCPT TO:<%V>" CRLF,
                                   &to[ctx->n++]);
            break;
        }

        b->last = ngx_cpymem(b->pos, "DATA" CRLF, sizeof("DATA" CRLF) - 1);

        state = sw_data;
        break;

    case sw_data:
        if (p[0] != '3' || p[1] != '5' || p[2] != '4') {
            return NGX_ERROR;
        }

        b->last = ngx_slprintf(b->pos, b->end, "Subject: %V" CRLF,
                               &ctx->subject);

        to = ctx->to.elts;
        for (i = 0; i < ctx->to.nelts; i++) {
            b->last = ngx_slprintf(b->last, b->end, "To: %V" CRLF, &to[i]);
        }

        b->last = ngx_slprintf(b->last, b->end, CRLF "%V" CRLF "." CRLF,
                               &ctx->content);

        state = sw_quit;
        break;

    case sw_quit:
        if (p[0] != '2' || p[1] != '5' || p[2] != '0') {
            return NGX_ERROR;
        }

        b->last = ngx_cpymem(b->pos, "QUIT" CRLF, sizeof("QUIT" CRLF) - 1);

        state = sw_done;
        break;

    case sw_done:
        if (p[0] != '2' || p[1] != '2' || p[2] != '1') {
            return NGX_ERROR;
        }

        return NGX_DONE;

    default:
        return NGX_ERROR;
    }

    ctx->state = state;

    ctx->peer.connection->read->handler = ngx_squ_smtp_dummy_handler;
    ctx->peer.connection->write->handler = ngx_squ_smtp_write_handler;

    ngx_squ_smtp_write_handler(ctx->peer.connection->write);

    return NGX_OK;
}


static void
ngx_squ_smtp_finalize(ngx_squ_smtp_ctx_t *ctx, char *errstr)
{
    ngx_int_t          rc;
    ngx_squ_thread_t  *thr;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ smtp finalize");

    if (ctx->cln_ctx != NULL) {
        ctx->cln_ctx->ctx = NULL;
    }

    thr = ctx->thr;

    if (thr == NULL) {
        if (ctx->peer.connection) {
            ngx_close_connection(ctx->peer.connection);
        }

        ngx_destroy_pool(ctx->pool);
        return;
    }

    ctx->rc = 1;

    if (errstr == NULL) {
        squ_pushboolean(thr->l, 1);

    } else {
        squ_pushboolean(thr->l, 0);
        squ_pushstring(thr->l, errstr);

        ctx->rc++;
    }

    if (ctx->peer.connection) {
        ngx_close_connection(ctx->peer.connection);
    }

    if (ctx->not_event) {
        return;
    }

    rc = ctx->rc;

    ngx_destroy_pool(ctx->pool);

    rc = ngx_squ_thread_run(thr, rc);
    if (rc == NGX_AGAIN) {
        return;
    }

    ngx_squ_finalize(thr, rc);
}


static void
ngx_squ_smtp_cleanup(void *data)
{
    ngx_squ_smtp_cleanup_ctx_t *cln_ctx = data;

    ngx_squ_smtp_ctx_t  *ctx;

    ctx = cln_ctx->ctx;

    if (ctx != NULL) {
        ctx->thr = NULL;
        ctx->cln_ctx = NULL;

        ngx_squ_smtp_finalize(ctx, NULL);
    }
}


static ngx_int_t
ngx_squ_smtp_module_init(ngx_cycle_t *cycle)
{
    int              n;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ smtp module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    squ_getglobal(scf->l, NGX_SQU_TABLE);

    n = sizeof(ngx_squ_smtp_methods) / sizeof(squL_Reg) - 1;

    squ_createtable(scf->l, 0, n);

    for (n = 0; ngx_squ_smtp_methods[n].name != NULL; n++) {
        squ_pushcfunction(scf->l, ngx_squ_smtp_methods[n].func);
        squ_setfield(scf->l, -2, ngx_squ_smtp_methods[n].name);
    }

    squ_setfield(scf->l, -2, "smtp");

    squ_pop(scf->l, 1);

    return NGX_OK;
}
