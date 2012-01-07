
/*
 * Copyright (C) Ngwsx
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_squ_axis2c.h>


static int ngx_squ_xml_parse(squ_State *l);
static void ngx_squ_xml_parse_children(squ_State *l, axutil_env_t *env,
    axiom_node_t *parent, axiom_element_t *parent_elem);

static int ngx_squ_xml_serialize(squ_State *l);
static void ngx_squ_xml_serialize_tables(squ_State *l, axutil_env_t *env,
    axiom_node_t *parent);
static void ngx_squ_xml_serialize_table(squ_State *l, axutil_env_t *env,
    axiom_node_t *parent, char *name, int index);

static ngx_int_t ngx_squ_xml_module_init(ngx_cycle_t *cycle);


static ngx_squ_const_t  ngx_squ_xml_consts[] = {
    { NULL, 0 }
};


static squL_Reg  ngx_squ_xml_methods[] = {
    { "parse", ngx_squ_xml_parse },
    { "serialize", ngx_squ_xml_serialize },
    { NULL, NULL }
};


ngx_module_t  ngx_squ_xml_module = {
    NGX_MODULE_V1,
    NULL,                                  /* module context */
    NULL,                                  /* module directives */
    NGX_CORE_MODULE,                       /* module type */
    NULL,                                  /* init master */
    ngx_squ_xml_module_init,               /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static int
ngx_squ_xml_parse(squ_State *l)
{
    char                  *name;
    ngx_str_t              xml;
    axiom_node_t          *node;
    axutil_env_t          *env;
    axutil_log_t          *log;
    axutil_error_t        *error;
    axiom_element_t       *elem;
    axiom_document_t      *doc;
    ngx_squ_thread_t      *thr;
    axutil_allocator_t    *a;
    axiom_xml_reader_t    *reader;
    axiom_stax_builder_t  *builder;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ xml parse");

    xml.data = (u_char *) squL_checklstring(l, -1, &xml.len);

    squ_createtable(l, 2, 2);

    a = ngx_squ_axis2c_allocator_create(thr);
    log = ngx_squ_axis2c_log_create(thr);
    error = axutil_error_create(a);
    env = axutil_env_create_with_error_log(a, error, log);

    reader = axiom_xml_reader_create_for_memory(env, xml.data, xml.len, NULL,
                                                AXIS2_XML_PARSER_TYPE_BUFFER);
    builder = axiom_stax_builder_create(env, reader);
    doc = axiom_stax_builder_get_document(builder, env);

    node = axiom_document_get_root_element(doc, env);

    if (axiom_node_get_node_type(node, env) != AXIOM_ELEMENT) {
        return 1;
    }

    elem = axiom_node_get_data_element(node, env);
    name = axiom_element_get_localname(elem, env);

    squ_newtable(l);

    ngx_squ_xml_parse_children(l, env, node, elem);

    squ_setfield(l, -2, name);

    return 1;
}


static void
ngx_squ_xml_parse_children(squ_State *l, axutil_env_t *env,
    axiom_node_t *parent, axiom_element_t *parent_elem)
{
    int                              n;
    char                            *uri, *prefix, *name, *text, *value;
    axiom_node_t                    *node;
    axutil_hash_t                   *attrs;
    axiom_element_t                 *elem;
    ngx_squ_thread_t                *thr;
    axiom_attribute_t               *attr;
    axiom_namespace_t               *ns;
    axutil_hash_index_t             *hi;
    axiom_child_element_iterator_t  *it;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ xml parse children");

    it = axiom_element_get_child_elements(parent_elem, env, parent);
    if (it == NULL) {
        return;
    }

    n = 1;

    do {

        node = axiom_child_element_iterator_next(it, env);
        elem = axiom_node_get_data_element(node, env);
        name = axiom_element_get_localname(elem, env);

        squ_createtable(l, 2, 4);

        squ_pushstring(l, name);
        squ_setfield(l, -2, "name");

        ns = axiom_element_get_namespace(elem, env, node);
        if (ns != NULL) {
            uri = axiom_namespace_get_uri(ns, env);
            if (uri != NULL) {
                squ_pushstring(l, uri);
                squ_setfield(l, -2, "uri");
            }

            prefix = axiom_namespace_get_prefix(ns, env);
            if (prefix != NULL) {
                squ_pushstring(l, prefix);
                squ_setfield(l, -2, "prefix");
            }
        }

        attrs = axiom_element_get_all_attributes(elem, env);
        if (attrs != NULL) {
            squ_newtable(l);

            hi = axutil_hash_first(attrs, env);

            do {
                if (hi == NULL) {
                    break;
                }

                axutil_hash_this(hi, NULL, NULL, (void **) &attr);

                name = axiom_attribute_get_localname(attr, env);
                value = axiom_attribute_get_value(attr, env);

                squ_pushstring(l, value);
                squ_setfield(l, -2, name);

                hi = axutil_hash_next(env, hi);
            } while (1);

            squ_setfield(l, -2, "attributes");
        }

        text = axiom_element_get_text(elem, env, node);
        if (text != NULL) {
            squ_pushstring(l, text);
            squ_setfield(l, -2, "text");

        } else {
            squ_newtable(l);

            ngx_squ_xml_parse_children(l, env, node, elem);

            squ_setfield(l, -2, "children");
        }

        squ_setfield(l, -2, name);

        squ_getfield(l, -1, name);
        squ_rawseti(l, -2, n++);

    } while (axiom_child_element_iterator_has_next(it, env) == AXIS2_TRUE);
}


