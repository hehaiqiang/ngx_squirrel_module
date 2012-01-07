
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_http_module.h>


static SQInteger ngx_squ_http_session_create(HSQUIRRELVM v);
static SQInteger ngx_squ_http_session_destroy(HSQUIRRELVM v);
static SQInteger ngx_squ_http_session_set_param(HSQUIRRELVM v);
static SQInteger ngx_squ_http_session_get_param(HSQUIRRELVM v);
static SQInteger ngx_squ_http_session_get(HSQUIRRELVM v);
static SQInteger ngx_squ_http_session_set(HSQUIRRELVM v);

static ngx_int_t ngx_squ_http_session_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_http_session_consts[] = {
    { NULL, 0 }
};


static SQRegFunction  ngx_squ_http_session_methods[] = {
    { "create", ngx_squ_http_session_create },
    { "destroy", ngx_squ_http_session_destroy },
    { "set_param", ngx_squ_http_session_set_param },
    { "get_param", ngx_squ_http_session_get_param },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_http_session_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_http_session_module_init,      /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


#if (NGX_SQU_DLL)
extern ngx_module_t  ngx_squ_session_module;


ngx_module_t **
ngx_squ_get_modules(void)
{
    static ngx_module_t  *modules[] = {
        &ngx_squ_session_module,
        &ngx_squ_http_session_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_http_session_create(HSQUIRRELVM v)
{
#if 0
    ngx_pool_t           *pool;
    ngx_session_ctx_t    *ctx;
    ngx_http_request_t   *r;
    ngx_squ_main_conf_t  *lmcf;

    thr = ngx_squ_thread(l);

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_session_ctx_t));

    lmcf = ngx_http_get_module_main_conf(r, ngx_squ_http_module);

    if (lmcf->session_mode == NGX_SESSION_MODE_SINGLE) {
        ngx_session_create(&lmcf->session, ctx);
    }

    /* TODO */
#endif

    return 0;
}


static SQInteger
ngx_squ_http_session_destroy(HSQUIRRELVM v)
{
#if 0
    ngx_pool_t           *pool;
    ngx_session_ctx_t    *ctx;
    ngx_http_request_t   *r;
    ngx_squ_main_conf_t  *lmcf;

    thr = ngx_squ_thread(l);

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_session_ctx_t));

    lmcf = ngx_http_get_module_main_conf(r, ngx_squ_http_module);

    if (lmcf->session_mode == NGX_SESSION_MODE_SINGLE) {
        ngx_session_destroy(&lmcf->session, ctx);
    }

    /* TODO */
#endif

    return 0;
}


static SQInteger
ngx_squ_http_session_set_param(HSQUIRRELVM v)
{
#if 0
    ngx_pool_t           *pool;
    ngx_session_ctx_t    *ctx;
    ngx_http_request_t   *r;
    ngx_squ_main_conf_t  *lmcf;

    thr = ngx_squ_thread(l);

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_session_ctx_t));

    lmcf = ngx_http_get_module_main_conf(r, ngx_squ_http_module);

    if (lmcf->session_mode == NGX_SESSION_MODE_SINGLE) {
        ngx_session_set_param(&lmcf->session, ctx);
    }

    /* TODO */
#endif

    return 0;
}


static SQInteger
ngx_squ_http_session_get_param(HSQUIRRELVM v)
{
#if 0
    ngx_pool_t           *pool;
    ngx_session_ctx_t    *ctx;
    ngx_http_request_t   *r;
    ngx_squ_main_conf_t  *lmcf;

    thr = ngx_squ_thread(l);

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_session_ctx_t));

    lmcf = ngx_http_get_module_main_conf(r, ngx_squ_http_module);

    if (lmcf->session_mode == NGX_SESSION_MODE_SINGLE) {
        ngx_session_get_param(&lmcf->session, ctx);
    }

    /* TODO */
#endif

    return 0;
}


static SQInteger
ngx_squ_http_session_get(HSQUIRRELVM v)
{
#if 0
    ngx_pool_t           *pool;
    ngx_session_ctx_t    *ctx;
    ngx_http_request_t   *r;
    ngx_squ_main_conf_t  *lmcf;

    thr = ngx_squ_thread(l);

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_session_ctx_t));

    lmcf = ngx_http_get_module_main_conf(r, ngx_squ_http_module);

    if (lmcf->session_mode == NGX_SESSION_MODE_SINGLE) {
        ngx_session_get_var(&lmcf->session, ctx);
    }

    /* TODO */
#endif

    return 0;
}


static SQInteger
ngx_squ_http_session_set(HSQUIRRELVM v)
{
#if 0
    ngx_pool_t           *pool;
    ngx_squ_thread_t     *thr;
    ngx_session_ctx_t    *ctx;
    ngx_squ_main_conf_t  *lmcf;

    thr = ngx_squ_thread(l);

    pool = ngx_create_pool(ngx_pagesize, thr->log);
    if (pool == NULL) {
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_session_ctx_t));

    lmcf = ngx_http_get_module_main_conf(r, ngx_squ_http_module);

    if (lmcf->session_mode == NGX_SESSION_MODE_SINGLE) {
        ngx_session_set_var(&lmcf->session, ctx);

        ngx_session_del_var(&lmcf->session, ctx);
    }

    /* TODO */
#endif

    return 0;
}


static ngx_int_t
ngx_squ_http_session_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ http session module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);
    sq_pushstring(scf->v, NGX_SQU_HTTP_TABLE, sizeof(NGX_SQU_HTTP_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_http_session_consts) / sizeof(ngx_squ_const_t) - 1;
    n += sizeof(ngx_squ_http_session_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "session", sizeof("session") - 1);
    sq_newtableex(scf->v, n);

    for (n = 0; ngx_squ_http_session_consts[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_http_session_consts[n].name, -1);
        sq_pushinteger(scf->v, ngx_squ_http_session_consts[n].value);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    for (n = 0; ngx_squ_http_session_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_http_session_methods[n].name, -1);
        sq_newclosure(scf->v, ngx_squ_http_session_methods[n].f, 0);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    sq_newtableex(scf->v, 2);

    sq_pushstring(scf->v, "_get", sizeof("_get") - 1);
    sq_newclosure(scf->v, ngx_squ_http_session_get, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pushstring(scf->v, "_set", sizeof("_set") - 1);
    sq_newclosure(scf->v, ngx_squ_http_session_set, 0);
    rc = sq_newslot(scf->v, -3, SQFalse);

    rc = sq_setdelegate(scf->v, -2);

    rc = sq_newslot(scf->v,  -3, SQFalse);

    sq_pop(scf->v, 3);

    return NGX_OK;
}
