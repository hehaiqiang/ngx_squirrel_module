
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ.h>


#define NGX_SQU_FILE  "ngx_squ_file_ctx_t*"


typedef struct ngx_squ_file_cleanup_ctx_s  ngx_squ_file_cleanup_ctx_t;


typedef struct {
    ngx_pool_t                    *pool;
    ngx_file_t                     file;
    off_t                          offset;
    size_t                         size;
    ngx_buf_t                     *in;
    ngx_buf_t                     *out;
    ngx_squ_thread_t              *thr;
    ngx_squ_file_cleanup_ctx_t    *cln_ctx;
} ngx_squ_file_ctx_t;


struct ngx_squ_file_cleanup_ctx_s {
    ngx_squ_file_ctx_t            *ctx;
};


static int ngx_squ_file_open(squ_State *l);
static int ngx_squ_file_close(squ_State *l);
static int ngx_squ_file_read(squ_State *l);
static int ngx_squ_file_write(squ_State *l);
#if 0
static int ngx_squ_file_index(squ_State *l);
#endif
static int ngx_squ_file_gc(squ_State *l);
static int ngx_squ_file_info(squ_State *l);

static ngx_inline ngx_squ_file_ctx_t *ngx_squ_file(lua_State *l);
static void ngx_squ_file_cleanup(void *data);

static ngx_int_t ngx_squ_file_aio_read(ngx_squ_thread_t *thr,
    ngx_squ_file_ctx_t *ctx);
#if (NGX_HAVE_FILE_AIO)
static void ngx_squ_file_aio_read_handler(ngx_event_t *ev);
#endif
static ngx_int_t ngx_squ_file_aio_write(ngx_squ_thread_t *thr,
    ngx_squ_file_ctx_t *ctx);
#if (NGX_HAVE_FILE_AIO)
static void ngx_squ_file_aio_write_handler(ngx_event_t *ev);
#endif

static ngx_int_t ngx_squ_file_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_file_consts[] = {
    { "RDONLY", NGX_FILE_RDONLY },
    { "WRONLY", NGX_FILE_WRONLY },
    { "RDWR", NGX_FILE_RDWR },
    { "APPEND", NGX_FILE_APPEND },

    { "CREATE_OR_OPEN", NGX_FILE_CREATE_OR_OPEN },
    { "OPEN", NGX_FILE_OPEN },
    { "TRUNCATE", NGX_FILE_TRUNCATE },

    { "DEFAULT_ACCESS", NGX_FILE_DEFAULT_ACCESS },
    { "OWNER_ACCESS", NGX_FILE_OWNER_ACCESS },

    { NULL, 0 }
};


static squL_Reg  ngx_squ_file_methods[] = {
    { "close", ngx_squ_file_close },
    { "read", ngx_squ_file_read },
    { "write", ngx_squ_file_write },
#if 0
    { "__index", ngx_squ_file_index },
#endif
    { "__gc", ngx_squ_file_gc },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_file_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_file_module_init,              /* init module */
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
        &ngx_squ_file_module,
        NULL
    };

    return modules;
}
#endif


static int
ngx_squ_file_open(squ_State *l)
{
    int                           mode, create, access;
    char                         *errstr;
    ngx_str_t                     name;
    ngx_pool_t                   *pool;
    ngx_file_t                   *file;
    ngx_squ_thread_t             *thr;
    ngx_pool_cleanup_t           *cln;
    ngx_squ_file_ctx_t          **ctx;
    ngx_squ_file_cleanup_ctx_t   *cln_ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file open");

    name.data = (u_char *) squL_checklstring(l, 1, &name.len);
    mode = (int) squL_optnumber(l, 2, NGX_FILE_RDWR);
    create = (int) squL_optnumber(l, 3, NGX_FILE_CREATE_OR_OPEN);
    access = (int) squL_optnumber(l, 4, NGX_FILE_DEFAULT_ACCESS);

    mode |= NGX_FILE_NONBLOCK;

#if (NGX_WIN32 && NGX_HAVE_FILE_AIO)
    if (ngx_event_flags & NGX_USE_IOCP_EVENT) {
        mode |= NGX_FILE_OVERLAPPED;
    }
#endif

    ctx = squ_newuserdata(l, sizeof(ngx_squ_file_ctx_t *));
    squL_getmetatable(l, NGX_SQU_FILE);
    squ_setmetatable(l, -2);

    *ctx = NULL;

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
        errstr = "ngx_create_pool() failed";
        goto error;
    }

    *ctx = ngx_pcalloc(pool, sizeof(ngx_squ_file_ctx_t));
    if (*ctx == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pcalloc() failed";
        goto error;
    }

    (*ctx)->pool = pool;
    (*ctx)->file.fd = NGX_INVALID_FILE;

    cln_ctx = ngx_pcalloc(thr->pool, sizeof(ngx_squ_file_cleanup_ctx_t));
    if (cln_ctx == NULL) {
        errstr = "ngx_pcalloc() failed";
        goto error;
    }

    cln_ctx->ctx = (*ctx);

    cln = ngx_pool_cleanup_add(thr->pool, 0);
    if (cln == NULL) {
        errstr = "ngx_pool_cleanup_add() failed";
        goto error;
    }

    cln->handler = ngx_squ_file_cleanup;
    cln->data = cln_ctx;

    (*ctx)->thr = thr;
    (*ctx)->cln_ctx = cln_ctx;

    file = &(*ctx)->file;

    file->name.data = ngx_pstrdup(pool, &name);
    if (file->name.data == NULL) {
        errstr = "ngx_pstrdup() failed";
        goto error;
    }

    file->name.len = name.len;
    file->log = ngx_cycle->log;

    file->fd = ngx_open_file(name.data, mode, create, access);
    if (file->fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      ngx_open_file_n " \"%V\" failed", &name);
        errstr = ngx_open_file_n " failed";
        goto error;
    }

    return 1;

