
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ.h>


static SQInteger ngx_squ_logger_error(HSQUIRRELVM v);
static SQInteger ngx_squ_logger_debug(HSQUIRRELVM v);

static ngx_int_t ngx_squ_logger_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_logger_consts[] = {
    { "STDERR", NGX_LOG_STDERR },
    { "EMERG", NGX_LOG_EMERG },
    { "ALERT", NGX_LOG_ALERT },
    { "CRIT", NGX_LOG_CRIT },
    { "ERR", NGX_LOG_ERR },
    { "WARN", NGX_LOG_WARN },
    { "NOTICE", NGX_LOG_NOTICE },
    { "INFO", NGX_LOG_INFO },
    { "DEBUG", NGX_LOG_DEBUG },

    { "DEBUG_CORE", NGX_LOG_DEBUG_CORE },
    { "DEBUG_ALLOC", NGX_LOG_DEBUG_ALLOC },
    { "DEBUG_MUTEX", NGX_LOG_DEBUG_MUTEX },
    { "DEBUG_EVENT", NGX_LOG_DEBUG_EVENT },
    { "DEBUG_HTTP", NGX_LOG_DEBUG_HTTP },
    { "DEBUG_MAIL", NGX_LOG_DEBUG_MAIL },
    { "DEBUG_MYSQL", NGX_LOG_DEBUG_MYSQL },

    { NULL, 0 }
};


static SQRegFunction  ngx_squ_logger_methods[] = {
    { "error", ngx_squ_logger_error },
    { "debug", ngx_squ_logger_debug },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_logger_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_logger_module_init,            /* init module */
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
        &ngx_squ_logger_module,
        NULL
    };

    return modules;
}
#endif


static SQInteger
ngx_squ_logger_error(HSQUIRRELVM v)
{
    SQChar            *str;
    SQRESULT           rc;
    SQInteger          level;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    rc = sq_getinteger(v, 2, &level);

    sq_tostring(v, 3);
    rc = sq_getstring(v, 4, &str);

    ngx_log_error((ngx_uint_t) level, thr->log, 0, str);

    sq_poptop(v);

    return 0;
}


static SQInteger
ngx_squ_logger_debug(HSQUIRRELVM v)
{
    SQChar            *str;
    SQRESULT           rc;
    SQInteger          level;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    rc = sq_getinteger(v, 2, &level);

    sq_tostring(v, 3);
    rc = sq_getstring(v, 4, &str);

    ngx_log_debug0(level, thr->log, 0, str);

    sq_poptop(v);

    return 0;
}


static ngx_int_t
ngx_squ_logger_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ logger module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);
    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_logger_consts) / sizeof(ngx_squ_const_t) - 1;
    n += sizeof(ngx_squ_logger_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "logger", sizeof("logger") - 1);
    sq_newtableex(scf->v, n);

    for (n = 0; ngx_squ_logger_consts[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_logger_consts[n].name, -1);
        sq_pushinteger(scf->v, ngx_squ_logger_consts[n].value);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    for (n = 0; ngx_squ_logger_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_logger_methods[n].name, -1);
        sq_newclosure(scf->v, ngx_squ_logger_methods[n].f, 0);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 2);

    return NGX_OK;
}
