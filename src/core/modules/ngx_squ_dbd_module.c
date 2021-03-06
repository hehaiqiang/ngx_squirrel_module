
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_dbd.h>
#include <ngx_squ.h>


typedef struct {
    ngx_rbtree_node_t             node;
    ngx_str_t                     name;
    ngx_str_t                     drv;
    ngx_str_t                     host;
    in_port_t                     port;
    ngx_str_t                     db;
    ngx_str_t                     user;
    ngx_str_t                     passwd;
    ngx_uint_t                    connection_n;
} ngx_squ_dbd_pool_conf_t;


typedef struct {
    ngx_queue_t                   queue;
    ngx_str_t                     name;
    ngx_pool_t                   *pool;
    ngx_dbd_t                    *dbd;
    ngx_uint_t                    connected;
} ngx_squ_dbd_connection_t;


typedef struct {
    ngx_rbtree_node_t             node;
    ngx_str_t                     name;
    ngx_queue_t                   connections;
    ngx_uint_t                    connection_n;
    ngx_queue_t                   free_connections;
    ngx_uint_t                    free_connection_n;
} ngx_squ_dbd_pool_t;


typedef struct {
    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;
} ngx_squ_dbd_t;


typedef struct {
    ngx_str_t                     name;
    size_t                        size;
    ngx_squ_dbd_t                *dbd;
    ngx_slab_pool_t              *pool;
    ngx_shm_zone_t               *zone;
    ngx_rbtree_t                  rbtree;
    ngx_rbtree_node_t             sentinel;
} ngx_squ_dbd_conf_t;


typedef struct ngx_squ_dbd_cleanup_ctx_s  ngx_squ_dbd_cleanup_ctx_t;


typedef struct {
    ngx_pool_t                   *pool;
    ngx_squ_dbd_connection_t     *c;
    ngx_str_t                    *col_names;
    ngx_list_t                    col_fields;
    ngx_str_t                    *cur_row;
    uint64_t                      row_count;
    uint64_t                      col_count;
    ngx_int_t                     rc;
    ngx_uint_t                    not_event;
    ngx_squ_thread_t             *thr;
    ngx_squ_dbd_cleanup_ctx_t    *cln_ctx;
} ngx_squ_dbd_ctx_t;


struct ngx_squ_dbd_cleanup_ctx_s {
    ngx_squ_dbd_ctx_t            *ctx;
};


static int ngx_squ_dbd_create_pool(squ_State *l);
static int ngx_squ_dbd_destroy_pool(squ_State *l);
static void ngx_squ_dbd_pool_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static ngx_squ_dbd_pool_conf_t *ngx_squ_dbd_pool_lookup(
    ngx_squ_dbd_conf_t *ldcf, ngx_str_t *name);

static int ngx_squ_dbd_execute(squ_State *l);

static ngx_squ_dbd_connection_t *ngx_squ_dbd_get_connection(
    ngx_squ_thread_t *thr, ngx_str_t *name);
static void ngx_squ_dbd_free_connection(ngx_squ_dbd_conf_t *ldcf,
    ngx_squ_dbd_connection_t *c);
static void ngx_squ_dbd_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel);
static ngx_squ_dbd_pool_t *ngx_squ_dbd_lookup(ngx_squ_dbd_conf_t *ldcf,
    ngx_str_t *name);

static void ngx_squ_dbd_connect(void *data);
static void ngx_squ_dbd_query(void *data);
static void ngx_squ_dbd_column(void *data);
static void ngx_squ_dbd_row(void *data);
static void ngx_squ_dbd_field(void *data);

static void ngx_squ_dbd_finalize(ngx_squ_dbd_ctx_t *ctx, ngx_int_t rc);
static void ngx_squ_dbd_cleanup(void *data);

static ngx_int_t ngx_squ_dbd_init(ngx_shm_zone_t *shm_zone, void *data);

static ngx_int_t ngx_squ_dbd_module_init(ngx_cycle_t *cycle);
static void *ngx_squ_dbd_create_conf(ngx_cycle_t *cycle);
static char *ngx_squ_dbd_init_conf(ngx_cycle_t *cycle, void *conf);
static char *ngx_squ_dbd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static squL_Reg  ngx_squ_dbd_methods[] = {
    { "create_pool", ngx_squ_dbd_create_pool },
    { "destroy_pool", ngx_squ_dbd_destroy_pool },
    { "execute", ngx_squ_dbd_execute },
    { NULL, NULL }
};


