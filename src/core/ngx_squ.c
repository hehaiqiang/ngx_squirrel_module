
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ.h>


static void ngx_squ_aio_read(ngx_squ_thread_t *thr);
#if (NGX_HAVE_FILE_AIO)
static void ngx_squ_aio_read_handler(ngx_event_t *ev);
#endif
static void ngx_squ_handler(ngx_squ_thread_t *thr);
static SQInteger ngx_squ_reader(SQUserPointer data, SQUserPointer buf,
    SQInteger size);
static SQInteger ngx_squ_writer(SQUserPointer data, SQUserPointer buf,
    SQInteger size);
static void ngx_squ_compiler_error(HSQUIRRELVM v, const SQChar *desc,
    const SQChar *source, SQInteger line, SQInteger column);
static SQInteger ngx_squ_runtime_error(HSQUIRRELVM v);
static void ngx_squ_error(HSQUIRRELVM v, const SQChar *fmt, ...);
static void ngx_squ_print(HSQUIRRELVM v, const SQChar *fmt, ...);


ngx_int_t
ngx_squ_create(ngx_cycle_t *cycle, ngx_squ_conf_t *scf)
{
    SQRESULT  rc;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ create");

    scf->v = sq_open(1024);
    if (scf->v == NULL) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, 0, "sq_open() failed");
        return NGX_ERROR;
    }

    sq_setcompilererrorhandler(scf->v, ngx_squ_compiler_error);

    sq_newclosure(scf->v, ngx_squ_runtime_error, 0);
    sq_seterrorhandler(scf->v);

    sq_setprintfunc(scf->v, ngx_squ_print, ngx_squ_error);

    /* TODO: opening squirrel stdlib */

    sq_pushroottable(scf->v);

    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    sq_newtableex(scf->v, ngx_squ_max_module);
    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_poptop(scf->v);

    return NGX_OK;
}


void
ngx_squ_destroy(void *data)
{
    ngx_squ_conf_t *scf = data;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ destroy");

    sq_close(scf->v);
}


ngx_int_t
ngx_squ_thread_create(ngx_squ_thread_t *thr)
{
    SQInteger        top, state, rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ thread create");

    scf = (ngx_squ_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx, ngx_squ_module);

    thr->v = sq_newthread(scf->v, 1024);
    if (thr->v == NULL) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "sq_newthread() failed");
        return NGX_ERROR;
    }

    sq_setforeignptr(thr->v, thr);
    sq_setprintfunc(thr->v, ngx_squ_print, ngx_squ_error);

    sq_resetobject(&thr->obj);

    rc = sq_getstackobj(scf->v, -1, &thr->obj);
    if (SQ_FAILED(rc)) {
        sq_poptop(scf->v);
        return NGX_ERROR;
    }

    sq_addref(scf->v, &thr->obj);
    sq_poptop(scf->v);
    thr->ref = 1;

    sq_move(thr->v, scf->v, -1);

    state = sq_getvmstate(scf->v);
    state = sq_getvmstate(thr->v);
    top = sq_gettop(scf->v);
    top = sq_gettop(thr->v);

#if 0
    squ_newtable(thr->l);

    squ_createtable(thr->l, 0, 1);
    squ_pushvalue(thr->l, SQU_GLOBALSINDEX);
    squ_setfield(thr->l, -2, "__index");
    squ_setmetatable(thr->l, -2);

    squ_replace(thr->l, SQU_GLOBALSINDEX);

    thr->ref = squL_ref(scf->l, -2);
    if (thr->ref == SQU_NOREF) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                      "squL_ref() return SQU_NOREF");
        squ_settop(scf->l, top);
        return NGX_ERROR;
    }

    squ_pop(scf->l, 1);

    sq_move(scf->l, thr->l, scf->l, 1);

    squ_pushvalue(thr->l, SQU_GLOBALSINDEX);
    squ_setfenv(thr->l, -2);
#endif

    //ngx_squ_debug_start(thr);

    return NGX_OK;
}


