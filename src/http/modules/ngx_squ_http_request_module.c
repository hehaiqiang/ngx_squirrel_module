
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_http_module.h>


static SQInteger ngx_squ_request_headers_get(HSQUIRRELVM v);
static SQInteger ngx_squ_request_cookies_get(HSQUIRRELVM v);
static SQInteger ngx_squ_request_get_get(HSQUIRRELVM v);
static SQInteger ngx_squ_request_post_get(HSQUIRRELVM v);
static SQInteger ngx_squ_request_get(HSQUIRRELVM v);

static ngx_int_t ngx_squ_request_copy_request_body(ngx_http_request_t *r,
    ngx_squ_http_ctx_t *ctx);
static ngx_int_t ngx_squ_request_get_posted_arg(ngx_str_t *posted,
    ngx_str_t *key, ngx_str_t *value);

static ngx_int_t ngx_squ_request_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_request_consts[] = {
    { "UNKNOWN", NGX_HTTP_UNKNOWN },
    { "GET", NGX_HTTP_GET },
    { "HEAD", NGX_HTTP_HEAD },
    { "POST", NGX_HTTP_POST },
    { "PUT", NGX_HTTP_PUT },
    { "DELETE", NGX_HTTP_DELETE },
    { "MKCOL", NGX_HTTP_MKCOL },
    { "COPY", NGX_HTTP_COPY },
    { "MOVE", NGX_HTTP_MOVE },
    { "OPTIONS", NGX_HTTP_OPTIONS },
    { "PROPFIND", NGX_HTTP_PROPFIND },
    { "PROPPATCH", NGX_HTTP_PROPPATCH },
    { "LOCK", NGX_HTTP_LOCK },
    { "UNLOCK", NGX_HTTP_UNLOCK },
    { "PATCH", NGX_HTTP_PATCH },
    { "TRACE", NGX_HTTP_TRACE },
    { NULL, 0 }
};


static SQRegFunction  ngx_squ_request_methods[] = {
    { NULL, NULL }
};


ngx_module_t  ngx_squ_http_request_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_request_module_init,           /* init module */
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
        &ngx_squ_http_request_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_request_headers_get(HSQUIRRELVM v)
{
    u_char               ch;
    SQRESULT             rc;
    ngx_str_t            key;
    ngx_uint_t           i, n;
    ngx_list_part_t     *part;
    ngx_table_elt_t     *header;
    ngx_squ_thread_t    *thr;
    ngx_squ_http_ctx_t  *ctx;

    thr = sq_getforeignptr(v);

    ctx = thr->module_ctx;

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    part = &ctx->r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */ ; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        for (n = 0; n < key.len && n < header[i].key.len; n++) {
            ch = header[i].key.data[n];

            if (ch >= 'A' && ch <= 'Z') {
                ch |= 0x20;

            } else if (ch == '-') {
                ch = '_';
            }

            if (key.data[n] != ch) {
                break;
            }
        }

        if (n == key.len && n == header[i].key.len) {
            sq_pushstring(v, (SQChar *) header[i].value.data,
                          header[i].value.len);
            return 1;
        }
    }

    sq_pushnull(v);

    return 1;
}


static SQInteger
ngx_squ_request_cookies_get(HSQUIRRELVM v)
{
    SQRESULT             rc;
    ngx_str_t            key, value;
    ngx_squ_thread_t    *thr;
    ngx_squ_http_ctx_t  *ctx;

    thr = sq_getforeignptr(v);

    ctx = thr->module_ctx;

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    if (ngx_http_parse_multi_header_lines(&ctx->r->headers_in.cookies, &key,
                                          &value)
        == NGX_DECLINED)
    {
        sq_pushnull(v);
        return 1;
    }

    sq_pushstring(v, (SQChar *) value.data, value.len);

    return 1;
}


static SQInteger
ngx_squ_request_get_get(HSQUIRRELVM v)
{
    SQRESULT             rc;
    ngx_str_t            key, value;
    ngx_squ_thread_t    *thr;
    ngx_squ_http_ctx_t  *ctx;

    thr = sq_getforeignptr(v);

    ctx = thr->module_ctx;

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    if (ngx_http_arg(ctx->r, key.data, key.len, &value) == NGX_DECLINED) {
        sq_pushnull(v);
        return 1;
    }

    sq_pushstring(v, (SQChar *) value.data, value.len);

    return 1;
}