error:

    squ_pop(l, 1);
    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return 2;
}


static int
ngx_squ_file_close(squ_State *l)
{
    ngx_squ_thread_t    *thr;
    ngx_squ_file_ctx_t  *ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file close");

    ctx = ngx_squ_file(l);

    if (ctx->file.fd != NGX_INVALID_FILE) {
        ngx_close_file(ctx->file.fd);
        ctx->file.fd = NGX_INVALID_FILE;
    }

    return 0;
}


static int
ngx_squ_file_read(squ_State *l)
{
    char                *errstr;
    size_t               size;
    ngx_int_t            rc;
    ngx_buf_t           *b;
    ngx_file_t          *file;
    ngx_file_info_t      fi;
    ngx_squ_thread_t    *thr;
    ngx_squ_file_ctx_t  *ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file read");

    ctx = ngx_squ_file(l);

    file = &ctx->file;

    if (file->fd == NGX_INVALID_FILE) {
        errstr = "invalid fd";
        goto error;
    }

    ngx_memzero(&fi, sizeof(ngx_file_info_t));

    if (ngx_fd_info(file->fd, &fi) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      ngx_fd_info_n " \"%V\" failed", &file->name);
        errstr = ngx_fd_info_n " failed";
        goto error;
    }

    size = (size_t) ngx_file_size(&fi);

    ctx->offset = squL_optint(l, 2, (squ_Integer) ctx->offset);
    ctx->size = squL_optint(l, 3, (squ_Integer) (size - ctx->offset));

    if (ctx->offset < 0 || ctx->offset >= size
        || ctx->size <= 0 || ctx->size > size - ctx->offset)
    {
        errstr = "invalid offset or size of the file or the file is empty";
        goto error;
    }

    b = ctx->in;

    if (b == NULL || (size_t) (b->end - b->start) < ctx->size) {
        if (b != NULL && (size_t) (b->end - b->start) > ctx->pool->max) {
            ngx_pfree(ctx->pool, b->start);
        }

        size = ngx_max(ngx_pagesize, ctx->size);

        b = ngx_create_temp_buf(ctx->pool, size);
        if (b == NULL) {
            errstr = "ngx_create_temp_buf() failed";
            goto error;
        }

        ctx->in = b;
    }

    rc = ngx_squ_file_aio_read(thr, ctx);
    if (rc == NGX_AGAIN) {
        return squ_yield(l, 0);
    }

    return rc;

error:

    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return 2;
}