void
ngx_squ_thread_destroy(ngx_squ_thread_t *thr, ngx_uint_t force)
{
    SQBool           rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ thread destroy");

    if (thr->file.fd != NGX_INVALID_FILE) {
        ngx_close_file(thr->file.fd);
        thr->file.fd = NGX_INVALID_FILE;
    }

    if (!thr->ref) {
        return;
    }

    /* TODO */

    scf = (ngx_squ_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx, ngx_squ_module);

    rc = sq_release(scf->v, &thr->obj);

#if 0
    ngx_squ_debug_stop(thr);

    squ_getfield(scf->l, SQU_REGISTRYINDEX, NGX_SQU_KEY_REF);

    squ_rawgeti(scf->l, -1, thr->ref);
    l = squ_tothread(scf->l, -1);
    squ_pop(scf->l, 1);

    if (l != NULL && force) {
        squ_getglobal(l, NGX_SQU_KEY_CODE);
        squ_getfenv(l, -1);
        squ_xmove(l, scf->l, 1);

        squ_newtable(l);
        squ_setfenv(l, -2);

        do {
            squ_settop(l, 0);
        } while (squ_resume(l, 0) == SQU_YIELD);

        squ_settop(l, 0);
        squ_getglobal(l, NGX_SQU_KEY_CODE);
        squ_xmove(scf->l, l, 1);
        squ_setfenv(l, -2);
        squ_pop(l, 1);
    }

    squL_unref(scf->l, -1, thr->ref);
    squ_pop(scf->l, 1);
#endif

    thr->ref = 0;
}


ngx_int_t
ngx_squ_thread_run(ngx_squ_thread_t *thr, int n)
{
    SQRESULT   rc;
    SQInteger  state;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ thread run");

    sq_setforeignptr(thr->v, thr);

    state = sq_getvmstate(thr->v);

    if (state == SQ_VMSTATE_RUNNING) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "incorrect vm state");
        ngx_squ_thread_destroy(thr, 0);
        return NGX_ERROR;
    }

    /* TODO: handling exception */
    /* TODO: n */

    if (state == SQ_VMSTATE_IDLE) {

        sq_pushroottable(thr->v);

        rc = sq_call(thr->v, 1, SQFalse, SQTrue);

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, thr->log, 0, "sq_call() rc:%d", rc);

        if (SQ_FAILED(rc)) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                          "sq_call() failed rc:%d", rc);
        }

    } else {

        /* state == SQ_VMSTATE_SUSPENDED */

        rc = sq_wakeupvm(thr->v, SQTrue, SQFalse, SQTrue, SQFalse);

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, thr->log, 0,
                       "sq_wakeupvm() rc:%d", rc);

        if (SQ_FAILED(rc)) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                          "sq_wakeupvm() failed rc:%d", rc);
        }
    }

    if (SQ_FAILED(rc)) {
        ngx_squ_thread_destroy(thr, 0);
        return NGX_ERROR;
    }

    state = sq_getvmstate(thr->v);

    if (state == SQ_VMSTATE_SUSPENDED) {
        return NGX_AGAIN;
    }

    ngx_squ_thread_destroy(thr, 0);

    return NGX_OK;
}


ngx_int_t
ngx_squ_check_script(ngx_squ_thread_t *thr)
{
    ngx_int_t          rc;
    ngx_file_info_t    fi;
    ngx_squ_script_t  *script;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ check script");

    script = thr->script;

    if (script->from == NGX_SQU_SCRIPT_FROM_CONF) {
        thr->size = script->code.len;
        thr->mtime = -1;

    } else {

        ngx_memzero(&fi, sizeof(ngx_file_info_t));

        rc = (ngx_int_t) ngx_file_info(thr->path.data, &fi);

        if (rc == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                          ngx_file_info_n " \"%V\" failed", &thr->path);
            return NGX_ERROR;
        }

        thr->size = (size_t) ngx_file_size(&fi);
        thr->mtime = ngx_file_mtime(&fi);
    }

    return NGX_OK;
}