static SQInteger
ngx_squ_request_post_get(HSQUIRRELVM v)
{
    u_char              *dst, *src;
    SQRESULT             rc;
    ngx_str_t            key, value;
    ngx_squ_thread_t    *thr;
    ngx_squ_http_ctx_t  *ctx;
    ngx_http_request_t  *r;

    thr = sq_getforeignptr(v);

    ctx = thr->module_ctx;
    r = ctx->r;

    if (r->request_body == NULL) {
        sq_pushnull(v);
        return 1;
    }

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    switch (key.len) {

    case 12:

        if (ngx_strncmp(key.data, "request_body", 12) == 0) {
            if (ngx_squ_request_copy_request_body(r, ctx) != NGX_OK) {
                sq_pushnull(v);
                return 1;
            }

            sq_pushstring(v, (SQChar *) ctx->req_body.data, ctx->req_body.len);
            return 1;
        }

        break;

    case 17:

        if (ngx_strncmp(key.data, "request_body_file", 17) == 0) {
            if (r->request_body->temp_file == NULL) {
                sq_pushnull(v);
                return 1;
            }

            sq_pushstring(v,
                          (SQChar *) r->request_body->temp_file->file.name.data,
                          r->request_body->temp_file->file.name.len);
            return 1;
        }

        break;

    default:
        break;
    }

    if (ngx_squ_request_copy_request_body(r, ctx) != NGX_OK) {
        sq_pushnull(v);
        return 1;
    }

    if (ngx_squ_request_get_posted_arg(&ctx->req_body, &key, &value) != NGX_OK)
    {
        sq_pushnull(v);
        return 1;
    }

    /* TODO: unescape uri */

    dst = ngx_pnalloc(r->pool, value.len);
    if (dst == NULL) {
        sq_pushnull(v);
        return 1;
    }

    src = value.data;

    value.data = dst;
    ngx_unescape_uri(&dst, &src, value.len, 0);
    value.len = dst - value.data;

    sq_pushstring(v, (SQChar *) value.data, value.len);

    return 1;
}


static SQInteger
ngx_squ_request_get(HSQUIRRELVM v)
{
    SQRESULT             rc;
    ngx_str_t            key, value;
    ngx_msec_int_t       ms;
    struct timeval       tv;
    ngx_squ_thread_t    *thr;
    ngx_squ_http_ctx_t  *ctx;
    ngx_http_request_t  *r;

    thr = sq_getforeignptr(v);

    ctx = thr->module_ctx;
    r = ctx->r;

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    switch (key.len) {

    case 3:

        if (ngx_strncmp(key.data, "uri", 3) == 0) {
            sq_pushstring(v, (SQChar *) r->uri.data, r->uri.len);
            return 1;
        }

        break;

    case 4:

        if (ngx_strncmp(key.data, "args", 4) == 0) {
            sq_pushstring(v, (SQChar *) r->args.data, r->args.len);
            return 1;
        }

        if (r->headers_in.host != NULL && ngx_strncmp(key.data, "host", 4) == 0)
        {
            sq_pushstring(v, (SQChar *) r->headers_in.host->value.data,
                          r->headers_in.host->value.len);
            return 1;
        }

        break;

    case 5:

        if (ngx_strncmp(key.data, "exten", 5) == 0) {
            sq_pushstring(v, (SQChar *) r->exten.data, r->exten.len);
            return 1;
        }

        break;

    case 6:

        if (ngx_strncmp(key.data, "method", 6) == 0) {
            sq_pushinteger(v, r->method);
            return 1;
        }

        break;

    case 7:

        if (r->headers_in.referer != NULL
            && ngx_strncmp(key.data, "referer", 7) == 0)
        {
            sq_pushstring(v, (SQChar *) r->headers_in.referer->value.data,
                          r->headers_in.referer->value.len);
            return 1;
        }

        break;

    case 10:

        if (r->headers_in.user_agent != NULL
            && ngx_strncmp(key.data, "user_agent", 10) == 0)
        {
            sq_pushstring(v, (SQChar *) r->headers_in.user_agent->value.data,
                          r->headers_in.user_agent->value.len);
            return 1;
        }

        break;

    case 11:

        if (ngx_strncmp(key.data, "method_name", 11) == 0) {
            sq_pushstring(v, (SQChar *) r->method_name.data,
                          r->method_name.len);
            return 1;
        }

        break;

    case 12:

        if (ngx_strncmp(key.data, "request_time", 12) == 0) {
            ngx_gettimeofday(&tv);
            ms = (ngx_msec_int_t) ((tv.tv_sec - r->start_sec) * 1000
                                   + (tv.tv_usec / 1000 - r->start_msec));
            ms = ngx_max(ms, 0);

            sq_pushinteger(v, ms);
            return 1;
        }

        if (ngx_strncmp(key.data, "request_line", 12) == 0) {
            sq_pushstring(v, (SQChar *) r->request_line.data,
                          r->request_line.len);
            return 1;
        }

        if (ngx_strncmp(key.data, "unparsed_uri", 12) == 0) {
            sq_pushstring(v, (SQChar *) r->unparsed_uri.data,
                          r->unparsed_uri.len);
            return 1;
        }

        break;

    case 13:

        if (ngx_strncmp(key.data, "http_protocol", 13) == 0) {
            sq_pushstring(v, (SQChar *) r->http_protocol.data,
                          r->http_protocol.len);
            return 1;
        }

        break;

    default:
        break;
    }

    if (ngx_http_arg(r, key.data, key.len, &value) == NGX_OK) {
        sq_pushstring(v, (SQChar *) value.data, value.len);
        return 1;
    }

    return ngx_squ_request_post_get(v);
}


