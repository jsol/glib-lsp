#include <glib.h>
#include <json-glib/json-glib.h>

#include "message.h"

#define INITIALIZE  "initialize"
#define INITIALIZED "initialized"
#define DIDOPEN     "textDocument/didOpen"
#define DIDCHANGE   "textDocument/didChange"
#define DIDSAVE     "textDocument/didSave"
#define DIAGNOSTIC  "textDocument/diagnostic"

static gchar *
get_string_from_json_object(JsonObject *object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object(json_node_alloc(), object);
  generator = json_generator_new();
  json_generator_set_root(generator, root);
  text = json_generator_to_data(generator, NULL);

  /* Release everything */
  g_object_unref(generator);
  json_node_free(root);

  return text;
}

static gboolean
parse_range(JsonObject *params, struct range *r)
{
  JsonObject *range;
  JsonObject *start;
  JsonObject *end;

  g_assert(r);
  g_assert(params);

  range = json_object_get_object_member(params, "range");
  start = json_object_get_object_member(range, "start");
  end = json_object_get_object_member(range, "end");

  r->start.character = json_object_get_int_member(start, "character");
  r->start.line = json_object_get_int_member(start, "line");
  r->end.character = json_object_get_int_member(end, "character");
  r->end.line = json_object_get_int_member(end, "line");

  return TRUE;
}

static gboolean
parse_document(JsonObject *params, struct document_change *d)
{
  JsonObject *text_document;

  g_assert(params);
  g_assert(d);

  text_document = json_object_get_object_member(params, "textDocument");

  d->uri = g_strdup(json_object_get_string_member(text_document, "uri"));
  if (json_object_has_member(text_document, "languageId")) {
    d->language = g_strdup(
      json_object_get_string_member(text_document, "languageId"));
  }
  if (json_object_has_member(text_document, "version")) {
    d->version = json_object_get_int_member(text_document, "version");
  }
  if (json_object_has_member(text_document, "text")) {
    d->text = g_strdup(json_object_get_string_member(text_document, "text"));
  } else {
    if (json_object_has_member(params, "contentChanges")) {
      JsonArray *a = json_object_get_array_member(params, "contentChanges");
      if (json_array_get_length(a) == 1) {
        JsonObject *cc;
        cc = json_array_get_object_element(a, 0);

        d->text = g_strdup(json_object_get_string_member(cc, "text"));
      }
    }
  }

  return TRUE;
}

static message_t *
parse_request(JsonObject *root, GError **err)
{
  const gchar *method;
  message_t *msg = NULL;

  g_assert(root);
  g_assert(err == NULL || *err == NULL);

  method = json_object_get_string_member(root, "method");

  if (g_strcmp0(method, INITIALIZE) == 0) {
    JsonObject *client;
    JsonObject *params;
    params = json_object_get_object_member(root, "params");
    client = json_object_get_object_member(params, "clientInfo");

    g_assert(client);

    msg = g_malloc0(sizeof(*msg));
    msg->type = MESSAGE_TYPE_INITIALIZE;
    msg->data.init.id = json_object_get_int_member(root, "id");
    msg->data.init.client_name = g_strdup(
      json_object_get_string_member(client, "name"));
    msg->data.init.client_version = g_strdup(
      json_object_get_string_member(client, "version"));
    return msg;
  }
  if (g_strcmp0(method, DIAGNOSTIC) == 0) {
    JsonObject *params;

    params = json_object_get_object_member(root, "params");
    msg = g_malloc0(sizeof(*msg));
    msg->type = MESSAGE_TYPE_DIAGNOSTIC;
    msg->data.diagnostic.id = json_object_get_int_member(root, "id");
    parse_document(params, &msg->data.diagnostic.document);
    parse_range(params, &msg->data.diagnostic.range);
    return msg;
  }

  g_set_error(err, MESSAGE_ERROR, -1, "Invalid request method: %s", method);

  return NULL;
}

