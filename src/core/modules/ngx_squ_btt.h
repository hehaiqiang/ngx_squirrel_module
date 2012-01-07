
/*
 * Copyright (C) Ngwsx
 */


#ifndef _NGX_BTT_H_INCLUDED_
#define _NGX_BTT_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>


#define NGX_BTT_ACTION_CONNECT   0
#define NGX_BTT_ACTION_ANNOUNCE  1
#define NGX_BTT_ACTION_SCRAPE    2
#define NGX_BTT_ACTION_ERROR     3


#define NGX_BTT_EVENT_NONE       0
#define NGX_BTT_EVENT_COMPLETED  1
#define NGX_BTT_EVENT_STARTED    2
#define NGX_BTT_EVENT_STOPPED    3


typedef struct {
    ngx_queue_t            queue;

    uint64_t               uploaded;
    uint64_t               downloaded;
    uint64_t               left;

    /* IPv4 */

    in_addr_t              internal_ip;
    in_addr_t              external_ip;
    in_port_t              internal_port;
    in_port_t              external_port;

    /* TODO: IPv6 */

    u_char                 peer_id[20];
} ngx_btt_peer_info_t;


typedef struct {
    ngx_rbtree_node_t      node;
    ngx_queue_t            queue;
    time_t                 expire;

    uint64_t               connection_id;
    ngx_btt_peer_info_t   *peer_info;
} ngx_btt_peer_t;


typedef struct {
    ngx_rbtree_node_t      node;
    ngx_queue_t            queue;
    time_t                 expire;

    ngx_rbtree_t           peers_rbtree;
    ngx_rbtree_node_t      peers_sentinel;
    ngx_queue_t            peers_queue;
    ngx_atomic_t           peers_n;

    u_char                 info_hash[20];
} ngx_btt_torrent_t;


typedef struct {
    ngx_rbtree_t           peers_rbtree;
    ngx_rbtree_node_t      peers_sentinel;
    ngx_queue_t            peers_queue;
    ngx_atomic_t           peers_n;

    ngx_rbtree_t           leechers_rbtree;
    ngx_rbtree_node_t      leechers_sentinel;
    ngx_queue_t            leechers_queue;
    ngx_atomic_t           leechers_n;

    ngx_rbtree_t           seeders_rbtree;
    ngx_rbtree_node_t      seeders_sentinel;
    ngx_queue_t            seeders_queue;
    ngx_atomic_t           seeders_n;

    ngx_queue_t            free_torrents;
    ngx_queue_t            free_peers;
    ngx_queue_t            free_peer_infos;
} ngx_btt_t;


typedef struct {
    ngx_str_t              name;
    size_t                 size;
    time_t                 expire;
    time_t                 interval;
    ngx_btt_t             *btt;
    ngx_slab_pool_t       *pool;
    ngx_shm_zone_t        *zone;
    ngx_event_t            event;
} ngx_btt_conf_t;


typedef struct {
    ngx_pool_t            *pool;
    ngx_log_t             *log;

    uint64_t               connection_id;
    uint32_t               action;
    uint32_t               transaction_id;

    u_char                 info_hash[20];
    u_char                 peer_id[20];
    u_char                 key[24];

    uint64_t               uploaded;
    uint64_t               downloaded;
    uint64_t               left;

    in_addr_t              internal_ip;
    in_addr_t              external_ip;
    in_port_t              internal_port;
    in_port_t              external_port;

    uint32_t               event;
    uint32_t               compact;
    uint32_t               numwant;
    uint32_t               no_peer_id;

    /* BitComet */

    u_char                 port_type[16];
    uint32_t               natmapped;

    /* NetTransport */

    uint32_t               supportcrypto;

    /* uTorrent */

    uint32_t               corrupt;

    u_char                *info_hashes[20];
    uint32_t               info_hashes_n;

    uint32_t               leechers;
    uint32_t               seeders;
    ngx_btt_peer_info_t   *peers;
    uint32_t               peers_n;
} ngx_btt_ctx_t;


ngx_int_t ngx_btt_query_peers(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx);
ngx_int_t ngx_btt_update_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx);


extern ngx_module_t  ngx_lua_btt_module;


#endif /* _NGX_BTT_H_INCLUDED_ */
