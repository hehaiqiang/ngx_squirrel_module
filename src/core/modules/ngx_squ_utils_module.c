
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_md5.h>
#include <ngx_sha1.h>
#include <ngx_squ.h>


typedef struct {
    ngx_event_t    event;
} ngx_squ_utils_ctx_t;


extern SQInteger ngx_squ_http(HSQUIRRELVM v);

static SQInteger ngx_squ_escape_uri(HSQUIRRELVM v);
static SQInteger ngx_squ_unescape_uri(HSQUIRRELVM v);
static SQInteger ngx_squ_encode_base64(HSQUIRRELVM v);
static SQInteger ngx_squ_decode_base64(HSQUIRRELVM v);
static SQInteger ngx_squ_crc16(HSQUIRRELVM v);
static SQInteger ngx_squ_crc32(HSQUIRRELVM v);
static SQInteger ngx_squ_murmur_hash2(HSQUIRRELVM v);
static SQInteger ngx_squ_md5(HSQUIRRELVM v);
static SQInteger ngx_squ_sha1(HSQUIRRELVM v);

static SQInteger ngx_squ_sleep(HSQUIRRELVM v);
static void ngx_squ_sleep_handler(ngx_event_t *ev);
static void ngx_squ_sleep_cleanup(void *data);

static ngx_int_t ngx_squ_utils_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_consts[] = {
    { "OK", NGX_OK },
    { "ERROR", NGX_ERROR },
    { "AGAIN", NGX_AGAIN },
    { "BUSY", NGX_BUSY },
    { "DONE", NGX_DONE },
    { "DECLINED", NGX_DECLINED },
    { "ABORT", NGX_ABORT },
    { NULL, 0 }
};


static SQRegFunction  ngx_squ_methods[] = {
    { "escape_uri", ngx_squ_escape_uri },
    { "unescape_uri", ngx_squ_unescape_uri },
    { "encode_base64", ngx_squ_encode_base64 },
    { "decode_base64", ngx_squ_decode_base64 },
    { "crc16", ngx_squ_crc16 },
    { "crc32", ngx_squ_crc32 },
    { "murmur_hash2", ngx_squ_murmur_hash2 },
    { "md5", ngx_squ_md5 },
    { "sha1", ngx_squ_sha1 },
    { "sleep", ngx_squ_sleep },
    { "http", ngx_squ_http },

#if 0
    iconv
#endif

    { NULL, NULL }
};


ngx_module_t  ngx_squ_utils_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_utils_module_init,             /* init module */
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
        &ngx_squ_utils_module,
        NULL
    };

    return modules;
}
#endif


static int
ngx_squ_escape_uri(HSQUIRRELVM v)
{
    size_t             len;
    u_char            *p, *last;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ escape uri");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    len = ngx_escape_uri(NULL, str.data, str.len, 0);
    if (len == 0) {
        sq_pushstring(v, (char *) str.data, str.len);
        return 1;
    }

    len = str.len + len * 2;

    p = ngx_pnalloc(thr->pool, len);
    if (p == NULL) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    last = (u_char *) ngx_escape_uri(p, str.data, str.len, 0);

    sq_pushstring(v, (char *) p, last - p);

    return 1;
}


static int
ngx_squ_unescape_uri(HSQUIRRELVM v)
{
    u_char            *dst, *p;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ unescape uri");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    p = ngx_pnalloc(thr->pool, str.len);
    if (p == NULL) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    dst = p;

    ngx_unescape_uri(&dst, &str.data, str.len, 0);

    sq_pushstring(v, (char *) p, dst - p);

    return 1;
}


static int
ngx_squ_encode_base64(HSQUIRRELVM v)
{
    ngx_str_t          dst, src;
    SQRESULT           rc;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ encode base64");

    rc = sq_getstring(v, 2, (SQChar **) &src.data);

    src.len = ngx_strlen(src.data);

    dst.len = ngx_base64_encoded_length(src.len);

    dst.data = ngx_pnalloc(thr->pool, dst.len);
    if (dst.data == NULL) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    ngx_encode_base64(&dst, &src);

    sq_pushstring(v, (char *) dst.data, dst.len);

    return 1;
}