static message_t *
parse_notification(JsonObject *root, GError **err)
{
  const gchar *method;
  message_t *msg = NULL;
  JsonObject *params;

  g_assert(root);
  g_assert(err == NULL || *err == NULL);

  method = json_object_get_string_member(root, "method");
  if (g_strcmp0(method, INITIALIZED) == 0) {
    msg = g_malloc0(sizeof(*msg));
    msg->type = MESSAGE_TYPE_INITIALIZED;
    return msg;
  }

  if (g_strcmp0(method, DIDCHANGE) == 0) {
    params = json_object_get_object_member(root, "params");
    msg = g_malloc0(sizeof(*msg));
    msg->type = MESSAGE_TYPE_CHANGE;
    parse_document(params, &msg->data.change);

    return msg;
  }
  if (g_strcmp0(method, DIDOPEN) == 0) {
    params = json_object_get_object_member(root, "params");
    msg = g_malloc0(sizeof(*msg));
    msg->type = MESSAGE_TYPE_OPEN;
    parse_document(params, &msg->data.change);
    return msg;
  }

  g_set_error(err, MESSAGE_ERROR, -1, "Invalid notification method: %s", method);

  return NULL;
}

message_t *
message_parse(const gchar *json, gsize len, GError **err)
{
  message_t *msg = NULL;
  JsonParser *parser = NULL;
  JsonNode *root;
  JsonObject *obj;

  parser = json_parser_new();

  if (!json_parser_load_from_data(parser, json, len, err)) {
    goto out;
  }

  root = json_parser_get_root(parser);
  if (root == NULL) {
    g_set_error(err, MESSAGE_ERROR, -1,
                "Failed to get root node from string: %s", json);
    goto out;
  }

  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_set_error(err, MESSAGE_ERROR, -1,
                "Root node is not an object in string: %s", json);
    goto out;
  }

  obj = json_node_get_object(root);

  /* Here the actual message content is parsed, if it is of interest: */
  if (json_object_has_member(obj, "jsonrpc") &&
      json_object_has_member(obj, "method")) {
    if (json_object_has_member(obj, "id")) {
      msg = parse_request(obj, err);
    } else {
      msg = parse_notification(obj, err);
    }
  }

  if (msg == NULL && (err != NULL && *err == NULL)) {
    g_set_error(err, MESSAGE_ERROR, -1, "Did not recognize: %s", json);
    goto out;
  }
  /* Fall through */
out:
  return msg;
}

static JsonObject *
get_response_root(gint64 id)
{
  JsonObject *root;

  root = json_object_new();

  json_object_set_string_member(root, "jsonrpc", "2.0");
  json_object_set_int_member(root, "id", id);

  return root;
}

static JsonObject *
range_to_json(struct range *r)
{
  JsonObject *root;
  JsonObject *start;
  JsonObject *end;

  g_assert(r);

  root = json_object_new();
  start = json_object_new();
  end = json_object_new();

  json_object_set_int_member(start, "line", r->start.line);
  json_object_set_int_member(start, "character", r->start.character);
  json_object_set_int_member(end, "line", r->end.line);
  json_object_set_int_member(end, "character", r->end.character);

  json_object_set_object_member(root, "end", end);
  json_object_set_object_member(root, "start", start);

  return root;
}

struct problem *
message_problem_new(gint severity,
                    gint64 start_line,
                    gint64 start_char,
                    gint64 end_line,
                    gint64 end_char,
                    const gchar *format,
                    ...)
{
  struct problem *p;
  va_list args;

  p = g_malloc0(sizeof(*p));
  va_start(args, format);
  p->msg = g_strdup_vprintf(format, args);
  va_end(args);

  p->range.start.line = start_line;
  p->range.start.character = start_char;
  p->range.end.line = end_line;
  p->range.end.character = end_char;
  p->severity = severity;

  return p;
}

static JsonArray *
diagnostics_to_json(GList *issues)
{
  JsonArray *dia;
  dia = json_array_new();

  while (issues != NULL) {
    struct problem *p = (struct problem *) issues->data;
    JsonObject *range;
    JsonObject *d;

    range = range_to_json(&p->range);
    d = json_object_new();

    json_object_set_object_member(d, "range", range);
    json_object_set_int_member(d, "severity", p->severity);
    json_object_set_string_member(d, "message", p->msg);

    json_array_add_object_element(dia, d);
    issues = issues->next;
  }
  return dia;
}

