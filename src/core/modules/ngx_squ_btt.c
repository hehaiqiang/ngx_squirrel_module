
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_lua_btt.h>


static void ngx_btt_get_peer_list(ngx_btt_ctx_t *ctx, ngx_btt_torrent_t *t);

static ngx_btt_torrent_t *ngx_btt_get_torrent(ngx_btt_conf_t *bcf,
    ngx_btt_ctx_t *ctx, ngx_rbtree_t *tree, ngx_queue_t *queue);
static void ngx_btt_free_torrent(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_rbtree_t *tree, ngx_btt_torrent_t *t);
static void ngx_btt_insert_torrent(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static ngx_btt_torrent_t *ngx_btt_lookup_torrent(ngx_btt_conf_t *bcf,
    ngx_btt_ctx_t *ctx, ngx_rbtree_t *tree);

static ngx_btt_peer_t *ngx_btt_get_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_rbtree_t *tree, ngx_queue_t *queue);
static void ngx_btt_free_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_rbtree_t *tree, ngx_btt_peer_t *p);
static void ngx_btt_insert_peer(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static ngx_btt_peer_t *ngx_btt_lookup_peer(ngx_btt_conf_t *bcf,
    ngx_btt_ctx_t *ctx, ngx_rbtree_t *tree);

static ngx_btt_peer_info_t *ngx_btt_get_peer_info(ngx_btt_conf_t *bcf,
    ngx_btt_ctx_t *ctx);
static void ngx_btt_free_peer_info(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_btt_peer_info_t *pi);

static void ngx_btt_expire(ngx_event_t *ev);
static ngx_int_t ngx_btt_init(ngx_shm_zone_t *shm_zone, void *data);

static ngx_int_t ngx_btt_process_init(ngx_cycle_t *cycle);
static void *ngx_btt_create_conf(ngx_cycle_t *cycle);
static char *ngx_btt_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_btt(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_btt_commands[] = {

    { ngx_string("btt"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE4,
      ngx_btt,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_btt_module_ctx = {
    ngx_string("btt"),
    ngx_btt_create_conf,
    ngx_btt_init_conf,
};


ngx_module_t  ngx_lua_btt_module = {
    NGX_MODULE_V1,
    &ngx_btt_module_ctx,                   /* module context */
    ngx_btt_commands,                      /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_btt_process_init,                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_int_t
ngx_btt_query_peers(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx)
{
    ngx_btt_torrent_t  *t;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ctx->log, 0, "btt query peers");

    ngx_shmtx_lock(&bcf->pool->mutex);

    t = ngx_btt_lookup_torrent(bcf, ctx, &bcf->btt->seeders_rbtree);
    if (t != NULL) {
        ngx_btt_get_peer_list(ctx, t);
    }

    if (ctx->peers_n == ctx->numwant) {
        ngx_shmtx_unlock(&bcf->pool->mutex);
        return NGX_OK;
    }

    t = ngx_btt_lookup_torrent(bcf, ctx, &bcf->btt->leechers_rbtree);
    if (t != NULL) {
        ngx_btt_get_peer_list(ctx, t);
    }

    ngx_shmtx_unlock(&bcf->pool->mutex);

    return NGX_OK;
}


ngx_int_t
ngx_btt_update_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx)
{
    ngx_btt_peer_t       *pl, *ps;
    ngx_btt_torrent_t    *tl, *ts;
    ngx_btt_peer_info_t  *pi;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ctx->log, 0, "btt update peer");

    ts = NULL;
    pl = NULL;
    ps = NULL;

    ngx_shmtx_lock(&bcf->pool->mutex);

    tl = ngx_btt_lookup_torrent(bcf, ctx, &bcf->btt->leechers_rbtree);

    if (tl == NULL && ctx->left != 0) {

        /* insert torrent into leechers rbtree */

        tl = ngx_btt_get_torrent(bcf, ctx, &bcf->btt->leechers_rbtree,
                                 &bcf->btt->leechers_queue);

        if (tl == NULL) {
            ngx_shmtx_unlock(&bcf->pool->mutex);
            return NGX_ERROR;
        }
    }

    if (ctx->left == 0) {
        ts = ngx_btt_lookup_torrent(bcf, ctx, &bcf->btt->seeders_rbtree);
        if (ts == NULL) {

            /* insert torrent into seeders rbtree */

            ts = ngx_btt_get_torrent(bcf, ctx, &bcf->btt->seeders_rbtree,
                                     &bcf->btt->seeders_queue);

            if (ts == NULL) {
                ngx_shmtx_unlock(&bcf->pool->mutex);
                return NGX_ERROR;
            }
        }
    }

    if (tl != NULL) {
        pl = ngx_btt_lookup_peer(bcf, ctx, &tl->peers_rbtree);
        if (pl == NULL && ctx->left != 0) {

            /* insert peer into leechers peer rbtree */

            pl = ngx_btt_get_peer(bcf, ctx, &tl->peers_rbtree,
                                  &tl->peers_queue);
            if (pl == NULL) {
                ngx_shmtx_unlock(&bcf->pool->mutex);
                return NGX_ERROR;
            }

            pl->peer_info = NULL;
        }
    }

    if (ts != NULL) {
        ps = ngx_btt_lookup_peer(bcf, ctx, &ts->peers_rbtree);
        if (ps == NULL) {

            /* insert peer into seeders peer rbtree */

            ps = ngx_btt_get_peer(bcf, ctx, &ts->peers_rbtree,
                                  &ts->peers_queue);
            if (ps == NULL) {
                ngx_shmtx_unlock(&bcf->pool->mutex);
                return NGX_ERROR;
            }

            ps->peer_info = NULL;
        }
    }

    if (ctx->left != 0) {

        if (pl->peer_info == NULL) {
            pl->peer_info = ngx_btt_get_peer_info(bcf, ctx);
            if (pl->peer_info == NULL) {
                ngx_shmtx_unlock(&bcf->pool->mutex);
                return NGX_ERROR;
            }
        }

        pi = pl->peer_info;

    } else {

        if (ps->peer_info == NULL && pl != NULL) {
            ps->peer_info = pl->peer_info;
            pl->peer_info = NULL;
        }

        if (ps->peer_info == NULL) {
            ps->peer_info = ngx_btt_get_peer_info(bcf, ctx);
            if (ps->peer_info == NULL) {
                ngx_shmtx_unlock(&bcf->pool->mutex);
                return NGX_ERROR;
            }
        }

        pi = ps->peer_info;

        if (pl != NULL) {
            ngx_btt_free_peer(bcf, ctx, &tl->peers_rbtree, pl);
        }
    }

    pi->uploaded = ctx->uploaded;
    pi->downloaded = ctx->downloaded;
    pi->left = ctx->left;
    pi->internal_ip = ctx->internal_ip;
    pi->external_ip = ctx->external_ip;
    pi->internal_port = ctx->internal_port;
    pi->external_port = ctx->external_port;

    ngx_memcpy(pi->peer_id, ctx->peer_id, sizeof(ctx->peer_id));

    ngx_shmtx_unlock(&bcf->pool->mutex);

    return NGX_OK;
}


static void
ngx_btt_get_peer_list(ngx_btt_ctx_t *ctx, ngx_btt_torrent_t *t)
{
    ngx_queue_t          *q;
    ngx_btt_peer_t       *p;
    ngx_btt_peer_info_t  *pi;

    if (ngx_queue_empty(&t->peers_queue)) {
        return;
    }

    for (q = ngx_queue_head(&t->peers_queue);
         q != ngx_queue_sentinel(&t->peers_queue);
         q = ngx_queue_next(q))
    {
        p = ngx_queue_data(q, ngx_btt_peer_t, queue);

        pi = p->peer_info;

        if (pi->external_ip == ctx->external_ip
            && pi->internal_port == ctx->internal_port)
        {
            continue;
        }

        ctx->peers[ctx->peers_n].internal_ip = pi->internal_ip;
        ctx->peers[ctx->peers_n].external_ip = pi->external_ip;
        ctx->peers[ctx->peers_n].internal_port = pi->internal_port;
        ctx->peers[ctx->peers_n].external_port = pi->external_port;
        ngx_memcpy(ctx->peers[ctx->peers_n].peer_id, pi->peer_id,
                   sizeof(pi->peer_id));

        if (++ctx->peers_n == ctx->numwant) {
            break;
        }
    }
}


static ngx_btt_torrent_t *
ngx_btt_get_torrent(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx, ngx_rbtree_t *tree,
    ngx_queue_t *queue)
{
    ngx_queue_t        *q;
    ngx_btt_torrent_t  *t;

    t = NULL;

    if (!ngx_queue_empty(&bcf->btt->free_torrents)) {
        q = ngx_queue_head(&bcf->btt->free_torrents);
        ngx_queue_remove(q);

        t = ngx_queue_data(q, ngx_btt_torrent_t, queue);
    }

    if (t == NULL) {
        t = ngx_slab_alloc_locked(bcf->pool, sizeof(ngx_btt_torrent_t));
        if (t == NULL) {
            return NULL;
        }
    }

    /* TODO */

    ngx_rbtree_init(&t->peers_rbtree, &t->peers_sentinel, ngx_btt_insert_peer);
    ngx_queue_init(&t->peers_queue);

    ngx_memcpy(t->info_hash, ctx->info_hash, sizeof(ctx->info_hash));

    t->node.key = ngx_crc32_short(ctx->info_hash, sizeof(ctx->info_hash));
    ngx_rbtree_insert(tree, &t->node);
    ngx_queue_insert_head(queue, &t->queue);

    return t;
}


#if 0
static void
ngx_btt_free_torrent(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_rbtree_t *tree, ngx_btt_torrent_t *t)
{
    ngx_rbtree_delete(tree, &t->node);
    ngx_queue_remove(&t->queue);

    ngx_queue_insert_head(&bcf->btt->free_torrents, &t->queue);
}
#endif


static void
ngx_btt_insert_torrent(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t  **p;
    ngx_btt_torrent_t   *t, *tt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            t = (ngx_btt_torrent_t *) node;
            tt = (ngx_btt_torrent_t *) temp;

            p = ngx_memn2cmp(t->info_hash, tt->info_hash,
                             sizeof(t->info_hash), sizeof(tt->info_hash))
                < 0 ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_btt_torrent_t *
ngx_btt_lookup_torrent(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_rbtree_t *tree)
{
    ngx_int_t           rc;
    ngx_rbtree_key_t    key;
    ngx_rbtree_node_t  *node, *sentinel;
    ngx_btt_torrent_t  *t;

    key = ngx_crc32_short(ctx->info_hash, sizeof(ctx->info_hash));

    node = tree->root;
    sentinel = tree->sentinel;

    while (node != sentinel) {

        if (key < node->key) {
            node = node->left;
            continue;
        }

        if (key > node->key) {
            node = node->right;
            continue;
        }

        /* key == node->key */

        do {
            t = (ngx_btt_torrent_t *) node;

            rc = ngx_memn2cmp(t->info_hash, ctx->info_hash,
                              sizeof(t->info_hash), sizeof(ctx->info_hash));

            if (rc == 0) {
                return t;
            }

            node = (rc < 0) ? node->left : node->right;

        } while (node != sentinel && key == node->key);

        break;
    }

    /* not found */

    return NULL;
}


static ngx_btt_peer_t *
ngx_btt_get_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx, ngx_rbtree_t *tree,
    ngx_queue_t *queue)
{
    ngx_queue_t     *q;
    ngx_btt_peer_t  *p;

    p = NULL;

    if (!ngx_queue_empty(&bcf->btt->free_peers)) {
        q = ngx_queue_head(&bcf->btt->free_peers);
        ngx_queue_remove(q);

        p = ngx_queue_data(q, ngx_btt_peer_t, queue);
    }

    if (p == NULL) {
        p = ngx_slab_alloc_locked(bcf->pool, sizeof(ngx_btt_peer_t));
        if (p == NULL) {
            return NULL;
        }
    }

    /* TODO */

    ngx_memcpy(&p->connection_id, &ctx->connection_id,
               sizeof(ctx->connection_id));

    p->node.key = ngx_crc32_short((u_char *) &ctx->connection_id,
                                  sizeof(ctx->connection_id));
    ngx_rbtree_insert(tree, &p->node);
    ngx_queue_insert_head(queue, &p->queue);

    return p;
}


static void
ngx_btt_free_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx, ngx_rbtree_t *tree,
    ngx_btt_peer_t *p)
{
    if (p->peer_info != NULL) {
        ngx_btt_free_peer_info(bcf, ctx, p->peer_info);

        p->peer_info = NULL;
    }

    ngx_rbtree_delete(tree, &p->node);
    ngx_queue_remove(&p->queue);

    ngx_queue_insert_head(&bcf->btt->free_peers, &p->queue);
}


static void
ngx_btt_insert_peer(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_btt_peer_t      *peer, *peert;
    ngx_rbtree_node_t  **p;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            peer = (ngx_btt_peer_t *) node;
            peert = (ngx_btt_peer_t *) temp;

            p = ngx_memn2cmp((u_char *) &peer->connection_id,
                             (u_char *) &peert->connection_id,
                             sizeof(peer->connection_id),
                             sizeof(peert->connection_id))
                < 0 ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


static ngx_btt_peer_t *
ngx_btt_lookup_peer(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_rbtree_t *tree)
{
    ngx_int_t           rc;
    ngx_btt_peer_t     *p;
    ngx_rbtree_key_t    key;
    ngx_rbtree_node_t  *node, *sentinel;

    key = ngx_crc32_short((u_char *) &ctx->connection_id,
                          sizeof(ctx->connection_id));

    node = tree->root;
    sentinel = tree->sentinel;

    while (node != sentinel) {

        if (key < node->key) {
            node = node->left;
            continue;
        }

        if (key > node->key) {
            node = node->right;
            continue;
        }

        /* key == node->key */

        do {
            p = (ngx_btt_peer_t *) node;

            rc = ngx_memn2cmp((u_char *) &p->connection_id,
                              (u_char *) &ctx->connection_id,
                              sizeof(p->connection_id),
                              sizeof(ctx->connection_id));

            if (rc == 0) {
                return p;
            }

            node = (rc < 0) ? node->left : node->right;

        } while (node != sentinel && key == node->key);

        break;
    }

    /* not found */

    return NULL;
}


static ngx_btt_peer_info_t *
ngx_btt_get_peer_info(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx)
{
    ngx_queue_t          *q;
    ngx_btt_peer_info_t  *pi;

    if (!ngx_queue_empty(&bcf->btt->free_peer_infos)) {
        q = ngx_queue_head(&bcf->btt->free_peer_infos);
        ngx_queue_remove(q);

        pi = ngx_queue_data(q, ngx_btt_peer_info_t, queue);
        return pi;
    }

    pi = ngx_slab_alloc_locked(bcf->pool, sizeof(ngx_btt_peer_info_t));
    if (pi == NULL) {
        return NULL;
    }

    return pi;
}


static void
ngx_btt_free_peer_info(ngx_btt_conf_t *bcf, ngx_btt_ctx_t *ctx,
    ngx_btt_peer_info_t *pi)
{
    ngx_queue_insert_head(&bcf->btt->free_peer_infos, &pi->queue);
}


static void
ngx_btt_expire(ngx_event_t *ev)
{
#if 0
    time_t              now;
    ngx_uint_t          i;
    ngx_queue_t        *q;
#endif
    ngx_btt_conf_t     *bcf;
#if 0
    ngx_btt_torrent_t  *t;
#endif

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ev->log, 0, "btt expire");

    bcf = ev->data;

#if 0
    if (!ngx_shmtx_trylock(&bcf->pool->mutex)) {
        goto done;
    }

    now = ngx_time();

    for (i = 0; i < 2; i++) {
        if (ngx_queue_empty(&bcf->cache->queue)) {
            break;
        }

        q = ngx_queue_last(&bcf->cache->queue);
        code = ngx_queue_data(q, ngx_btt_torrent_t, queue);

        if (code->expire >= now) {
            break;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_CORE, ev->log, 0,
                       "lua cache expire node \"%V\"", &code->path);

        ngx_queue_remove(&code->queue);
        ngx_rbtree_delete(&bcf->cache->rbtree, &code->node);
        ngx_slab_free_locked(bcf->pool, code);
    }

    ngx_shmtx_unlock(&bcf->pool->mutex);

done:
#endif

    ngx_add_timer(&bcf->event, bcf->expire * 1000 / 10);
}


static ngx_int_t
ngx_btt_init(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_btt_conf_t *obcf = data;

    size_t           len;
    ngx_btt_conf_t  *bcf;

    bcf = shm_zone->data;

    if (obcf) {
        bcf->btt = obcf->btt;
        bcf->pool = obcf->pool;
        return NGX_OK;
    }

    bcf->pool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        bcf->btt = bcf->pool->data;
        return NGX_OK;
    }

    bcf->btt = ngx_slab_alloc(bcf->pool, sizeof(ngx_btt_t));
    if (bcf->btt == NULL) {
        return NGX_ERROR;
    }

    bcf->pool->data = bcf->btt;

    ngx_rbtree_init(&bcf->btt->leechers_rbtree, &bcf->btt->leechers_sentinel,
                    ngx_btt_insert_torrent);
    ngx_queue_init(&bcf->btt->leechers_queue);

    ngx_rbtree_init(&bcf->btt->seeders_rbtree, &bcf->btt->seeders_sentinel,
                    ngx_btt_insert_torrent);
    ngx_queue_init(&bcf->btt->seeders_queue);

    ngx_queue_init(&bcf->btt->free_torrents);
    ngx_queue_init(&bcf->btt->free_peers);
    ngx_queue_init(&bcf->btt->free_peer_infos);

    len = sizeof(" in btt \"\"") + shm_zone->shm.name.len;

    bcf->pool->log_ctx = ngx_slab_alloc(bcf->pool, len);
    if (bcf->pool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(bcf->pool->log_ctx, " in btt \"%V\"%Z", &shm_zone->shm.name);

    return NGX_OK;
}


static ngx_int_t
ngx_btt_process_init(ngx_cycle_t *cycle)
{
    ngx_btt_conf_t  *bcf;

    /* TODO */

#if 0
    bcf = ngx_lua_get_conf(cycle, ngx_lua_btt_module);
#else
    bcf = (ngx_btt_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_lua_btt_module);
#endif

    if (bcf->btt == NULL) {
        return NGX_OK;
    }

    bcf->event.handler = ngx_btt_expire;
    bcf->event.data = bcf;
    bcf->event.log = cycle->log;

    ngx_add_timer(&bcf->event, bcf->expire * 1000 / 10);

    return NGX_OK;
}


static void *
ngx_btt_create_conf(ngx_cycle_t *cycle)
{
    ngx_btt_conf_t  *bcf;

    bcf = ngx_pcalloc(cycle->pool, sizeof(ngx_btt_conf_t));
    if (bcf == NULL) {
        return NULL;
    }

    bcf->size = NGX_CONF_UNSET_SIZE;
    bcf->expire = NGX_CONF_UNSET;
    bcf->interval = NGX_CONF_UNSET;

    return bcf;
}


static char *
ngx_btt_init_conf(ngx_cycle_t *cycle, void *conf)
{
    /* TODO */

    return NGX_CONF_OK;
}


static char *
ngx_btt(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_btt_conf_t *bcf = conf;

    ngx_str_t   *value, str;
    ngx_uint_t   i;

    if (bcf->name.data != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "name=", 5) == 0) {
            bcf->name.len = value[i].len - 5;
            bcf->name.data = value[i].data + 5;
            continue;
        }

        if (ngx_strncmp(value[i].data, "size=", 5) == 0) {
            str.len = value[i].len - 5;
            str.data = value[i].data + 5;
            bcf->size = ngx_parse_size(&str);
            if (bcf->size == (size_t) NGX_ERROR) {
                goto invalid;
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, "expire=", 7) == 0) {
            str.len = value[i].len - 7;
            str.data = value[i].data + 7;
            bcf->expire = ngx_parse_time(&str, 1);
            if (bcf->expire == NGX_ERROR) {
                goto invalid;
            }
            continue;
        }

        if (ngx_strncmp(value[i].data, "interval=", 9) == 0) {
            str.len = value[i].len - 9;
            str.data = value[i].data + 9;
            bcf->interval = ngx_parse_time(&str, 1);
            if (bcf->interval == NGX_ERROR) {
                goto invalid;
            }
            continue;
        }

        goto invalid;
    }

    if (bcf->name.data == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the directive \"btt\" must be specified");
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_size_value(bcf->size, 1024 * 1024 * 10);
    ngx_conf_init_value(bcf->expire, 60);
    ngx_conf_init_value(bcf->interval, 30);

    bcf->zone = ngx_shared_memory_add(cf, &bcf->name, bcf->size,
                                      &ngx_lua_btt_module);
    if (bcf->zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (bcf->zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate btt name \"%V\"", &bcf->name);
        return NGX_CONF_ERROR;
    }

    bcf->zone->init = ngx_btt_init;
    bcf->zone->data = bcf;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\" in btt", &value[i]);

    return NGX_CONF_ERROR;
}