static int
ngx_squ_xml_serialize(squ_State *l)
{
    int                     top;
    char                   *uri, *prefix;
    axiom_node_t           *node;
    axutil_env_t           *env;
    axutil_log_t           *log;
    axutil_error_t         *error;
    axiom_output_t         *output;
    ngx_squ_thread_t       *thr;
    axiom_namespace_t      *ns;
    axiom_soap_body_t      *body;
    axutil_allocator_t     *a;
    axiom_xml_writer_t     *writer;
    axiom_soap_header_t    *header;
    axiom_soap_envelope_t  *envelope;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ xml serialize");

    top = squ_gettop(l);

    if (!squ_istable(l, top)) {
        return squL_error(l, "invalid argument, must be a table");
    }

    a = ngx_squ_axis2c_allocator_create(thr);
    log = ngx_squ_axis2c_log_create(thr);
    error = axutil_error_create(a);
    env = axutil_env_create_with_error_log(a, error, log);

    squ_getfield(l, top, "uri");
    uri = (char *) squL_optstring(l, -1,
                                  "http://www.w3.org/2003/05/soap-envelope");

    squ_getfield(l, top, "prefix");
    prefix = (char *) squL_optstring(l, -1, "soap");

    ns = axiom_namespace_create(env, uri, prefix);
    envelope = axiom_soap_envelope_create(env, ns);

    squ_getfield(l, top, "header");
    if (!squ_isnil(l, -1)) {
        if (!squ_istable(l, -1)) {
            return squL_error(l, "the value of \"header\" must be a table");
        }

        header = axiom_soap_header_create_with_parent(env, envelope);
        node = axiom_soap_header_get_base_node(header, env);

        ngx_squ_xml_serialize_tables(l, env, node);
    }

    squ_getfield(l, top, "body");
    if (!squ_isnil(l, -1)) {
        if (!squ_istable(l, -1)) {
            return squL_error(l, "the value of \"body\" must be a table");
        }

        body = axiom_soap_body_create_with_parent(env, envelope);
        node = axiom_soap_body_get_base_node(body, env);

        ngx_squ_xml_serialize_tables(l, env, node);
    }

    squ_settop(l, top);

    writer = axiom_xml_writer_create_for_memory(env, NULL, AXIS2_FALSE,
                                                AXIS2_FALSE,
                                                AXIS2_XML_PARSER_TYPE_BUFFER);
    output = axiom_output_create(env, writer);
    axiom_soap_envelope_serialize(envelope, env, output, AXIS2_FALSE);

    squ_pushstring(l, axiom_xml_writer_get_xml(writer, env));

    return 1;
}