static ngx_int_t
ngx_squ_request_copy_request_body(ngx_http_request_t *r,
    ngx_squ_http_ctx_t *ctx)
{
    u_char       *p;
    size_t        len;
    ngx_buf_t    *buf, *next;
    ngx_chain_t  *cl;

    if (ctx->req_body.len) {
        return NGX_OK;
    }

    if (r->request_body->bufs != NULL) {
        cl = r->request_body->bufs;
        buf = cl->buf;

        if (cl->next == NULL) {
            ctx->req_body.len = buf->last - buf->pos;
            ctx->req_body.data = buf->pos;

            return NGX_OK;
        }

        next = cl->next->buf;
        len = (buf->last - buf->pos) + (next->last - next->pos);

        p = ngx_pnalloc(r->pool, len);
        if (p == NULL) {
            return NGX_ERROR;
        }

        p = ngx_cpymem(p, buf->pos, buf->last - buf->pos);
        ngx_memcpy(p, next->pos, next->last - next->pos);

    } else if (r->request_body->temp_file != NULL) {

        /* TODO: reading request body from temp file */

        len = 0;
        p = NULL;

    } else {
        return NGX_DECLINED;
    }

    ctx->req_body.len = len;
    ctx->req_body.data = p;

    return NGX_OK;
}


static ngx_int_t
ngx_squ_request_get_posted_arg(ngx_str_t *posted, ngx_str_t *key,
    ngx_str_t *value)
{
    u_char  *p, *last;

    p = posted->data;
    last = p + posted->len;

    for ( /* void */ ; p < last; p++) {

        /* we need '=' after name, so drop one char from last */

        p = ngx_strlcasestrn(p, last - 1, key->data, key->len - 1);

        if (p == NULL) {
            return NGX_DECLINED;
        }

        if ((p == posted->data || *(p - 1) == '&') && *(p + key->len) == '=') {

            value->data = p + key->len + 1;

            p = ngx_strlchr(p, last, '&');

            if (p == NULL) {
                p = posted->data + posted->len;
            }

            value->len = p - value->data;

            return NGX_OK;
        }
    }

    return NGX_DECLINED;
}


static ngx_int_t
ngx_squ_request_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cycle->log, 0,
                   "squ request module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_HTTP_TABLE, sizeof(NGX_SQU_HTTP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_request_consts) / sizeof(ngx_squ_const_t) - 1;
    n += sizeof(ngx_squ_request_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "request", sizeof("request") - 1);
    sq_newtableex(scf->v, n + 4);

    for (n = 0; ngx_squ_request_consts[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_request_consts[n].name, -1);
        sq_pushinteger(scf->v, ngx_squ_request_consts[n].value);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    for (n = 0; ngx_squ_request_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_request_methods[n].name, -1);
        sq_newclosure(scf->v, ngx_squ_request_methods[n].f, 0);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    sq_pushstring(scf->v, "headers", sizeof("headers") - 1);
    sq_newtable(scf->v);
    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_request_headers_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pushstring(scf->v, "cookies", sizeof("cookies") - 1);
    sq_newtable(scf->v);
    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_request_cookies_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pushstring(scf->v, "get", sizeof("get") - 1);
    sq_newtable(scf->v);
    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_request_get_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pushstring(scf->v, "post", sizeof("post") - 1);
    sq_newtable(scf->v);
    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_request_post_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_request_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -3);

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
