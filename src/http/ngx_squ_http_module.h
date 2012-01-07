
/*
 * Copyright (C) Ngwsx
 */


#ifndef _NGX_SQU_HTTP_MODULE_H_INCLUDED_
#define _NGX_SQU_HTTP_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_squ.h>
#include <ngx_squ_http_session.h>


#define NGX_SQU_HTTP_TABLE  "http_srv"


typedef struct {
    void                  **conf;
} ngx_squ_http_main_conf_t;


typedef struct {
    void                  **conf;
} ngx_squ_http_srv_conf_t;


typedef struct {
    ngx_squ_script_t        script;
    void                  **conf;
} ngx_squ_http_loc_conf_t;


typedef struct {
    ngx_http_request_t     *r;
    ngx_squ_session_t      *s;
    ngx_chain_t            *out;
    ngx_chain_t            *last;
    ngx_str_t               req_body;
} ngx_squ_http_ctx_t;


#endif /* _NGX_SQU_HTTP_MODULE_H_INCLUDED_ */