static int
ngx_squ_decode_base64(HSQUIRRELVM v)
{
    ngx_str_t          dst, src;
    SQRESULT           rc;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ decode base64");

    rc = sq_getstring(v, 2, (SQChar **) &src.data);

    src.len = ngx_strlen(src.data);

    dst.len = ngx_base64_decoded_length(src.len);

    dst.data = ngx_pnalloc(thr->pool, dst.len);
    if (dst.data == NULL) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    if (ngx_decode_base64(&dst, &src) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0, "ngx_decode_base64() failed");
        sq_pushbool(v, SQFalse);
        return 1;
    }

    sq_pushstring(v, (char *) dst.data, dst.len);

    return 1;
}


static int
ngx_squ_crc16(HSQUIRRELVM v)
{
    u_char             crc[4], hex[8], *last;
    uint32_t           crc16;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ crc16");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    crc16 = ngx_crc(str.data, str.len);

    crc[0] = crc16 >> 24;
    crc[1] = (u_char) (crc16 >> 16);
    crc[2] = (u_char) (crc16 >> 8);
    crc[3] = (u_char) crc16;

    last = ngx_hex_dump(hex, crc, 4);

    sq_pushstring(v, (char *) hex, last - hex);

    return 1;
}


static int
ngx_squ_crc32(HSQUIRRELVM v)
{
    u_char             crc[4], hex[8], *last;
    uint32_t           crc32;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ crc32");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    crc32 = ngx_crc32_long(str.data, str.len);

    crc[0] = crc32 >> 24;
    crc[1] = (u_char) (crc32 >> 16);
    crc[2] = (u_char) (crc32 >> 8);
    crc[3] = (u_char) crc32;

    last = ngx_hex_dump(hex, crc, 4);

    sq_pushstring(v, (char *) hex, last - hex);

    return 1;
}


static int
ngx_squ_murmur_hash2(HSQUIRRELVM v)
{
    u_char             hash[4], hex[8], *last;
    uint32_t           murmur;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ murmur hash2");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    murmur = ngx_murmur_hash2(str.data, str.len);

    hash[0] = murmur >> 24;
    hash[1] = (u_char) (murmur >> 16);
    hash[2] = (u_char) (murmur >> 8);
    hash[3] = (u_char) murmur;

    last = ngx_hex_dump(hex, hash, 4);

    sq_pushstring(v, (char *) hex, last - hex);

    return 1;
}


static int
ngx_squ_md5(HSQUIRRELVM v)
{
    u_char            *md5, *hex, *last;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_md5_t          ctx;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ md5");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    md5 = ngx_pnalloc(thr->pool, 48);
    if (md5 == NULL) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    hex = md5 + 16;

    ngx_md5_init(&ctx);
    ngx_md5_update(&ctx, str.data, str.len);
    ngx_md5_final(md5, &ctx);

    last = ngx_hex_dump(hex, md5, 16);

    sq_pushstring(v, (char *) hex, last - hex);

    return 1;
}


static int
ngx_squ_sha1(HSQUIRRELVM v)
{
    u_char            *sha1, *hex, *last;
    SQRESULT           rc;
    ngx_str_t          str;
    ngx_sha1_t         ctx;
    ngx_squ_thread_t  *thr;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ sha1");

    rc = sq_getstring(v, 2, (SQChar **) &str.data);

    str.len = ngx_strlen(str.data);

    sha1 = ngx_pnalloc(thr->pool, 72);
    if (sha1 == NULL) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    hex = sha1 + 24;

    ngx_sha1_init(&ctx);
    ngx_sha1_update(&ctx, str.data, str.len);
    ngx_sha1_final(sha1, &ctx);

    last = ngx_hex_dump(hex, sha1, 20);

    sq_pushstring(v, (char *) hex, last - hex);

    return 1;
}


