#pragma once
#include <glib.h>
#include <tree_sitter/api.h>

#define SYMBOL_IDENTIFIER          1
#define SYMBOL_COMMENT             160
#define SYMBOL_FUNCTION            196
#define SYMBOL_DECLARATION         198
#define SYMBOL_POINTER_DECLARATION 226
#define SYMBOL_FUNC_DECLARATION    230
#define SYMBOL_BODY                241
#define SYMBOL_STORAGE_SPEC        242
#define SYMBOL_STRUCT_SPEC         249
#define SYMBOL_FIELD_DECL_LIST     251
#define SYMBOL_FIELD_DECL          253
#define SYMBOL_PARAM_DECLARATION   260
#define SYMBOL_CAST_EXPRESSION     292
#define SYMBOL_CALL_EXPRESSION     299
#define SYMBOL_ARG_LIST            309
#define SYMBOL_FIELD_IDENT         360
#define SYMBOL_TYPE                362

gboolean parse_utils_node_eq(const gchar *content,
                             TSNode *node,
                             const gchar *needle);

gchar *parse_utils_node_get_string(const gchar *content, TSNode *node);

gboolean parse_utils_is_function_static(const gchar *content, TSNode p);

gboolean parse_utils_parameter_is_pointer(const gchar *content,
                                          TSNode param,
                                          gchar **name);

gboolean parse_utils_parameter_is_unused(const gchar *content, TSNode param);

gboolean parse_utils_is_gobject_cast(const gchar *content,
                                      TSNode decl,
                                      gchar **to,
                                      gchar **from);

TSNode parse_utils_get_first_node_id(TSNode check, guint id, gboolean *found);
