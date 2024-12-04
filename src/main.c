#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib.h>
#include <stdio.h>
#include <sysexits.h>

#include "message.h"
#include "processor.h"
#include "rpc.h"

/* Processors */
#include "process_init.h"
#include "process_asserts.h"

static void
add_processors(processor_t *p)
{
  g_assert(p);
  struct process_ctx  *ctx;

  ctx = g_malloc0(sizeof(*ctx));
  ctx->name = "glib_lsp";
  ctx->version = "0.0.1";
  ctx->conf.sync = 1;
  /* TODO: Check config before adding processors */
  processor_add_process(p, process_init_do, ctx);
  processor_add_process(p, process_asserts, ctx);
}

int
main(int argc, char *argv[])
{
  GError *err = NULL;
  gint ret_val = 0;
  GInputStream *stdinput;
  GDataInputStream *in;
  message_t *msg;
  processor_t *processor;

  g_log_set_writer_func(g_log_writer_journald, NULL, NULL);
  g_message("hello");

  stdinput = g_unix_input_stream_new(fileno(stdin), FALSE);
  processor = processor_new(g_unix_output_stream_new(fileno(stdout), FALSE));

  if (stdinput == NULL) {
    g_warning("Could not open input stream: %s", err->message);
    ret_val = 1;
    goto out;
  }

  add_processors(processor);

  in = g_data_input_stream_new(stdinput);

  while (TRUE) {
    msg = rpc_read_message(in, &err);
    if (msg == NULL) {
      if (err == NULL) {
        break;
      }
      g_warning("Error reading message: %s", err != NULL ? err->message : "No error message");
      g_clear_error(&err);
      continue;
    }
    if (!processor_handle_message(processor, msg, &err)) {
      g_warning("Error processing message: %s", err->message);
      g_clear_error(&err);
      message_free(msg);
    }
  }
  g_clear_object(&in);

out:
  return ret_val;
}