static ngx_command_t  ngx_squ_dbd_commands[] = {

    { ngx_string("squ_dbd"),
      NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE2,
      ngx_squ_dbd,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_core_module_t  ngx_squ_dbd_module_ctx = {
    ngx_string("dbd"),
    ngx_squ_dbd_create_conf,
    ngx_squ_dbd_init_conf,
};


ngx_module_t  ngx_squ_dbd_module = {
    NGX_MODULE_V1,
    &ngx_squ_dbd_module_ctx,               /* module context */
    ngx_squ_dbd_commands,                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_dbd_module_init,               /* init module */
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
        &ngx_squ_dbd_module,
        NULL
    };

    return modules;
}
#endif


static int
ngx_squ_dbd_create_pool(squ_State *l)
{
    int                       n;
    char                     *errstr;
    size_t                    size;
    u_char                   *p;
    in_port_t                 port;
    ngx_str_t                 name, drv, host, db, user, passwd;
    ngx_uint_t                max_connections;
    ngx_squ_thread_t         *thr;
    ngx_squ_dbd_conf_t       *ldcf;
    ngx_squ_dbd_pool_conf_t  *conf;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ dbd create pool");

    if (!squ_istable(l, 1)) {
        return squL_error(l, "invalid argument, must be a table");
    }

    n = squ_gettop(l);

    squ_getfield(l, 1, "name");
    name.data = (u_char *) squL_checklstring(l, -1, &name.len);

    squ_getfield(l, 1, "driver");
    drv.data = (u_char *) squL_checklstring(l, -1, &drv.len);

    squ_getfield(l, 1, "host");
    host.data = (u_char *) squL_optlstring(l, -1, "", &host.len);

    squ_getfield(l, 1, "port");
    port = (in_port_t) squL_optnumber(l, -1, 0);

    squ_getfield(l, 1, "database");
    db.data = (u_char *) squL_checklstring(l, -1, &db.len);

    squ_getfield(l, 1, "user");
    user.data = (u_char *) squL_optlstring(l, -1, "", &user.len);

    squ_getfield(l, 1, "password");
    passwd.data = (u_char *) squL_optlstring(l, -1, "", &passwd.len);

    squ_getfield(l, 1, "max_connections");
    max_connections = (ngx_uint_t) squL_checknumber(l, -1);

    ldcf = ngx_squ_thread_get_conf(thr, ngx_squ_dbd_module);

    ngx_shmtx_lock(&ldcf->pool->mutex);

    conf = ngx_squ_dbd_pool_lookup(ldcf, &name);
    if (conf != NULL) {
        goto done;
    }

    /* TODO: alignment */

    size = sizeof(ngx_squ_dbd_conf_t)
           + name.len + drv.len + host.len
           + sizeof(in_port_t)
           + db.len + user.len + passwd.len
           + sizeof(ngx_uint_t);

    p = ngx_slab_alloc_locked(ldcf->pool, size);
    if (p == NULL) {
        ngx_log_error(NGX_LOG_ALERT, thr->log, 0,
                      "ngx_slab_alloc_locked() failed");
        errstr = "ngx_slab_alloc_locked() failed";
        goto error;
    }

    conf = (ngx_squ_dbd_pool_conf_t *) p;
    p += sizeof(ngx_squ_dbd_pool_conf_t);

    conf->name.len = name.len;
    conf->name.data = p;
    p = ngx_cpymem(conf->name.data, name.data, name.len);

    conf->drv.len = drv.len;
    conf->drv.data = p;
    p = ngx_cpymem(conf->drv.data, drv.data, drv.len);

    conf->host.len = host.len;
    conf->host.data = p;
    p = ngx_cpymem(conf->host.data, host.data, host.len);

    conf->port = port;

    conf->db.len = db.len;
    conf->db.data = p;
    p = ngx_cpymem(conf->db.data, db.data, db.len);

    conf->user.len = user.len;
    conf->user.data = p;
    p = ngx_cpymem(conf->user.data, user.data, user.len);

    conf->passwd.len = passwd.len;
    conf->passwd.data = p;
    p = ngx_cpymem(conf->passwd.data, passwd.data, passwd.len);

    conf->connection_n = max_connections;

    ngx_memzero(&conf->node, sizeof(ngx_rbtree_node_t));

    conf->node.key = ngx_crc32_short(name.data, name.len);
    ngx_rbtree_insert(&ldcf->dbd->rbtree, &conf->node);

done:

    ngx_shmtx_unlock(&ldcf->pool->mutex);

    squ_settop(l, n);
    squ_pushboolean(l, 1);

    return 1;

error:

    ngx_shmtx_unlock(&ldcf->pool->mutex);

    squ_settop(l, n);
    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return 2;
}


static int
ngx_squ_dbd_destroy_pool(squ_State *l)
{
    ngx_str_t                 name;
    ngx_squ_thread_t         *thr;
    ngx_squ_dbd_conf_t       *ldcf;
    ngx_squ_dbd_pool_conf_t  *conf;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ dbd destroy pool");

    name.data = (u_char *) squL_checklstring(l, -1, &name.len);

    ldcf = ngx_squ_thread_get_conf(thr, ngx_squ_dbd_module);

    ngx_shmtx_lock(&ldcf->pool->mutex);

    conf = ngx_squ_dbd_pool_lookup(ldcf, &name);

    /* TODO */

    ngx_shmtx_unlock(&ldcf->pool->mutex);

    return 0;
}


static void
ngx_squ_dbd_pool_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t        **p;
    ngx_squ_dbd_pool_conf_t   *conf, *conf_temp;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            conf = (ngx_squ_dbd_pool_conf_t *) node;
            conf_temp = (ngx_squ_dbd_pool_conf_t *) temp;

            p = ngx_memn2cmp(conf->name.data, conf_temp->name.data,
                             conf->name.len, conf_temp->name.len)
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


static ngx_squ_dbd_pool_conf_t *
ngx_squ_dbd_pool_lookup(ngx_squ_dbd_conf_t *ldcf, ngx_str_t *name)
{
    ngx_int_t                 rc;
    ngx_rbtree_key_t          key;
    ngx_rbtree_node_t        *node, *sentinel;
    ngx_squ_dbd_pool_conf_t  *conf;

    key = ngx_crc32_short(name->data, name->len);

    node = ldcf->dbd->rbtree.root;
    sentinel = ldcf->dbd->rbtree.sentinel;

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
            conf = (ngx_squ_dbd_pool_conf_t *) node;

            rc = ngx_memn2cmp(conf->name.data, name->data, conf->name.len,
                              name->len);

            if (rc == 0) {
                return conf;
            }

            node = (rc < 0) ? node->left : node->right;

        } while (node != sentinel && key == node->key);

        break;
    }

    /* not found */

    return NULL;
}