static int
ngx_squ_file_write(squ_State *l)
{
    char                *errstr;
    size_t               size;
    u_char              *str;
    ngx_int_t            rc;
    ngx_buf_t           *b;
    ngx_file_t          *file;
    ngx_file_info_t      fi;
    ngx_squ_thread_t    *thr;
    ngx_squ_file_ctx_t  *ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file write");

    ctx = ngx_squ_file(l);

    file = &ctx->file;

    if (file->fd == NGX_INVALID_FILE) {
        errstr = "invalid fd";
        goto error;
    }

    ngx_memzero(&fi, sizeof(ngx_file_info_t));

    if (ngx_fd_info(file->fd, &fi) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      ngx_fd_info_n " \"%V\" failed", &file->name);
        errstr = ngx_fd_info_n " failed";
        goto error;
    }

    size = (size_t) ngx_file_size(&fi);

    str = (u_char *) squL_checklstring(l, 2, &ctx->size);
    ctx->offset = squL_optint(l, 3, (squ_Integer) ctx->offset);

    if (ctx->offset < 0 || ctx->offset > size) {
        errstr = "invalid offset of the file";
        goto error;
    }

    b = ctx->out;

    if (b == NULL || (size_t) (b->end - b->start) < ctx->size) {
        if (b != NULL && (size_t) (b->end - b->start) > ctx->pool->max) {
            ngx_pfree(ctx->pool, b->start);
        }

        size = ngx_max(ngx_pagesize, ctx->size);

        b = ngx_create_temp_buf(ctx->pool, size);
        if (b == NULL) {
            errstr = "ngx_create_temp_buf() failed";
            goto error;
        }

        ctx->out = b;
    }

    ngx_memcpy(b->start, str, ctx->size);

    rc = ngx_squ_file_aio_write(thr, ctx);
    if (rc == NGX_AGAIN) {
        return squ_yield(l, 0);
    }

    return rc;

error:

    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return 2;
}


#if 0
static int
ngx_squ_file_index(squ_State *l)
{
    ngx_str_t            key;
    ngx_file_info_t      fi;
    ngx_http_request_t  *r;
    ngx_squ_file_ctx_t  *ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, r->connection->log, 0, "squ file index");

    key.data = (u_char *) squL_checklstring(l, -1, &key.len);

    switch (key.len) {

    case 4:

        if (ngx_strncmp(key.data, "size", 4) == 0) {
            ctx = ngx_squ_file(l);

            ngx_memzero(&fi, sizeof(ngx_file_info_t));

            if (ngx_fd_info(ctx->file.fd, &fi) == NGX_FILE_ERROR) {
                ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                              ngx_fd_info_n " failed");
                squ_pushnil(l);
                squ_pushstring(l, ngx_fd_info_n " failed");
                return 1;
            }

            squ_pushnumber(l, (squ_Number) ngx_file_size(&fi));

            return 1;
        }

        break;

    case 10:

        if (ngx_strncmp(key.data, "attributes", 10) == 0) {
            /* TODO */
        }

        break;

    default:
        break;
    }

    return 0;
}
#endif


static int
ngx_squ_file_gc(squ_State *l)
{
    ngx_squ_file_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ file gc");

    ctx = ngx_squ_file(l);

    if (ctx->file.fd != NGX_INVALID_FILE) {
        ngx_close_file(ctx->file.fd);
        ctx->file.fd = NGX_INVALID_FILE;
    }

    ngx_destroy_pool(ctx->pool);

    return 0;
}


static int
ngx_squ_file_info(squ_State *l)
{
    ngx_squ_thread_t  *thr;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file info");

    /* TODO */

    return 0;
}


static ngx_inline ngx_squ_file_ctx_t *
ngx_squ_file(squ_State *l)
{
    ngx_squ_file_ctx_t  **ctx;

    ctx = squL_checkudata(l, 1, NGX_SQU_FILE);
    if (*ctx == NULL) {
        squL_error(l, "ngx_squ_file() *ctx == NULL");
    }

    return *ctx;
}


static void
ngx_squ_file_cleanup(void *data)
{
    ngx_squ_file_cleanup_ctx_t *cln_ctx = data;

    if (cln_ctx->ctx != NULL) {
        cln_ctx->ctx->thr = NULL;
        cln_ctx->ctx->cln_ctx = NULL;
    }
}


static ngx_int_t
ngx_squ_file_aio_read(ngx_squ_thread_t *thr, ngx_lua_file_ctx_t *ctx)
{
    ssize_t      n;
    ngx_buf_t   *b;
    ngx_file_t  *file;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file aio read");

    file = &ctx->file;
    b = ctx->in;

#if (NGX_HAVE_FILE_AIO)
    n = ngx_file_aio_read(file, b->start, ctx->size, ctx->offset, ctx->pool);
#else
    n = ngx_read_file(file, b->start, ctx->size, ctx->offset);
#endif

    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      "ngx_file_aio_read() \"%V\" failed", &file->name);
        squ_pushboolean(thr->l, 0);
        squ_pushstring(thr->l, "ngx_file_aio_read() failed");
        return 2;
    }

