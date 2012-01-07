
/*
 * Copyright (C) Ngwsx
 */


#ifndef _NGX_SQU_H_INCLUDED_
#define _NGX_SQU_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <squirrel.h>
#include <sqstdio.h>
#include <sqstdaux.h>


#define NGX_SQU_TABLE  "nginx"


#define NGX_SQU_SCRIPT_FROM_CONF  1
#define NGX_SQU_SCRIPT_FROM_FILE  2


typedef struct {
    HSQUIRRELVM              v;
    void                   **handle;
    void                   **conf;
} ngx_squ_conf_t;


typedef struct {
    char                    *name;
    int                      value;
} ngx_squ_const_t;


typedef struct ngx_squ_thread_s  ngx_squ_thread_t;

typedef ngx_int_t (*ngx_squ_parser_pt)(ngx_squ_thread_t *thr);
typedef ngx_int_t (*ngx_squ_output_pt)(ngx_squ_thread_t *thr, u_char *buf,
    size_t size);
typedef void (*ngx_squ_finalize_pt)(ngx_squ_thread_t *thr, ngx_int_t rc);


typedef struct {
    ngx_str_t                name;
    ngx_squ_parser_pt        parser;
} ngx_squ_parser_t;


typedef struct {
    ngx_uint_t               from;
    ngx_str_t                path;
    ngx_str_t                code;
    ngx_squ_parser_pt        parser;
} ngx_squ_script_t;


struct ngx_squ_thread_s {
    void                   **conf;
    ngx_squ_script_t        *script;
    ngx_squ_output_pt        output;
    ngx_squ_finalize_pt      finalize;

    ngx_pool_t              *pool;
    ngx_log_t               *log;
    ngx_connection_t        *c;

    ngx_flag_t               aio;
    ngx_file_t               file;
    ngx_str_t                path;
    size_t                   size;
    time_t                   mtime;

    HSQUIRRELVM              v;
    HSQOBJECT                obj;
    int                      ref;
    ngx_buf_t               *ssp;
    ngx_buf_t               *buf;
    void                    *module_ctx;
    void                   **ctx;

    unsigned                 cached:1;
};


ngx_int_t ngx_squ_create(ngx_cycle_t *cycle, ngx_squ_conf_t *scf);
void ngx_squ_destroy(void *data);
ngx_int_t ngx_squ_thread_create(ngx_squ_thread_t *thr);
void ngx_squ_thread_destroy(ngx_squ_thread_t *thr, ngx_uint_t force);
ngx_int_t ngx_squ_thread_run(ngx_squ_thread_t *thr, int n);
ngx_int_t ngx_squ_check_script(ngx_squ_thread_t *thr);
void ngx_squ_load_script(ngx_squ_thread_t *thr);

ngx_int_t ngx_squ_cache_get(ngx_squ_thread_t *thr);
ngx_int_t ngx_squ_cache_set(ngx_squ_thread_t *thr);

ngx_int_t ngx_squ_debug_start(ngx_squ_thread_t *thr);
ngx_int_t ngx_squ_debug_stop(ngx_squ_thread_t *thr);

ngx_squ_parser_pt ngx_squ_parser_find(ngx_log_t *log, ngx_str_t *name);

char *ngx_squ_set_script_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
char *ngx_squ_set_script_parser_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


#define ngx_squ_output(thr, buf, size)  thr->output(thr, buf, size)
#define ngx_squ_finalize(thr, rc)       thr->finalize(thr, rc)


#define ngx_squ_get_conf(cycle, module)                                       \
    ((ngx_squ_conf_t *)                                                       \
     ngx_get_conf(cycle->conf_ctx, ngx_squ_module))->conf[module.index]

#define ngx_squ_thread_get_conf(thr, module)  (thr)->conf[module.index]

#define ngx_squ_thread_get_module_ctx(thr, module)  (thr)->ctx[module.index]
#define ngx_squ_thread_set_ctx(thr, ctx, module)    thr->ctx[module.index] = ctx


#if !(NGX_WIN32)

#include <dlfcn.h>

#define ngx_squ_dlopen(name)        dlopen(name, RTLD_LAZY)
#define ngx_squ_dlopen_n            "dlopen()"

#define ngx_squ_dlclose(handle)     dlclose(handle)

#define ngx_squ_dlsym(handle, sym)  dlsym(handle, sym)
#define ngx_squ_dlsym_n             "dlsym()"

#define ngx_squ_dlerror()           dlerror()

#else

#define ngx_squ_dlopen(name)        LoadLibrary(name)
#define ngx_squ_dlopen_n            "LoadLibrary()"

#define ngx_squ_dlclose(handle)     FreeLibrary(handle)

#define ngx_squ_dlsym(handle, sym)  GetProcAddress(handle, sym)
#define ngx_squ_dlsym_n             "GetProcAddress()"

#define ngx_squ_dlerror()           ""

#endif


extern ngx_dll ngx_module_t   ngx_squ_module;
extern ngx_module_t          *ngx_squ_modules[];
extern ngx_uint_t             ngx_squ_max_module;


#endif /* _NGX_SQU_H_INCLUDED_ */