static int
ngx_squ_dbd_execute(squ_State *l)
{
    char                       *errstr;
    ngx_str_t                   name, sql;
    ngx_int_t                   rc;
    ngx_pool_t                 *pool;
    ngx_squ_thread_t           *thr;
    ngx_squ_dbd_ctx_t          *ctx;
    ngx_pool_cleanup_t         *cln;
    ngx_squ_dbd_cleanup_ctx_t  *cln_ctx;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ dbd execute");

    name.data = (u_char *) squL_checklstring(l, 1, &name.len);
    sql.data = (u_char *) squL_checklstring(l, 2, &sql.len);

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
        errstr = "ngx_create_pool() failed";
        goto error;
    }

    ctx = ngx_pcalloc(pool, sizeof(ngx_squ_dbd_ctx_t));
    if (ctx == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pcalloc() failed";
        goto error;
    }

    ctx->pool = pool;

    cln_ctx = ngx_pcalloc(thr->pool, sizeof(ngx_squ_dbd_cleanup_ctx_t));
    if (cln_ctx == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pcalloc() failed";
        goto error;
    }

    cln = ngx_pool_cleanup_add(thr->pool, 0);
    if (cln == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_pool_cleanup_add() failed";
        goto error;
    }

    cln->handler = ngx_squ_dbd_cleanup;
    cln->data = cln_ctx;

    ctx->thr = thr;
    ctx->cln_ctx = cln_ctx;

    ctx->c = ngx_squ_dbd_get_connection(thr, &name);
    if (ctx->c == NULL) {
        ngx_destroy_pool(pool);
        errstr = "ngx_squ_dbd_get_connection() failed";
        goto error;
    }

    cln_ctx->ctx = ctx;

    /* TODO: escape sql */

    ngx_dbd_set_sql(ctx->c->dbd, sql.data, sql.len);

    ctx->rc = 0;
    ctx->not_event = 1;

    if (!ctx->c->connected) {
        ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_connect, ctx);

        ngx_squ_dbd_connect(ctx);

    } else {
        ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_query, ctx);

        ngx_squ_dbd_query(ctx);
    }

    ctx->not_event = 0;

    rc = ctx->rc;

    if (rc == NGX_AGAIN) {
        return squ_yield(l, 0);
    }

    cln_ctx->ctx = NULL;

    ngx_destroy_pool(ctx->pool);

    return rc;

