
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_http_module.h>


static SQInteger ngx_squ_http_variable_get(HSQUIRRELVM v);

static ngx_int_t ngx_squ_http_variable_module_init(ngx_cycle_t *cycle);


ngx_module_t  ngx_squ_http_variable_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_http_variable_module_init,     /* init module */
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
        &ngx_squ_http_variable_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_http_variable_get(HSQUIRRELVM v)
{
    u_char                     *p;
    SQRESULT                    rc;
    ngx_str_t                   name;
    ngx_uint_t                  key, status;
    ngx_squ_thread_t           *thr;
    ngx_squ_http_ctx_t         *ctx;
    ngx_http_request_t         *r;
    ngx_http_variable_value_t  *vv;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, thr->log, 0, "squ http variable get");

    ctx = thr->module_ctx;
    r = ctx->r;

    rc = sq_getstring(v, 2, (SQChar **) &p);
    name.len = ngx_strlen(p);

    name.data = ngx_palloc(thr->pool, name.len);
    if (name.data == NULL) {
        sq_pushnull(v);
        sq_throwobject(v);
        return 0;
    }

    key = ngx_hash_strlow(name.data, p, name.len);

    vv = ngx_http_get_variable(ctx->r, &name, key);
    if (vv != NULL && !vv->not_found) {
        sq_pushstring(v, (SQChar *) vv->data, vv->len);
        return 1;
    }

    switch (name.len) {

    case 6:

        if (ngx_strncmp(name.data, "status", 6) == 0) {
            if (r->err_status) {
                status = r->err_status;

            } else if (r->headers_out.status) {
                status = r->headers_out.status;

            } else if (r->http_version == NGX_HTTP_VERSION_9) {
                sq_pushstring(v, "009", -1);
                return 1;

            } else {
                status = 0;
            }

            sq_pushinteger(v, status);
            return 1;
        }

        break;

    case 10:

        if (ngx_strncmp(name.data, "time_local", 10) == 0) {
            sq_pushstring(v, (SQChar *) ngx_cached_http_log_time.data,
                          ngx_cached_http_log_time.len);
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
ngx_squ_http_variable_module_init(ngx_cycle_t *cycle)
{
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ http variable module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_HTTP_TABLE, sizeof(NGX_SQU_HTTP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    sq_pushstring(scf->v, "variable", sizeof("variable") - 1);
    sq_newtableex(scf->v, 1);

    sq_newtableex(scf->v, 1);
    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_http_variable_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);
    rc = sq_setdelegate(scf->v, -2);

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
