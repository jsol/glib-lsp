#include <gio/gio.h>
#include <glib.h>

#include "message.h"
#include "rpc.h"

#define CONTENT_LEN "Content-Length: "
message_t *
rpc_read_message(GDataInputStream *in, GError **err)
{
  gsize len = 0;
  gsize content_len = 0;
  gchar *json = NULL;
  gchar *line = NULL;
  message_t *msg = NULL;

  g_return_val_if_fail(in != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  while (content_len == 0) {
    line = g_data_input_stream_read_line_utf8(in, &len, NULL, err);

    if (line == NULL) {
      g_prefix_error(err, "reading Content-Length: ");
      goto err_out;
    }

    g_message("Read line: %s (%lu)", line, len);

    if (g_str_has_prefix(line, CONTENT_LEN)) {
      content_len = atoi(line + strlen(CONTENT_LEN));
      g_message("Content length: %lu", content_len);
    }
  }

  /* Content-Length: <NUM>\r\n is followed by an additional \r\n */
  line = g_data_input_stream_read_line_utf8(in, &len, NULL, err);

  if (line == NULL) {
    g_prefix_error(err, "reading extra linebreak: ");
    goto err_out;
  }

  json = g_malloc0(content_len + 1);

  if (!g_input_stream_read_all(G_INPUT_STREAM(in), json, content_len, NULL,
                               NULL, err)) {
    g_prefix_error(err, "fetching JSON: ");
    goto err_out;
  }

  g_message("JSON: %s", json);

  msg = message_parse(json, content_len, err);
  if (msg == NULL) {
    g_prefix_error(err, "parsing JSON: ");
    goto err_out;
  }

  return msg;

err_out:
  return NULL;
}

gboolean
rpc_write_msg(GOutputStream *out, const gchar *json, GError **err)
{
  gsize len;
  GString *msg;
  gboolean res;

  g_return_val_if_fail(out != NULL, FALSE);
  g_return_val_if_fail(json != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  len = strlen(json);
  msg = g_string_new(CONTENT_LEN);
  g_string_append_printf(msg, "%lu\r\n\r\n%s", len, json);
  g_message("Sending...");
  g_message("Sending... %s", json);
  g_message("Sending message: %s", msg->str);
  res = g_output_stream_write_all(out, msg->str, msg->len, NULL, NULL, err);
  g_string_free(msg, TRUE);
  return res;
}
