
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ.h>


#define NGX_SQU_MAX_PARSERS  16


#define NGX_SQU_FUNCTION_START   "return function() { "
#define NGX_SQU_FUNCTION_END     " }"

#define NGX_SQU_PRINT_START      " print(@\""
#define NGX_SQU_PRINT_END        "\"); "

#define NGX_SQU_EXP_PRINT_START  " print("
#define NGX_SQU_EXP_PRINT_END    "); "


typedef struct {
    ngx_uint_t    stub;
} ngx_squ_parser_conf_t;


static ngx_int_t ngx_squ_parse_default(ngx_squ_thread_t *thr);
static ngx_int_t ngx_squ_parse_ssp(ngx_squ_thread_t *thr);

static void *ngx_squ_parser_create_conf(ngx_cycle_t *cycle);
static char *ngx_squ_parser_init_conf(ngx_cycle_t *cycle, void *conf);


static ngx_core_module_t  ngx_squ_parser_module_ctx = {
    ngx_string("parser"),
    ngx_squ_parser_create_conf,
    ngx_squ_parser_init_conf,
};


ngx_module_t  ngx_squ_parser_module = {
    NGX_MODULE_V1,
    &ngx_squ_parser_module_ctx,            /* module context */
    NULL,                                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_squ_parser_t  ngx_squ_default_parser = {
    ngx_string("default"),
    ngx_squ_parse_default
};


static ngx_squ_parser_t  ngx_squ_ssp_parser = {
    ngx_string("ssp"),
    ngx_squ_parse_ssp
};


static ngx_squ_parser_t  *ngx_squ_parsers[NGX_SQU_MAX_PARSERS];
static ngx_uint_t         ngx_squ_parser_n;


ngx_squ_parser_pt
ngx_squ_parser_find(ngx_log_t *log, ngx_str_t *name)
{
    ngx_uint_t         i;
    ngx_squ_parser_t  *parser;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, log, 0, "squ parser find");

    for (i = 0; i < ngx_squ_parser_n; i++) {
        parser = ngx_squ_parsers[i];

        if (parser->name.len == name->len
            && ngx_strncmp(parser->name.data, name->data, name->len) == 0)
        {
            return parser->parser;
        }
    }

    return NULL;
}


static ngx_int_t
ngx_squ_parse_default(ngx_squ_thread_t *thr)
{
    size_t   size;
    u_char  *out, *p;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ parse default");

    out = ngx_cpymem(thr->buf->last, NGX_SQU_FUNCTION_START,
                     sizeof(NGX_SQU_FUNCTION_START) - 1);

    size = thr->ssp->last - thr->ssp->pos;
    p = thr->ssp->pos;

    /* UTF-8 BOM */

    if (size >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
        p += 3;
        size -= 3;
    }

    out = ngx_cpymem(out, p, size);

    out = ngx_cpymem(out, NGX_SQU_FUNCTION_END,
                     sizeof(NGX_SQU_FUNCTION_END) - 1);

    thr->buf->last = out;

    return NGX_OK;
}