error:

    squ_pushboolean(l, 0);
    squ_pushstring(l, errstr);

    return 2;
}


static ngx_squ_dbd_connection_t *
ngx_squ_dbd_get_connection(ngx_squ_thread_t *thr, ngx_str_t *name)
{
    size_t                     size;
    u_char                    *p, *drv, *host, *db, *user, *passwd;
    in_port_t                  port;
    ngx_uint_t                 max_connections;
    ngx_pool_t                *pool;
    ngx_queue_t               *q;
    ngx_squ_dbd_conf_t        *ldcf;
    ngx_squ_dbd_pool_t        *dbd_pool;
    ngx_squ_dbd_pool_conf_t   *conf;
    ngx_squ_dbd_connection_t  *c;

    ldcf = ngx_squ_thread_get_conf(thr, ngx_squ_dbd_module);

    ngx_shmtx_lock(&ldcf->pool->mutex);

    conf = ngx_squ_dbd_pool_lookup(ldcf, name);
    if (conf == NULL) {
        ngx_shmtx_unlock(&ldcf->pool->mutex);
        return NULL;
    }

    drv = ngx_palloc(thr->pool, conf->drv.len + 1);
    if (drv == NULL) {
        ngx_shmtx_unlock(&ldcf->pool->mutex);
        return NULL;
    }
    ngx_cpystrn(drv, conf->drv.data, conf->drv.len + 1);

    host = ngx_palloc(thr->pool, conf->host.len + 1);
    if (host == NULL) {
        ngx_shmtx_unlock(&ldcf->pool->mutex);
        return NULL;
    }
    ngx_cpystrn(host, conf->host.data, conf->host.len + 1);

    port = conf->port;

    db = ngx_palloc(thr->pool, conf->db.len + 1);
    if (db == NULL) {
        ngx_shmtx_unlock(&ldcf->pool->mutex);
        return NULL;
    }
    ngx_cpystrn(db, conf->db.data, conf->db.len + 1);

    user = ngx_palloc(thr->pool, conf->user.len + 1);
    if (user == NULL) {
        ngx_shmtx_unlock(&ldcf->pool->mutex);
        return NULL;
    }
    ngx_cpystrn(user, conf->user.data, conf->user.len + 1);

    passwd = ngx_palloc(thr->pool, conf->passwd.len + 1);
    if (passwd == NULL) {
        ngx_shmtx_unlock(&ldcf->pool->mutex);
        return NULL;
    }
    ngx_cpystrn(passwd, conf->passwd.data, conf->passwd.len + 1);

    max_connections = conf->connection_n;

    ngx_shmtx_unlock(&ldcf->pool->mutex);

    dbd_pool = ngx_squ_dbd_lookup(ldcf, name);
    if (dbd_pool != NULL) {
        if (!ngx_queue_empty(&dbd_pool->free_connections)) {
            q = ngx_queue_last(&dbd_pool->free_connections);
            c = ngx_queue_data(q, ngx_squ_dbd_connection_t, queue);

            ngx_queue_remove(q);
            dbd_pool->free_connection_n--;

            ngx_queue_insert_head(&dbd_pool->connections, q);
            dbd_pool->connection_n++;

            return c;
        }

        if (dbd_pool->connection_n == max_connections) {
            return NULL;
        }

        goto new_connection;
    }

    size = sizeof(ngx_squ_dbd_pool_t) + name->len;

    p = ngx_calloc(size, ngx_cycle->log);
    if (p == NULL) {
        return NULL;
    }

    dbd_pool = (ngx_squ_dbd_pool_t *) p;
    p += sizeof(ngx_squ_dbd_pool_t);

    dbd_pool->name.len = name->len;
    dbd_pool->name.data = p;
    ngx_memcpy(dbd_pool->name.data, name->data, name->len);

    dbd_pool->node.key = ngx_crc32_short(name->data, name->len);
    ngx_rbtree_insert(&ldcf->rbtree, &dbd_pool->node);

    ngx_queue_init(&dbd_pool->connections);
    ngx_queue_init(&dbd_pool->free_connections);

new_connection:

    pool = ngx_create_pool(ngx_pagesize, ngx_cycle->log);
    if (pool == NULL) {
        return NULL;
    }

    size = sizeof(ngx_squ_dbd_connection_t) + name->len;

    p = ngx_pcalloc(pool, size);
    if (p == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    c = (ngx_squ_dbd_connection_t *) p;
    p += sizeof(ngx_squ_dbd_connection_t);

    c->pool = pool;

    c->name.len = name->len;
    c->name.data = p;
    ngx_memcpy(c->name.data, name->data, name->len);

    c->dbd = ngx_dbd_create(pool, ngx_cycle->log, drv);
    if (c->dbd == NULL) {
        ngx_destroy_pool(pool);
        return NULL;
    }

    ngx_dbd_set_options(c->dbd, NGX_DBD_OPT_NON_BLOCKING);
    ngx_dbd_set_tcp(c->dbd, host, port);
    ngx_dbd_set_auth(c->dbd, user, passwd);
    ngx_dbd_set_db(c->dbd, db);

    ngx_queue_insert_head(&dbd_pool->connections, &c->queue);
    dbd_pool->connection_n++;

    return c;
}


static void
ngx_squ_dbd_free_connection(ngx_squ_dbd_conf_t *ldcf,
    ngx_squ_dbd_connection_t *c)
{
    ngx_squ_dbd_pool_t  *pool;

    pool = ngx_squ_dbd_lookup(ldcf, &c->name);
    if (pool == NULL) {
        /* TODO: error handling */
        return;
    }

    ngx_queue_remove(&c->queue);
    pool->connection_n--;

    ngx_queue_insert_head(&pool->free_connections, &c->queue);
    pool->free_connection_n++;
}


static void
ngx_squ_dbd_insert_value(ngx_rbtree_node_t *temp, ngx_rbtree_node_t *node,
    ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t   **p;
    ngx_squ_dbd_pool_t   *pool, *pool_temp;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            pool = (ngx_squ_dbd_pool_t *) node;
            pool_temp = (ngx_squ_dbd_pool_t *) temp;

            p = ngx_memn2cmp(pool->name.data, pool_temp->name.data,
                             pool->name.len, pool_temp->name.len)
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


static ngx_squ_dbd_pool_t *
ngx_squ_dbd_lookup(ngx_squ_dbd_conf_t *ldcf, ngx_str_t *name)
{
    ngx_int_t            rc;
    ngx_rbtree_key_t     key;
    ngx_rbtree_node_t   *node, *sentinel;
    ngx_squ_dbd_pool_t  *pool;

    key = ngx_crc32_short(name->data, name->len);

    node = ldcf->rbtree.root;
    sentinel = ldcf->rbtree.sentinel;

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
            pool = (ngx_squ_dbd_pool_t *) node;

            rc = ngx_memn2cmp(pool->name.data, name->data, pool->name.len,
                              name->len);

            if (rc == 0) {
                return pool;
            }

            node = (rc < 0) ? node->left : node->right;

        } while (node != sentinel && key == node->key);

        break;
    }

    /* not found */

    return NULL;
}


