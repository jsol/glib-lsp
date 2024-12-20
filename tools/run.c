#include <glib.h>
#include <json-glib/json-glib.h>
#include <tree_sitter/api.h>

#include "message.h"
#include "parser.h"
#include "process_asserts.h"
#include "process_midscope.h"
#include "process_comments.h"

int
main(int argc, char *argv[])
{
  gchar *content = NULL;
  gsize len = 0;
  message_t *msg;
  GHashTable *ht;
  parser_t *parser;
  GError *lerr = NULL;
  GList *issues = NULL;

  if (argc < 3) {
    g_print("Add parser to use as the first arg\n");
    return 1;
  }

  if (argc != 3) {
    g_print("Add the file to parse as the second argument\n");
    return 1;
  }

  if (!g_file_get_contents(argv[2], &content, &len, &lerr)) {
    g_print("Could not open file %s: %s\n", argv[2],
            lerr ? lerr->message : "no error message");
    g_clear_error(&lerr);
    return 2;
  }

  msg = g_malloc0(sizeof(*msg));
  msg->type = MESSAGE_TYPE_OPEN;
  msg->data.open.uri = g_strdup(argv[2]);
  msg->data.open.text = content;
  msg->data.open.version = 1;
  msg->data.open.language = g_strdup("c");

  ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  parser = parser_new(msg, ht);

  if (g_strcmp0(argv[1], "midscope") == 0) {
    issues = process_midscope(parser, NULL);
  }else if (g_strcmp0(argv[1], "comment") == 0) {
    issues = process_comments(parser, NULL);
  }else if (g_strcmp0(argv[1], "assert") == 0) {
    issues = process_asserts(parser, NULL);
  } else {
    g_print("Could not find parser %s\n", argv[1]);
    goto out;
  }
  g_print("Results:\n");

  for (GList *r = issues; r != NULL; r = r->next) {
    struct problem *p = r->data;
    g_print("Issue: (%ld:%ld) -> (%ld:%ld): %s\n", p->range.start.line, p->range.start.character, p->range.end.line, p->range.end.character, p->msg);
  }

  g_list_free_full(issues, message_problem_free);

out:
  parser_unref(parser);
}
