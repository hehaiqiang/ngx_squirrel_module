
/*
 * Copyright (C) Ngwsx
 */


#ifndef _NGX_SQU_TCP_MODULE_H_INCLUDED_
#define _NGX_SQU_TCP_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_tcp.h>
#include <ngx_squ.h>


#define NGX_SQU_TCP_TABLE  "tcp_srv"


typedef struct {
    void                 **conf;
} ngx_squ_tcp_main_conf_t;


typedef struct {
    ngx_squ_script_t       script;
    void                 **conf;
} ngx_squ_tcp_srv_conf_t;


typedef struct {
    ngx_tcp_session_t     *s;
    ngx_chain_t           *out;
    ngx_chain_t           *last;
    ngx_int_t              rc;
    ngx_uint_t             not_event;
} ngx_squ_tcp_ctx_t;


extern ngx_dll ngx_module_t  ngx_squ_tcp_module;


#endif /* _NGX_SQU_TCP_MODULE_H_INCLUDED_ */