static void
ngx_squ_dbd_connect(void *data)
{
    ngx_squ_dbd_ctx_t *ctx = data;

    ngx_int_t  rc;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ dbd connect");

    rc = ngx_dbd_connect(ctx->c->dbd);

    if (rc == NGX_AGAIN) {
        ctx->rc = NGX_AGAIN;
        return;
    }

    if (rc == NGX_ERROR) {
        ngx_squ_dbd_finalize(ctx, NGX_ERROR);
        return;
    }

    /* rc == NGX_OK */

    ctx->c->connected = 1;

    ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_query, ctx);

    ngx_squ_dbd_query(ctx);
}


static void
ngx_squ_dbd_query(void *data)
{
    ngx_squ_dbd_ctx_t *ctx = data;

    uint64_t   cols;
    ngx_int_t  rc;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ dbd query");

    rc = ngx_dbd_query(ctx->c->dbd);

    if (rc == NGX_AGAIN) {
        ctx->rc = NGX_AGAIN;
        return;
    }

    if (rc == NGX_ERROR) {
        ngx_squ_dbd_finalize(ctx, NGX_ERROR);
        return;
    }

    /* rc == NGX_OK */

    if (ctx->thr != NULL) {
        cols = ngx_dbd_result_column_count(ctx->c->dbd);

        if (cols > 0) {
            ctx->col_names = ngx_palloc(ctx->pool,
                                        sizeof(ngx_str_t) * (size_t) cols);
            if (ctx->col_names == NULL) {
                ngx_squ_dbd_finalize(ctx, NGX_ERROR);
                return;
            }
        }
    }

    ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_column, ctx);

    ngx_squ_dbd_column(ctx);
}