static SQInteger
ngx_squ_sleep(HSQUIRRELVM v)
{
    SQRESULT              rc;
    ngx_int_t             time;
    ngx_str_t             str;
    ngx_squ_thread_t     *thr;
    ngx_pool_cleanup_t   *cln;
    ngx_squ_utils_ctx_t  *ctx;

    thr = sq_getforeignptr(v);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ sleep");

    if (sq_gettype(v, 2) == OT_STRING) {
        rc = sq_getstring(v, 2, (SQChar **) &str.data);
        str.len = ngx_strlen(str.data);

        time = ngx_parse_time(&str, 0);
        if (time == NGX_ERROR) {
            sq_pushbool(v, SQFalse);
            return 1;
        }

    } else {
        rc = sq_getinteger(v, 2, &time);

        /* TODO: error handling */
    }

    ctx = ngx_squ_thread_get_module_ctx(thr, ngx_squ_utils_module);
    if (ctx == NULL) {
        ctx = ngx_pcalloc(thr->pool, sizeof(ngx_squ_utils_ctx_t));
        if (ctx == NULL) {
            sq_pushbool(v, SQFalse);
            return 1;
        }

        ngx_squ_thread_set_ctx(thr, ctx, ngx_squ_utils_module);
    }

    if (ctx->event.handler == NULL) {
        ctx->event.handler = ngx_squ_sleep_handler;
        ctx->event.data = thr;
        ctx->event.log = thr->log;

        cln = ngx_pool_cleanup_add(thr->pool, 0);
        if (cln == NULL) {
            sq_pushbool(v, SQFalse);
            return 1;
        }

        cln->handler = ngx_squ_sleep_cleanup;
        cln->data = thr;
    }

    ngx_add_timer(&ctx->event, time);

    return sq_suspendvm(v);
}


static void
ngx_squ_sleep_handler(ngx_event_t *ev)
{
    ngx_int_t          rc;
    ngx_squ_thread_t  *thr;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "squ sleep handler");

    thr = ev->data;

    sq_pushbool(thr->v, SQTrue);

    rc = ngx_squ_thread_run(thr, 1);
    if (rc == NGX_AGAIN) {
        return;
    }

    ngx_squ_finalize(thr, rc);
}


static void
ngx_squ_sleep_cleanup(void *data)
{
    ngx_squ_thread_t *thr = data;

    ngx_squ_utils_ctx_t  *ctx;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ sleep cleanup");

    ctx = ngx_squ_thread_get_module_ctx(thr, ngx_squ_utils_module);

    if (ctx->event.timer_set) {
        ngx_event_del_timer(&ctx->event);
    }
}


static ngx_int_t
ngx_squ_utils_module_init(ngx_cycle_t *cycle)
{
    int              n;
    SQRESULT         rc;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ utils module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    sq_pushroottable(scf->v);
    sq_pushstring(scf->v, NGX_SQU_TABLE, sizeof(NGX_SQU_TABLE) - 1);
    rc = sq_get(scf->v, -2);

    n = sizeof(ngx_squ_consts) / sizeof(ngx_squ_const_t) - 1;
    n += sizeof(ngx_squ_methods) / sizeof(SQRegFunction) - 1;

    sq_pushstring(scf->v, "utils", sizeof("utils") - 1);
    sq_newtableex(scf->v, n);

    for (n = 0; ngx_squ_consts[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_consts[n].name, -1);
        sq_pushinteger(scf->v, ngx_squ_consts[n].value);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    for (n = 0; ngx_squ_methods[n].name != NULL; n++) {
        sq_pushstring(scf->v, ngx_squ_methods[n].name, -1);
        sq_newclosure(scf->v, ngx_squ_methods[n].f, 0);
        rc = sq_newslot(scf->v, -3, SQFalse);
    }

    rc = sq_newslot(scf->v, -3, SQFalse);

    sq_pop(scf->v, 2);

    return NGX_OK;
}