void
ngx_squ_load_script(ngx_squ_thread_t *thr)
{
    int                mode;
    size_t             size;
    ngx_file_t        *file;
    ngx_squ_script_t  *script;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ load script");

    /* TODO: size */

    size = ngx_max(thr->size * 4, ngx_pagesize);

    thr->buf = ngx_create_temp_buf(thr->pool, size);
    if (thr->buf == NULL) {
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

    if (ngx_squ_cache_get(thr) == NGX_OK) {
        thr->cached = 1;
        ngx_squ_handler(thr);
        return;
    }

    script = thr->script;

    if (script->from == NGX_SQU_SCRIPT_FROM_CONF) {
        thr->ssp = ngx_calloc_buf(thr->pool);
        if (thr->ssp == NULL) {
            ngx_squ_finalize(thr, NGX_ERROR);
            return;
        }

        thr->ssp->pos = script->code.data;
        thr->ssp->last = script->code.data + script->code.len;

        ngx_squ_handler(thr);
        return;
    }

    thr->ssp = ngx_create_temp_buf(thr->pool, thr->size);
    if (thr->ssp == NULL) {
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

    file = &thr->file;
    file->name = thr->path;
    file->log = thr->log;

    mode = NGX_FILE_RDONLY|NGX_FILE_NONBLOCK;

#if (NGX_WIN32 && NGX_HAVE_FILE_AIO)
    if (thr->aio && (ngx_event_flags & NGX_USE_IOCP_EVENT)) {
        mode |= NGX_FILE_OVERLAPPED;
    }
#endif

    file->fd = ngx_open_file(thr->path.data, mode, NGX_FILE_OPEN,
                             NGX_FILE_DEFAULT_ACCESS);
    if (file->fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      ngx_open_file_n " \"%V\" failed", &thr->path);
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

    ngx_squ_aio_read(thr);
}


static void
ngx_squ_aio_read(ngx_squ_thread_t *thr)
{
    ssize_t      n;
    ngx_file_t  *file;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ aio read");

    file = &thr->file;

#if (NGX_HAVE_FILE_AIO)
    if (thr->aio) {
        n = ngx_file_aio_read(file, thr->ssp->pos, thr->size, 0, thr->pool);

    } else {
        n = ngx_read_file(file, thr->ssp->pos, thr->size, 0);
    }
#else
    n = ngx_read_file(file, thr->ssp->pos, thr->size, 0);
#endif

    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, ngx_errno,
                      "ngx_file_aio_read() \"%V\" failed", &thr->path);
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

#if (NGX_HAVE_FILE_AIO)
    if (n == NGX_AGAIN) {
        file->aio->data = thr;
        file->aio->handler = ngx_squ_aio_read_handler;
        return;
    }
#endif

    thr->ssp->last += n;

    ngx_close_file(file->fd);
    file->fd = NGX_INVALID_FILE;

    ngx_squ_handler(thr);
}


#if (NGX_HAVE_FILE_AIO)
static void
ngx_squ_aio_read_handler(ngx_event_t *ev)
{
    ngx_event_aio_t   *aio;
    ngx_squ_thread_t  *thr;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "squ aio read handler");

    aio = ev->data;
    thr = aio->data;

    ngx_squ_aio_read(thr);
}
#endif


