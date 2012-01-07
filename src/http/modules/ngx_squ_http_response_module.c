
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_http_module.h>


static SQInteger ngx_squ_response_write(HSQUIRRELVM v);
static SQInteger ngx_squ_response_headers_set(HSQUIRRELVM v);
static SQInteger ngx_squ_response_cookies_set(HSQUIRRELVM v);
static SQInteger ngx_squ_response_set(HSQUIRRELVM v);

static ngx_int_t ngx_squ_response_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_response_consts[] = {
    { "OK", NGX_HTTP_OK },
    { "CREATED", NGX_HTTP_CREATED },
    { "ACCEPTED", NGX_HTTP_ACCEPTED },
    { "NO_CONTENT", NGX_HTTP_NO_CONTENT },
    { "PARTIAL_CONTENT", NGX_HTTP_PARTIAL_CONTENT },

    { "SPECIAL_RESPONSE", NGX_HTTP_PARTIAL_CONTENT },
    { "MOVED_PERMANENTLY", NGX_HTTP_MOVED_PERMANENTLY },
    { "MOVED_TEMPORARILY", NGX_HTTP_MOVED_TEMPORARILY },
    { "SEE_OTHER", NGX_HTTP_SEE_OTHER },
    { "NOT_MODIFIED", NGX_HTTP_NOT_MODIFIED },

    { "BAD_REQUEST", NGX_HTTP_BAD_REQUEST },
    { "UNAUTHORIZED", NGX_HTTP_UNAUTHORIZED },
    { "FORBIDDEN", NGX_HTTP_FORBIDDEN },
    { "NOT_FOUND", NGX_HTTP_NOT_FOUND },
    { "NOT_ALLOWED", NGX_HTTP_NOT_ALLOWED },
    { "REQUEST_TIME_OUT", NGX_HTTP_REQUEST_TIME_OUT },
    { "CONFLICT", NGX_HTTP_CONFLICT },
    { "LENGTH_REQUIRED", NGX_HTTP_LENGTH_REQUIRED },
    { "PRECONDITION_FAILED", NGX_HTTP_PRECONDITION_FAILED },
    { "REQUEST_ENTITY_TOO_LARGE", NGX_HTTP_REQUEST_ENTITY_TOO_LARGE },
    { "REQUEST_URI_TOO_LARGE", NGX_HTTP_REQUEST_URI_TOO_LARGE },
    { "UNSUPPORTED_MEDIA_TYPE", NGX_HTTP_UNSUPPORTED_MEDIA_TYPE },
    { "RANGE_NOT_SATISFIABLE", NGX_HTTP_RANGE_NOT_SATISFIABLE },
    { "CLOSE", NGX_HTTP_CLOSE },
    { "REQUEST_HEADER_TOO_LARGE", NGX_HTTP_REQUEST_HEADER_TOO_LARGE },
    { "HTTPS_CERT_ERROR", NGX_HTTPS_CERT_ERROR },
    { "HTTPS_NO_CERT", NGX_HTTPS_NO_CERT },
    { "HTTP_TO_HTTPS", NGX_HTTP_TO_HTTPS },
    { "CLIENT_CLOSED_REQUEST", NGX_HTTP_CLIENT_CLOSED_REQUEST },

    { "INTERNAL_SERVER_ERROR", NGX_HTTP_INTERNAL_SERVER_ERROR },
    { "NOT_IMPLEMENTED", NGX_HTTP_NOT_IMPLEMENTED },
    { "BAD_GATEWAY", NGX_HTTP_BAD_GATEWAY },
    { "SERVICE_UNAVAILABLE", NGX_HTTP_SERVICE_UNAVAILABLE },
    { "GATEWAY_TIME_OUT", NGX_HTTP_GATEWAY_TIME_OUT },
    { "INSUFFICIENT_STORAGE", NGX_HTTP_INSUFFICIENT_STORAGE },

    { NULL, 0 }
};


static SQRegFunction  ngx_squ_response_methods[] = {
    { "write", ngx_squ_response_write },
#if 0
    sendfile
#endif
    { NULL, NULL }
};


ngx_module_t  ngx_squ_http_response_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_response_module_init,          /* init module */
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
        &ngx_squ_http_response_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_response_write(HSQUIRRELVM v)
{
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    rc = sq_getstring(v, 2, (SQChar **) &str.data);
    str.len = ngx_strlen(str.data);

    ngx_squ_output(thr, str.data, str.len);

    return 0;
}


static SQInteger
ngx_squ_response_headers_set(HSQUIRRELVM v)
{
    /* TODO: */
    return 0;
}


static SQInteger
ngx_squ_response_cookies_set(HSQUIRRELVM v)
{
    /* TODO: */
    return 0;
}


static SQInteger
ngx_squ_response_set(HSQUIRRELVM v)
{
    SQRESULT             rc;
    ngx_str_t            key, value, str;
    ngx_squ_thread_t    *thr;
    ngx_squ_http_ctx_t  *ctx;

    thr = sq_getforeignptr(v);

    rc = sq_getstring(v, 2, (SQChar **) &key.data);
    key.len = ngx_strlen(key.data);

    rc = sq_getstring(v, 3, (SQChar **) &value.data);
    value.len = ngx_strlen(value.data);

    str.len = value.len;
    str.data = ngx_pstrdup(thr->pool, &value);

    /* TODO: r->headers_out.status */

    ctx = thr->module_ctx;

    switch (key.len) {

    case 12:

        if (ngx_strncmp(key.data, "content_type", 12) == 0) {
            ctx->r->headers_out.content_type.len = str.len;
            ctx->r->headers_out.content_type.data = str.data;
        }

        break;

    default:
        break;
    }

    return 0;
}


static ngx_int_t
ngx_squ_response_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ response module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_HTTP_TABLE, sizeof(NGX_SQU_HTTP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_response_consts) / sizeof(ngx_squ_const_t) - 1;
    n += sizeof(ngx_squ_response_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "response", sizeof("response") - 1);
    sq_newtableex(scf->v, n + 2);

    for (n = 0; ngx_squ_response_consts[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_response_consts[n].name, -1);
        sq_pushinteger(scf->v, ngx_squ_response_consts[n].value);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    for (n = 0; ngx_squ_response_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_response_methods[n].name, -1);
        sq_newclosure(scf->v, ngx_squ_response_methods[n].f, 0);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    sq_pushstring(scf->v, "headers", sizeof("headers") - 1);
    sq_newtable(scf->v);
    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_set", sizeof("_set") - 1);
    sq_newclosure(scf->v, ngx_squ_response_headers_set, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pushstring(scf->v, "cookies", sizeof("cookies") - 1);
    sq_newtable(scf->v);
    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_set", sizeof("_set") - 1);
    sq_newclosure(scf->v, ngx_squ_response_cookies_set, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_set", sizeof("_set") - 1);
    sq_newclosure(scf->v, ngx_squ_response_set, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -3);

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