#if (NGX_HAVE_FILE_AIO)
    if (n == NGX_AGAIN) {
        ctx->file.aio->data = ctx;
        ctx->file.aio->handler = ngx_squ_file_aio_read_handler;
        return NGX_AGAIN;
    }
#endif

    ctx->offset += n;

    squ_pushlstring(thr->l, (char *) b->start, n);

    return 1;
}


#if (NGX_HAVE_FILE_AIO)
static void
ngx_squ_file_aio_read_handler(ngx_event_t *ev)
{
    ngx_int_t            rc;
    ngx_event_aio_t     *aio;
    ngx_squ_file_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "squ file aio read handler");

    aio = ev->data;
    ctx = aio->data;

    if (ctx->thr == NULL) {
        return;
    }

    rc = ngx_squ_file_aio_read(ctx->thr, ctx);

    rc = ngx_squ_thread_run(ctx->thr, rc);
    if (rc == NGX_AGAIN) {
        return;
    }

    ngx_squ_finalize(ctx->thr, rc);
}
#endif


static ngx_int_t
ngx_squ_file_aio_write(ngx_squ_thread_t *thr, ngx_lua_file_ctx_t *ctx)
{
    ssize_t      n;
    ngx_buf_t   *b;
    ngx_file_t  *file;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ file aio write");

    file = &ctx->file;
    b = ctx->out;

#if (NGX_HAVE_FILE_AIO) && ((NGX_LINUX) || (NGX_WIN32))

    n = ngx_file_aio_write(file, b->start, ctx->size, ctx->offset, ctx->pool);

    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      "ngx_file_aio_write() \"%V\" failed", &file->name);
        squ_pushboolean(thr->l, 0);
        squ_pushstring(thr->l, "ngx_file_aio_write() failed");
        return 2;
    }

    if (n == NGX_AGAIN) {
        ctx->file.aio->data = ctx;
        ctx->file.aio->handler = ngx_squ_file_aio_write_handler;
        return NGX_AGAIN;
    }

#else

    /* TODO: aio write for freebsd and solaris, etc */

    n = ngx_write_file(file, b->start, ctx->size, ctx->offset);

    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      "ngx_write_file() \"%V\" failed", &file->name);
        squ_pushboolean(thr->l, 0);
        squ_pushstring(thr->l, "ngx_write_file() failed");
        return 2;
    }

#endif

    /* TODO: n != ctx->size */

    ctx->offset += n;

    squ_pushinteger(thr->l, n);

    return 1;
}


#if (NGX_HAVE_FILE_AIO)
static void
ngx_squ_file_aio_write_handler(ngx_event_t *ev)
{
    ngx_int_t            rc;
    ngx_event_aio_t     *aio;
    ngx_squ_file_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0,
                   "squ file aio write handler");

    aio = ev->data;
    ctx = aio->data;

    if (ctx->thr == NULL) {
        return;
    }

    rc = ngx_squ_file_aio_write(ctx->thr, ctx);

    rc = ngx_squ_thread_run(ctx->thr, rc);
    if (rc == NGX_AGAIN) {
        return;
    }

    ngx_squ_finalize(ctx->thr, rc);
}
#endif


static ngx_int_t
ngx_squ_file_module_init(ngx_cycle_t *cycle)
{
    int              n;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ file module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    squ_getglobal(scf->l, NGX_SQU_TABLE);

    squL_newmetatable(scf->l, NGX_SQU_FILE);
    squ_pushvalue(scf->l, -1);
    squ_setfield(scf->l, -2, "__index");

    for (n = 0; ngx_squ_file_methods[n].name != NULL; n++) {
        squ_pushcfunction(scf->l, ngx_squ_file_methods[n].func);
        squ_setfield(scf->l, -2, ngx_squ_file_methods[n].name);
    }

    squ_pop(scf->l, 1);

    n = sizeof(ngx_squ_file_consts) / sizeof(ngx_squ_const_t) - 1;
    n += 2;

    squ_createtable(scf->l, 0, n);

    for (n = 0; ngx_squ_file_consts[n].name != NULL; n++) {
        squ_pushinteger(scf->l, ngx_squ_file_consts[n].value);
        squ_setfield(scf->l, -2, ngx_squ_file_consts[n].name);
    }

    squ_pushcfunction(scf->l, ngx_squ_file_open);
    squ_setfield(scf->l, -2, "open");
    squ_pushcfunction(scf->l, ngx_squ_file_info);
    squ_setfield(scf->l, -2, "attributes");

    squ_setfield(scf->l, -2, "file");

    squ_pop(scf->l, 1);

    return NGX_OK;
}