static void
ngx_squ_xml_serialize_tables(squ_State *l, axutil_env_t *env,
    axiom_node_t *parent)
{
    int                type, n, i, index;
    char              *name;
    ngx_squ_thread_t  *thr;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ xml serialize tables");

    squ_pushnil(l);

    while (squ_next(l, -2)) {
        type = squ_type(l, -2);
        if (type == SQU_TNUMBER) {
            squ_pop(l, 1);
            continue;
        }

        name = (char *) squL_checkstring(l, -2);

        if (!squ_istable(l, -1)) {
            squL_error(l, "the value of \"%s\" must be a table", name);
        }

        index = squ_gettop(l);

        ngx_squ_xml_serialize_table(l, env, parent, name, index);

        squ_pop(l, 1);
    }

    n = squ_objlen(l, -1);

    for (i = 1; i <= n; i++) {
        squ_rawgeti(l, -1, i);

        /* TODO */

        if (!squ_istable(l, -1)) {
            squL_error(l, "must be a table");
        }

        squ_getfield(l, -1, "name");
        name = (char *) squL_checkstring(l, -1);

        index = squ_gettop(l) - 1;

        ngx_squ_xml_serialize_table(l, env, parent, name, index);

        squ_pop(l, 2);
    }
}


static void
ngx_squ_xml_serialize_table(squ_State *l, axutil_env_t *env,
    axiom_node_t *parent, char *name, int index)
{
    int                 top;
    char               *uri, *prefix, *text, *value;
    axiom_node_t       *node;
    axiom_element_t    *elem;
    ngx_squ_thread_t   *thr;
    axiom_namespace_t  *ns;
    axiom_attribute_t  *attr;

    thr = ngx_squ_thread(l);

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, thr->log, 0, "squ xml serialize table");

    top = squ_gettop(l);

    squ_getfield(l, index, "uri");
    uri = (char *) squL_optstring(l, -1, NULL);

    squ_getfield(l, index, "prefix");
    prefix = (char *) squL_optstring(l, -1, NULL);

    if (uri != NULL || prefix != NULL) {
        ns = axiom_namespace_create(env, uri, prefix);

    } else {
        ns = NULL;
    }

    elem = axiom_element_create(env, parent, name, ns, &node);

    squ_getfield(l, index, "attributes");
    if (!squ_isnil(l, -1)) {
        if (!squ_istable(l, -1)) {
            squL_error(l, "the value of \"attributes\" must be a table");
        }

        squ_pushnil(l);

        while (squ_next(l, -2)) {
            name = (char *) squL_checkstring(l, -2);
            value = (char *) squL_checkstring(l, -1);

            attr = axiom_attribute_create(env, name, value, NULL);
            axiom_element_add_attribute(elem, env, attr, node);

            squ_pop(l, 1);
        }
    }

    squ_getfield(l, index, "text");
    text = (char *) squL_optstring(l, -1, NULL);

    if (text != NULL) {
        axiom_element_set_text(elem, env, text, node);
        squ_settop(l, top);
        return;
    }

    squ_getfield(l, index, "children");
    if (!squ_isnil(l, -1)) {
        if (!squ_istable(l, -1)) {
            squL_error(l, "the value of \"children\" must be a table");
        }

        ngx_squ_xml_serialize_tables(l, env, node);
    }

    squ_settop(l, top);
}


static ngx_int_t
ngx_squ_xml_module_init(ngx_cycle_t *cycle)
{
    int              n;
    ngx_squ_conf_t  *scf;

    ngx_log_debug0(NGX_LOG_DEBUG_CORE, cycle->log, 0, "squ xml module init");

    scf = (ngx_squ_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_squ_module);

    squ_getglobal(scf->l, NGX_SQU_TABLE);

    n = sizeof(ngx_squ_xml_consts) / sizeof(ngx_squ_const_t) - 1;
    n += sizeof(ngx_squ_xml_methods) / sizeof(squL_Reg) - 1;

    squ_createtable(scf->l, 0, n);

    for (n = 0; ngx_squ_xml_consts[n].name != NULL; n++) {
        squ_pushinteger(scf->l, ngx_squ_xml_consts[n].value);
        squ_setfield(scf->l, -2, ngx_squ_xml_consts[n].name);
    }

    for (n = 0; ngx_squ_xml_methods[n].name != NULL; n++) {
        squ_pushcfunction(scf->l, ngx_squ_xml_methods[n].func);
        squ_setfield(scf->l, -2, ngx_squ_xml_methods[n].name);
    }

    squ_setfield(scf->l, -2, "xml");

    squ_pop(scf->l, 1);

    axutil_error_init();

    return NGX_OK;
}