static void
ngx_squ_dbd_column(void *data)
{
    ngx_squ_dbd_ctx_t *ctx = data;

    uint64_t   cols;
    ngx_int_t  rc;
    ngx_str_t  name;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ dbd column");

    for ( ;; ) {

        rc = ngx_dbd_column_read(ctx->c->dbd);

        if (rc == NGX_AGAIN) {
            ctx->rc = NGX_AGAIN;
            return;
        }

        if (rc == NGX_ERROR) {
            ngx_squ_dbd_finalize(ctx, NGX_ERROR);
            return;
        }

        if (rc == NGX_DONE) {
            break;
        }

        /* rc == NGX_OK */

        if (ctx->thr == NULL) {
            continue;
        }

        name.data = ngx_dbd_column_name(ctx->c->dbd);
        name.len = ngx_strlen(name.data);

        ctx->col_names[ctx->col_count].data = ngx_pstrdup(ctx->pool, &name);
        if (ctx->col_names[ctx->col_count].data == NULL) {
            ngx_squ_dbd_finalize(ctx, NGX_ERROR);
            return;
        }

        ctx->col_names[ctx->col_count++].len = name.len;
    }

    /* rc == NGX_DONE */

    if (ctx->thr != NULL) {
        cols = ngx_dbd_result_column_count(ctx->c->dbd);

        if (cols > 0) {
            if (ngx_list_init(&ctx->col_fields, ctx->pool, 16,
                              sizeof(ngx_str_t) * (size_t) cols)
                == NGX_ERROR)
            {
                ngx_squ_dbd_finalize(ctx, NGX_ERROR);
                return;
            }
        }
    }

    ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_row, ctx);

    ngx_squ_dbd_row(ctx);
}


static void
ngx_squ_dbd_row(void *data)
{
    ngx_squ_dbd_ctx_t *ctx = data;

    ngx_int_t  rc;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ dbd row");

    for ( ;; ) {

        rc = ngx_dbd_row_read(ctx->c->dbd);

        if (rc == NGX_AGAIN) {
            ctx->rc = NGX_AGAIN;
            return;
        }

        if (rc == NGX_ERROR) {
            ngx_squ_dbd_finalize(ctx, NGX_ERROR);
            return;
        }

        if (rc == NGX_DONE) {
            break;
        }

        /* rc == NGX_OK */

        if (ctx->thr != NULL) {
            ctx->cur_row = ngx_list_push(&ctx->col_fields);
            if (ctx->cur_row == NULL) {
                ngx_squ_dbd_finalize(ctx, NGX_ERROR);
                return;
            }
        }

        ctx->col_count = 0;
        ctx->row_count++;

        ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_field, ctx);

        ngx_squ_dbd_field(ctx);
        return;
    }

    /* rc == NGX_DONE */

    ngx_squ_dbd_finalize(ctx, NGX_OK);
}


static void
ngx_squ_dbd_field(void *data)
{
    ngx_squ_dbd_ctx_t *ctx = data;

    size_t      size;
    u_char     *value;
    ngx_int_t   rc;
    ngx_str_t   str;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ dbd field");

    for ( ;; ) {

        /* ngx_dbd_field_read */

        rc = ngx_dbd_field_buffer(ctx->c->dbd, &value, &size);

        if (rc == NGX_AGAIN) {
            ctx->rc = NGX_AGAIN;
            return;
        }

        if (rc == NGX_ERROR) {
            ngx_squ_dbd_finalize(ctx, NGX_ERROR);
            return;
        }

        if (rc == NGX_DONE) {
            break;
        }

        /* rc == NGX_OK */

        if (ctx->thr == NULL) {
            continue;
        }

        str.len = size;
        str.data = value;

        ctx->cur_row[ctx->col_count].data = ngx_pstrdup(ctx->pool, &str);
        if (ctx->cur_row[ctx->col_count].data == NULL) {
            ngx_squ_dbd_finalize(ctx, NGX_ERROR);
            return;
        }

        ctx->cur_row[ctx->col_count++].len = str.len;
    }

    ngx_dbd_set_handler(ctx->c->dbd, ngx_squ_dbd_row, ctx);

    ngx_squ_dbd_row(ctx);
}