static void
ngx_squ_handler(ngx_squ_thread_t *thr)
{
    SQRESULT         rc;
    SQInteger        n;
    ngx_buf_t       *b;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ handler");

    scf = (ngx_squ_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx, ngx_squ_module);

    sq_setforeignptr(scf->v, thr);

    thr->ctx = ngx_pcalloc(thr->pool, sizeof(void *) * ngx_squ_max_module);
    if (thr->ctx == NULL) {
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

    if (!thr->cached) {
        if (thr->script->parser(thr) == NGX_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "parsing error");
            ngx_squ_finalize(thr, NGX_ERROR);
            return;
        }
    }

    b = thr->buf;

    if (!thr->cached) {
        rc = sq_compilebuffer(scf->v, (SQChar *) b->pos, b->last - b->pos,
                              (SQChar *) thr->path.data, SQTrue);
        if (SQ_FAILED(rc)) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                          "sq_compilebuffer() failed");
            ngx_squ_finalize(thr, NGX_ERROR);
            return;
        }

    } else {

        rc = sq_readclosure(scf->v, ngx_squ_reader, thr);
        if (SQ_FAILED(rc)) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                          "sq_readclosure() failed");
            ngx_squ_finalize(thr, NGX_ERROR);
            return;
        }
    }

    if (!thr->cached) {
        thr->buf->pos = thr->buf->start;
        thr->buf->last = thr->buf->start;

        rc = sq_writeclosure(scf->v, ngx_squ_writer, thr);
        if (SQ_FAILED(rc)) {
            ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                          "sq_writeclosure() failed (rc:%d)", rc);
            sq_poptop(scf->v);
            ngx_squ_finalize(thr, NGX_ERROR);
            return;
        }

        ngx_squ_cache_set(thr);
    }

    sq_pushroottable(scf->v);

    rc = sq_call(scf->v, 1, SQTrue, SQTrue);
    if (SQ_FAILED(rc)) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "sq_call() failed");
        sq_poptop(scf->v);
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

    sq_remove(scf->v, -2);

    if (ngx_squ_thread_create(thr) == NGX_ERROR) {
        sq_poptop(scf->v);
        n = sq_gettop(scf->v);
        ngx_squ_finalize(thr, NGX_ERROR);
        return;
    }

    sq_poptop(scf->v);
    n = sq_gettop(scf->v);

    rc = ngx_squ_thread_run(thr, 0);
    if (rc != NGX_AGAIN) {
        ngx_squ_finalize(thr, rc);
        return;
    }
}


static SQInteger
ngx_squ_reader(SQUserPointer data, SQUserPointer buf, SQInteger size)
{
    ngx_squ_thread_t *thr = data;

    size_t      n;
    ngx_buf_t  *b;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ reader");

    b = thr->buf;

    n = b->last - b->pos;

    if (n == 0) {
        return -1;
    }

    n = ngx_min(n, (size_t) size);
    ngx_memcpy(buf, b->pos, n);
    b->pos += n;

    return n;
}


static SQInteger
ngx_squ_writer(SQUserPointer data, SQUserPointer buf, SQInteger size)
{
    ngx_squ_thread_t *thr = data;

    size_t      n;
    ngx_buf_t  *b;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ writer");

    b = thr->buf;

    n = b->end - b->last;

    if (n < (size_t) size) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                      "ngx_squ_writer() not enough space in buffer");
        return -1;
    }

    b->last = ngx_cpymem(b->last, buf, size);

    return size;
}


static void
ngx_squ_compiler_error(HSQUIRRELVM v, const SQChar *desc, const SQChar *source,
    SQInteger line, SQInteger column)
{
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                  "\"%s\" in %s:%d:%d", desc, source, line, column);
}


static SQInteger
ngx_squ_runtime_error(HSQUIRRELVM v)
{
    SQChar            *str;
    SQRESULT           rc;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    rc = sq_getstring(v, 2, &str);

    if (SQ_SUCCEEDED(rc)) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "%s", str);
    }

    return 0;
}


static void
ngx_squ_error(HSQUIRRELVM v, const SQChar *fmt, ...)
{
    u_char             *p;
    va_list            args;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    va_start(args, fmt);
    p = va_arg(args, u_char *);
    ngx_log_error(NGX_LOG_ALERT, thr->log, 0, (char *) p);
    va_end(args);
}


static void
ngx_squ_print(HSQUIRRELVM v, const SQChar *fmt, ...)
{
    u_char            *p;
    va_list            args;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    va_start(args, fmt);
    p = va_arg(args, u_char *);
    ngx_squ_output(thr, p, ngx_strlen(p));
    va_end(args);
}