static ngx_int_t
ngx_squ_parse_ssp(ngx_squ_thread_t *thr)
{
    u_char      *p, ch, *out, *html_start, *squ_start, *squ_end;
    ngx_uint_t   backslash, dquoted, squoted;
    enum {
        sw_start = 0,
        sw_html_block,
        sw_squ_start,
        sw_squ_block_start,
        sw_squ_block,
        sw_squ_block_end,
        sw_squ_exp_block_start,
        sw_squ_exp_block,
        sw_squ_exp_block_end,
        sw_error
    } state;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ parse ssp");

    state = sw_start;

    html_start = NULL;
    squ_start = NULL;
    backslash = 0;
    dquoted = 0;
    squoted = 0;

    out = ngx_cpymem(thr->buf->last, NGX_SQU_FUNCTION_START,
                     sizeof(NGX_SQU_FUNCTION_START) - 1);

    /* TODO */

    for (p = thr->ssp->pos; p < thr->ssp->last; p++) {
        ch = *p;

        switch (state) {

        case sw_start:
            if (ch == '<') {
                html_start = NULL;
                squ_start = p;

                state = sw_squ_start;
                break;
            }

            out = ngx_cpymem(out, NGX_SQU_PRINT_START,
                             sizeof(NGX_SQU_PRINT_START) - 1);

            *out++ = ch;

            html_start = p;
            squ_start = NULL;

            state = sw_html_block;
            break;

        case sw_html_block:
            if (ch == '<') {
                squ_start = p;

                state = sw_squ_start;
                break;
            }

            *out++ = ch;

            break;

        case sw_squ_start:
            if (ch == '%') {
                state = sw_squ_block_start;
                break;
            }

            if (html_start == NULL) {
                html_start = squ_start;
                squ_start = NULL;

                out = ngx_cpymem(out, NGX_SQU_PRINT_START,
                                 sizeof(NGX_SQU_PRINT_START) - 1);
            }

            *out++ = '<';
            *out++ = ch;

            state = sw_html_block;
            break;

        case sw_squ_block_start:
            if (html_start != NULL) {
                html_start = NULL;

                out = ngx_cpymem(out, NGX_SQU_PRINT_END,
                                 sizeof(NGX_SQU_PRINT_END) - 1);
            }

            backslash = 0;
            dquoted = 0;
            squoted = 0;

            if (ch == '=') {
                state = sw_squ_exp_block_start;
                break;
            }

            /* TODO: xxx */

            *out++ = ch;

            state = sw_squ_block;
            break;

        case sw_squ_block:
            switch (ch) {

            case '\'':
                if (backslash || dquoted || squoted) {
                    squoted = 0;
                    backslash = 0;

                } else {
                    squoted = 1;
                }
                break;

            case '\"':
                if (backslash || dquoted || squoted) {
                    dquoted = 0;
                    backslash = 0;

                } else {
                    dquoted = 1;
                }
                break;

            case '\\':
                if (backslash) {
                    backslash = 0;

                } else {
                    backslash = 1;
                }
                break;

            case '%':
                if (backslash || dquoted || squoted) {
                    break;
                }

                squ_end = p;

                state = sw_squ_block_end;
                break;

            default:
                backslash = 0;
                break;
            }

            if (state != sw_squ_block_end) {
                *out++ = ch;
            }

            break;

        case sw_squ_block_end:
            if (ch != '>') {
                /* syntax error */
                state = sw_error;
                break;
            }

            squ_start = NULL;

            state = sw_start;
            break;

        case sw_squ_exp_block_start:
            out = ngx_cpymem(out, NGX_SQU_EXP_PRINT_START,
                             sizeof(NGX_SQU_EXP_PRINT_START) - 1);

            *out++ = ch;

            state = sw_squ_exp_block;
            break;

        case sw_squ_exp_block:
            switch (ch) {

            case '\'':
                if (backslash || dquoted || squoted) {
                    squoted = 0;
                    backslash = 0;

                } else {
                    squoted = 1;
                }
                break;

            case '\"':
                if (backslash || dquoted || squoted) {
                    dquoted = 0;
                    backslash = 0;

                } else {
                    dquoted = 1;
                }
                break;

            case '\\':
                if (backslash) {
                    backslash = 0;

                } else {
                    backslash = 1;
                }
                break;

            case '%':
                if (backslash || dquoted || squoted) {
                    break;
                }

                squ_end = p;

                state = sw_squ_exp_block_end;
                break;

            default:
                backslash = 0;
                break;
            }

            if (state != sw_squ_exp_block_end) {
                *out++ = ch;
            }

            break;

        case sw_squ_exp_block_end:
            if (ch != '>') {
                /* syntax error */
                state = sw_error;
                break;
            }

            /* TODO: xxx */

            out = ngx_cpymem(out, NGX_SQU_EXP_PRINT_END,
                             sizeof(NGX_SQU_EXP_PRINT_END) - 1);

            squ_start = NULL;

            state = sw_start;
            break;

        case sw_error:
            /* TODO: error handling */
            break;
        }
    }

    if (squ_start != NULL) {
        /* TODO: error handling */
    }

    if (html_start != NULL) {
        out = ngx_cpymem(out, NGX_SQU_PRINT_END, sizeof(NGX_SQU_PRINT_END) - 1);
    }

    out = ngx_cpymem(out, NGX_SQU_FUNCTION_END,
                     sizeof(NGX_SQU_FUNCTION_END) - 1);

    thr->buf->last = out;

    return NGX_OK;
}


static void *
ngx_squ_parser_create_conf(ngx_cycle_t *cycle)
{
    ngx_squ_parser_conf_t  *lpcf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ parser create conf");

    lpcf = ngx_pcalloc(cycle->pool, sizeof(ngx_squ_parser_conf_t));
    if (lpcf == NULL) {
        return NULL;
    }

    ngx_squ_parsers[ngx_squ_parser_n++] = &ngx_squ_ssp_parser;
    ngx_squ_parsers[ngx_squ_parser_n++] = &ngx_squ_default_parser;

    return lpcf;
}


static char *
ngx_squ_parser_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ parser init conf");

    /* TODO */

    return NGX_CONF_OK;
}
