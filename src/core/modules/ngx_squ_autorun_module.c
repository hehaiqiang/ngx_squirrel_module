
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ.h>


typedef struct {
    ngx_uint_t            running;
} ngx_squ_autorun_t;


typedef struct {
    ngx_squ_script_t      script;
    ngx_str_t             name;
    size_t                size;
    ngx_squ_autorun_t    *autorun;
    ngx_slab_pool_t      *pool;
    ngx_shm_zone_t       *zone;
    ngx_squ_thread_t     *thr;
} ngx_squ_autorun_conf_t;


static ngx_int_t ngx_squ_autorun_init(ngx_shm_zone_t *shm_zone, void *data);

static ngx_int_t ngx_squ_autorun_process_init(ngx_cycle_t *cycle);
static void ngx_squ_autorun_process_exit(ngx_cycle_t *cycle);
static void *ngx_squ_autorun_create_conf(ngx_cycle_t *cycle);
static char *ngx_squ_autorun_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_squ_autorun(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_squ_autorun_commands[] = {

    { ngx_string("squ_autorun"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE2,
      ngx_squ_autorun,
      0,
      0,
      NULL },

    { ngx_string("squ_autorun_script_code"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_squ_set_script_slot,
      0,
      offsetof(ngx_squ_autorun_conf_t, script),
      NULL },

    { ngx_string("squ_autorun_script_file"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
      ngx_squ_set_script_slot,
      0,
      offsetof(ngx_squ_autorun_conf_t, script),
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_squ_autorun_module_ctx = {
    ngx_string("autorun"),
    ngx_squ_autorun_create_conf,
    ngx_squ_autorun_init_conf,
};


ngx_module_t  ngx_squ_autorun_module = {
    NGX_MODULE_V1,
    &ngx_squ_autorun_module_ctx,           /* module context */
    ngx_squ_autorun_commands,              /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_squ_autorun_process_init,          /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_squ_autorun_process_exit,          /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


#if (NGX_SQU_DLL)
ngx_module_t **
ngx_squ_get_modules(void)
{
    static ngx_module_t  *modules[] = {
        &ngx_squ_autorun_module,
        NULL
    };

    return modules;
}
#endif


static ngx_int_t
ngx_squ_autorun_output(ngx_squ_thread_t *thr, u_char *buf, size_t size)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ autorun output");

    /* TODO */

    return NGX_OK;
}


static void
ngx_squ_autorun_finalize(ngx_squ_thread_t *thr, ngx_int_t rc)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ autorun finalize");

    /* TODO */
}


static ngx_int_t
ngx_squ_autorun_init(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_squ_autorun_conf_t *olacf = data;

    size_t                   len;
    ngx_squ_autorun_conf_t  *lacf;

    lacf = shm_zone->data;

    if (olacf) {
        lacf->autorun = olacf->autorun;
        lacf->pool = olacf->pool;
        return NGX_OK;
    }

    lacf->pool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        lacf->autorun = lacf->pool->data;
        return NGX_OK;
    }

    lacf->autorun = ngx_slab_alloc(lacf->pool, sizeof(ngx_squ_autorun_t));
    if (lacf->autorun == NULL) {
        return NGX_ERROR;
    }

    lacf->pool->data = lacf->autorun;

    lacf->autorun->running = 0;

    len = sizeof(" in squ autorun \"\"") + shm_zone->shm.name.len;

    lacf->pool->log_ctx = ngx_slab_alloc(lacf->pool, len);
    if (lacf->pool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(lacf->pool->log_ctx, " in squ autorun \"%V\"%Z",
                &shm_zone->shm.name);

    return NGX_OK;
}


static ngx_int_t
ngx_squ_autorun_process_init(ngx_cycle_t *cycle)
{
    ngx_pool_t              *pool;
    ngx_squ_conf_t          *scf;
    ngx_squ_thread_t        *thr;
    ngx_squ_autorun_conf_t  *lacf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ autorun process init");

    lacf = ngx_squ_get_conf(cycle, ngx_squ_autorun_module);

    if (lacf->script.from == NGX_CONF_UNSET_UINT) {
        return NGX_OK;
    }

    ngx_shmtx_lock(&lacf->pool->mutex);

    if (lacf->autorun->running) {
        ngx_shmtx_unlock(&lacf->pool->mutex);
        return NGX_OK;
    }

    lacf->autorun->running = 1;

    ngx_shmtx_unlock(&lacf->pool->mutex);

    pool = ngx_create_pool(ngx_pagesize, cycle->log);
    if (pool == NULL) {
        return NGX_ERROR;
    }

    thr = ngx_pcalloc(pool, sizeof(ngx_squ_thread_t));
    if (thr == NULL) {
        ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    /* TODO: thr->module_ctx and thr->c */

    thr->pool = pool;
    thr->log = cycle->log;
    thr->aio = 1;
    thr->file.fd = NGX_INVALID_FILE;
    thr->conf = scf->conf;
    thr->module_ctx = NULL;
    thr->c = NULL;
    thr->script = &lacf->script;
    thr->output = ngx_squ_autorun_output;
    thr->finalize = ngx_squ_autorun_finalize;

    thr->path = lacf->script.path;

    if (ngx_squ_check_script(thr) == NGX_ERROR) {
        ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    lacf->thr = thr;

    ngx_squ_load_script(thr);

    return NGX_OK;
}


static void
ngx_squ_autorun_process_exit(ngx_cycle_t *cycle)
{
    ngx_squ_autorun_conf_t  *lacf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ autorun process exit");

    lacf = ngx_squ_get_conf(cycle, ngx_squ_autorun_module);

    if (lacf->script.from == NGX_CONF_UNSET_UINT) {
        return;
    }

    if (lacf->thr != NULL) {
        ngx_squ_thread_destroy(lacf->thr, 1);

        ngx_destroy_pool(lacf->thr->pool);
    }
}


static void *
ngx_squ_autorun_create_conf(ngx_cycle_t *cycle)
{
    ngx_squ_autorun_conf_t  *lacf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0,
                   "squ autorun create conf");

    lacf = ngx_pcalloc(cycle->pool, sizeof(ngx_squ_autorun_conf_t));
    if (lacf == NULL) {
        return NULL;
    }

    lacf->script.from = NGX_CONF_UNSET_UINT;
    lacf->size = NGX_CONF_UNSET_SIZE;

    return lacf;
}


static char *
ngx_squ_autorun_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_squ_autorun_conf_t *lacf = conf;

    ngx_str_t  name;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ autorun init conf");

    if (lacf->script.from != NGX_CONF_UNSET_UINT) {
        if (lacf->name.data == NULL) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, 0,
                          "the directive \"squ_autorun\" must be specified");
            return NGX_CONF_ERROR;
        }
    }

    ngx_str_set(&name, "default");
    lacf->script.parser = ngx_squ_parser_find(cycle->log, &name);

    return NGX_CONF_OK;
}


static char *
ngx_squ_autorun(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_squ_autorun_conf_t *lacf = conf;

    ngx_str_t   *value, str;
    ngx_uint_t   i;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cf->log, 0, "squ autorun");

    if (lacf->name.data != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "name=", 5) == 0) {
            lacf->name.len = value[i].len - 5;
            lacf->name.data = value[i].data + 5;
            continue;
        }

        if (ngx_strncmp(value[i].data, "size=", 5) == 0) {
            str.len = value[i].len - 5;
            str.data = value[i].data + 5;
            lacf->size = ngx_parse_size(&str);
            if (lacf->size == (size_t) NGX_ERROR) {
                goto invalid;
            }
            continue;
        }

        goto invalid;
    }

    if (lacf->name.data == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the directive \"squ_autorun\" must be specified");
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_size_value(lacf->size, 1024 * 1024 * 1);

    lacf->zone = ngx_shared_memory_add(cf, &lacf->name, lacf->size,
                                       &ngx_squ_autorun_module);
    if (lacf->zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (lacf->zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate squ autorun name \"%V\"", &lacf->name);
        return NGX_CONF_ERROR;
    }

    lacf->zone->init = ngx_squ_autorun_init;
    lacf->zone->data = lacf;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\" in squ_autorun", &value[i]);

    return NGX_CONF_ERROR;
}