gchar *
message_diagnostic(gint64 id, const gchar *uri, GList *issues)
{
  JsonObject *root;
  JsonObject *params;
  JsonArray *dia;
  gchar *res;

  params = json_object_new();
  if (id > 0) {
    root = get_response_root(id);
    dia = diagnostics_to_json(issues);
    json_object_set_string_member(root, "kind", "full");
    json_object_set_array_member(root, "items", dia);

  } else {
    root = json_object_new();
    dia = diagnostics_to_json(issues);
    json_object_set_string_member(root, "jsonrpc", "2.0");
    json_object_set_string_member(root, "method",
                                  "textDocument/publishDiagnostics");
    json_object_set_string_member(params, "uri", uri);
    json_object_set_array_member(params, "diagnostics", dia);
    json_object_set_object_member(root, "params", params);
  }

  res = get_string_from_json_object(root);

  json_object_unref(root);

  return res;
}

gchar *
message_init_response(gint64 id,
                      struct init_config *c,
                      const gchar *server_name,
                      const gchar *version)
{
  g_return_val_if_fail(c != NULL, NULL);

  JsonObject *root;
  JsonObject *result;
  JsonObject *server_info;
  JsonObject *capabilities;
  JsonObject *code_action;
  JsonArray *kinds;
  gchar *res;

  root = get_response_root(id);
  result = json_object_new();
  server_info = json_object_new();
  capabilities = json_object_new();

  json_object_set_string_member(server_info, "name", server_name);
  json_object_set_string_member(server_info, "version", version);

  json_object_set_int_member(capabilities, "textDocumentSync", c->sync);
  json_object_set_string_member(capabilities, "workspaceFolders", "utf-16");

  code_action = json_object_new();

  kinds = json_array_new();

  json_array_add_string_element(kinds, "quickfix");
  json_object_set_boolean_member(code_action, "workDoneProgress", FALSE);
  json_object_set_array_member(code_action, "codeActionKinds", kinds);
  json_object_set_object_member(capabilities, "codeActionProvider", code_action);

  /* JsonObject *diagnostic;
   diagnostic = json_object_new();
   json_object_set_boolean_member(diagnostic, "interFileDependencies", FALSE);
  json_object_set_object_member(capabilities, "diagnosticProvider", diagnostic);

   JsonObject *workspace;
   workspace = json_object_new();
   JsonObject *wsf;
   wsf = json_object_new();

   json_object_set_boolean_member(wsf, "changeNotifications", TRUE);
   json_object_set_boolean_member(wsf, "supported", TRUE);
   json_object_set_object_member(workspace, "workspaceFolders", wsf);
   json_object_set_object_member(capabilities, "workspace", workspace);
 */

  json_object_set_object_member(result, "serverInfo", server_info);
  json_object_set_object_member(result, "capabilities", capabilities);

  json_object_set_object_member(root, "result", result);

  res = get_string_from_json_object(root);

  json_object_unref(root);

  return res;
}

void
message_free(message_t *msg)
{
  if (msg == NULL) {
    return;
  }
  switch (msg->type) {
  case MESSAGE_TYPE_INITIALIZED:
    break;
  case MESSAGE_TYPE_INITIALIZE:
    g_free(msg->data.init.client_name);
    g_free(msg->data.init.client_version);
    break;
  case MESSAGE_TYPE_OPEN:
    g_free(msg->data.open.language);
    g_free(msg->data.open.uri);
    g_free(msg->data.open.text);
    break;
  case MESSAGE_TYPE_CHANGE:
    g_free(msg->data.open.language);
    g_free(msg->data.open.uri);
    g_free(msg->data.open.text);
    break;
  case MESSAGE_TYPE_DIAGNOSTIC:
    g_free(msg->data.diagnostic.document.uri);
    g_free(msg->data.diagnostic.document.text);
    break;
  case MESSAGE_TYPE_SAVE:
    /* ignore */
  }
}

void
message_problem_free(gpointer p)
{
  struct problem *ctx = (struct problem *) p;
  if (p == NULL) {
    return;
  }

  g_free(ctx->msg);
  g_free(p);
}

G_DEFINE_QUARK("message-error-quark", message_error)