static void
ngx_squ_dbd_finalize(ngx_squ_dbd_ctx_t *ctx, ngx_int_t rc)
{
    uint64_t             col_count, col, row;
    ngx_str_t           *names, *elts, *fields;
    ngx_uint_t           i;
    ngx_list_part_t     *part;
    ngx_squ_thread_t    *thr;
    ngx_squ_dbd_conf_t  *ldcf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, ngx_cycle->log, 0, "squ dbd finalize");

    ldcf = ngx_squ_get_conf(ngx_cycle, ngx_squ_dbd_module);

    if (ctx->cln_ctx != NULL) {
        ctx->cln_ctx->ctx = NULL;
    }

    thr = ctx->thr;

    if (thr == NULL) {
        ngx_squ_dbd_free_connection(ldcf, ctx->c);
        ngx_destroy_pool(ctx->pool);
        return;
    }

    ctx->rc = 1;

    if (rc == NGX_OK) {
        col_count = ngx_dbd_result_column_count(ctx->c->dbd);

        squ_newtable(thr->l);

        squ_pushnumber(thr->l, (squ_Number) col_count);
        squ_setfield(thr->l, -2, "col_count");

        squ_pushnumber(thr->l, (squ_Number) ctx->row_count);
        squ_setfield(thr->l, -2, "row_count");

        squ_pushnumber(thr->l,
                       (squ_Number) ngx_dbd_result_affected_rows(ctx->c->dbd));
        squ_setfield(thr->l, -2, "affected_rows");

        squ_pushnumber(thr->l,
                       (squ_Number) ngx_dbd_result_insert_id(ctx->c->dbd));
        squ_setfield(thr->l, -2, "insert_id");

        if (col_count > 0) {
            names = ctx->col_names;

            squ_createtable(thr->l, (int) col_count, 0);
            for (col = 0; col < col_count; col++) {
                squ_pushlstring(thr->l, (char *) names[col].data,
                                names[col].len);
                squ_rawseti(thr->l, -2, (int) col + 1);
            }
            squ_setfield(thr->l, -2, "columns");

            row = 1;

            squ_createtable(thr->l, (int) ctx->row_count, 0);

            part = &ctx->col_fields.part;
            elts = part->elts;

            for (i = 0; /* void */ ; i++) {

                if (i >= part->nelts) {
                    if (part->next == NULL) {
                        break;
                    }

                    part = part->next;
                    elts = part->elts;
                    i = 0;
                }

                fields = elts + col_count * i;

                squ_createtable(thr->l, 0, (int) col_count * 2);

                for (col = 0; col < col_count; col++) {
                    squ_pushlstring(thr->l, (char *) names[col].data,
                                    names[col].len);
                    squ_pushlstring(thr->l, (char *) fields[col].data,
                                    fields[col].len);
                    squ_rawset(thr->l, -3);

                    squ_pushlstring(thr->l, (char *) fields[col].data,
                                    fields[col].len);
                    squ_rawseti(thr->l, -2, (int) col + 1);
                }

                squ_rawseti(thr->l, -2, (int) row++);
            }

            squ_setfield(thr->l, -2, "rows");
        }

    } else {
        squ_pushboolean(thr->l, 0);
        squ_pushstring(thr->l, (char *) ngx_dbd_error(ctx->c->dbd));

        ctx->rc++;
    }

    ngx_squ_dbd_free_connection(ldcf, ctx->c);

    if (ctx->not_event) {
        return;
    }

    rc = ctx->rc;

    ngx_destroy_pool(ctx->pool);

    rc = ngx_squ_thread_run(thr, rc);
    if (rc == NGX_AGAIN) {
        return;
    }

    ngx_squ_finalize(thr, rc);
}


