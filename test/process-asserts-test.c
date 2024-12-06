#include <glib.h>
#include <json-glib/json-glib.h>

#include "message.h"
#include "parser.h"
#include "process_asserts.h"

struct fixture {
  parser_t *parser;
  GList *issues;
};

static GList *
load_issues(const gchar *file)
{
  JsonParser *parser;
  JsonNode *root;
  GList *res = NULL;
  JsonArray *list;
  GError *lerr = NULL;

  parser = json_parser_new();

  if (!json_parser_load_from_file(parser, file, &lerr)) {
    g_warning("Unable to load issues file: %s",
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    g_clear_object(&parser);
    return NULL;
  }

  root = json_parser_get_root(parser);

  if (!JSON_NODE_HOLDS_ARRAY(root)) {
    goto out;
  }

  list = json_node_get_array(root);

  for (guint i = 0; i < json_array_get_length(list); i++) {
    struct problem *p;
    JsonObject *j;
    JsonObject *start;
    JsonObject *end;

    j = json_array_get_object_element(list, i);
    start = json_object_get_object_member(j, "start");
    end = json_object_get_object_member(j, "end");

    p = message_problem_new(3, json_object_get_int_member(start, "line"),
                            json_object_get_int_member(start, "character"),
                            json_object_get_int_member(end, "line"),
                            json_object_get_int_member(end, "character"),
                            json_object_get_string_member(j, "msg"));

    res = g_list_prepend(res, p);
  }

  res = g_list_reverse(res);
  /* Fall through */

out:
  g_object_unref(parser);

  return res;
}

static void
fixture_setup(struct fixture *f, gconstpointer user_data)
{
  gchar *name = (gchar *) user_data;
  message_t *msg;
  GHashTable *ht;
  gchar *content = NULL;
  gchar *codefile;
  gchar *issuesfile;
  GError *lerr = NULL;

  g_assert(f);
  codefile = g_strdup_printf("%s/assert/%s.c", g_getenv("G_TEST_SRCDIR"), name);
  issuesfile = g_strdup_printf("%s/assert/%s.json", g_getenv("G_TEST_SRCDIR"),
                               name);

  if (!g_file_get_contents(codefile, &content, NULL, &lerr)) {
    g_warning("Failed to load test file: %s",
              lerr ? lerr->message : "No error msg");
    g_clear_error(&lerr);
    return;
  }

  msg = g_malloc0(sizeof(*msg));
  msg->type = MESSAGE_TYPE_OPEN;
  msg->data.open.uri = g_strdup(codefile);
  msg->data.open.text = content;
  msg->data.open.version = 1;
  msg->data.open.language = g_strdup("c");

  ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  f->parser = parser_new(msg, ht);
  f->issues = load_issues(issuesfile);
  g_free(codefile);
  g_free(issuesfile);
}

static void
fixture_teardown(struct fixture *f, G_GNUC_UNUSED gconstpointer user_data)
{
  parser_unref(f->parser);
  g_list_free_full(f->issues, message_problem_free);
}

static void
assert_problem(struct problem *exp, struct problem *act)
{
  g_assert_true(act != NULL);
  g_assert_cmpstr(exp->msg, ==, act->msg);
  g_assert_cmpint(exp->severity, ==, act->severity);
  g_assert_cmpint(exp->range.start.line, ==, act->range.start.line);
  g_assert_cmpint(exp->range.start.character, ==, act->range.start.character);
  g_assert_cmpint(exp->range.end.line, ==, act->range.end.line);
  g_assert_cmpint(exp->range.end.character, ==, act->range.end.character);
}

void
test_assert(struct fixture *f, gconstpointer user_data)
{
  parser_t *parser = (parser_t *) f;
  GList *actual;
  GList *check;

  g_assert(parser);

  actual = process_asserts(f->parser, NULL);
  check = actual;
  for (GList *exp = f->issues; exp != NULL; exp = exp->next) {
    assert_problem(exp->data, check->data);
    if (check != NULL) {
      check = check->next;
    }
  }

  g_list_free_full(actual, message_problem_free);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.

  g_test_add("/message/process/assert/missing", struct fixture, "missing",
             fixture_setup, test_assert, fixture_teardown);
  g_test_add("/message/process/assert/gpointer", struct fixture, "gpointer",
             fixture_setup, test_assert, fixture_teardown);

  return g_test_run();
}