static void
ngx_squ_dbd_cleanup(void *data)
{
    ngx_squ_dbd_cleanup_ctx_t *cln_ctx = data;

    if (cln_ctx->ctx != NULL) {
        cln_ctx->ctx->thr = NULL;
        cln_ctx->ctx->cln_ctx = NULL;
    }
}


static ngx_int_t
ngx_squ_dbd_init(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_squ_dbd_conf_t *oldcf = data;

    size_t               len;
    ngx_squ_dbd_conf_t  *ldcf;

    ldcf = shm_zone->data;

    if (oldcf) {
        ldcf->dbd = oldcf->dbd;
        ldcf->pool = oldcf->pool;
        return NGX_OK;
    }

    ldcf->pool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        ldcf->dbd = ldcf->pool->data;
        return NGX_OK;
    }

    ldcf->dbd = ngx_slab_alloc(ldcf->pool, sizeof(ngx_squ_dbd_t));
    if (ldcf->dbd == NULL) {
        return NGX_ERROR;
    }

    ldcf->pool->data = ldcf->dbd;

    ngx_rbtree_init(&ldcf->dbd->rbtree, &ldcf->dbd->sentinel,
                    ngx_squ_dbd_pool_insert_value);

    len = sizeof(" in squ dbd \"\"") + shm_zone->shm.name.len;

    ldcf->pool->log_ctx = ngx_slab_alloc(ldcf->pool, len);
    if (ldcf->pool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(ldcf->pool->log_ctx, " in squ dbd \"%V\"%Z",
                &shm_zone->shm.name);

    return NGX_OK;
}


static ngx_int_t
ngx_squ_dbd_module_init(ngx_cycle_t *cycle)
{
    int              n;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ dbd module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    squ_getglobal(scf->l, NGX_SQU_TABLE);

    n = sizeof(ngx_squ_dbd_methods) / sizeof(squL_Reg) - 1;

    squ_createtable(scf->l, 0, n);

    for (n = 0; ngx_squ_dbd_methods[n].name != NULL; n++) {
        squ_pushcfunction(scf->l, ngx_squ_dbd_methods[n].func);
        squ_setfield(scf->l, -2, ngx_squ_dbd_methods[n].name);
    }

    squ_setfield(scf->l, -2, "database");

    squ_pop(scf->l, 1);

    return NGX_OK;
}


static void *
ngx_squ_dbd_create_conf(ngx_cycle_t *cycle)
{
    ngx_squ_dbd_conf_t  *ldcf;

    ldcf = ngx_pcalloc(cycle->pool, sizeof(ngx_squ_dbd_conf_t));
    if (ldcf == NULL) {
        return NULL;
    }

    ldcf->size = NGX_CONF_UNSET_SIZE;

    return ldcf;
}


static char *
ngx_squ_dbd_init_conf(ngx_cycle_t *cycle, void *conf)
{
    ngx_squ_dbd_conf_t *ldcf = conf;

    ngx_rbtree_init(&ldcf->rbtree, &ldcf->sentinel, ngx_squ_dbd_insert_value);

    return NGX_CONF_OK;
}


static char *
ngx_squ_dbd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_squ_dbd_conf_t *ldcf = conf;

    ngx_str_t   *value, str;
    ngx_uint_t   i;

    if (ldcf->name.data != NULL) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "name=", 5) == 0) {
            ldcf->name.len = value[i].len - 5;
            ldcf->name.data = value[i].data + 5;
            continue;
        }

        if (ngx_strncmp(value[i].data, "size=", 5) == 0) {
            str.len = value[i].len - 5;
            str.data = value[i].data + 5;
            ldcf->size = ngx_parse_size(&str);
            if (ldcf->size == (size_t) NGX_ERROR) {
                goto invalid;
            }
            continue;
        }

        goto invalid;
    }

    if (ldcf->name.data == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the directive \"squ_dbd\" must be specified");
        return NGX_CONF_ERROR;
    }

    ngx_conf_init_size_value(ldcf->size, 1024 * 512);

    ldcf->zone = ngx_shared_memory_add(cf, &ldcf->name, ldcf->size,
                                       &ngx_squ_dbd_module);
    if (ldcf->zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (ldcf->zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate squ dbd name \"%V\"", &ldcf->name);
        return NGX_CONF_ERROR;
    }

    ldcf->zone->init = ngx_squ_dbd_init;
    ldcf->zone->data = ldcf;

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid parameter \"%V\" in squ_dbd", &value[i]);

    return NGX_CONF_ERROR;
}
