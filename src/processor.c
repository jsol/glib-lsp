#include <gio/gio.h>
#include <glib.h>

#include "message.h"
#include "parser.h"
#include "processor.h"
#include "rpc.h"

#define MAX_THREADS 10

struct proc_ctx {
  process_func_t func;
  gpointer user_data;
};

struct processor {
  GOutputStream *out;
  GAsyncQueue *messages;
  GThreadPool *pool;
  GThread *writer;
  GPtrArray *processors;
  GHashTable *files;
  GMutex file_lock;
  GCond file_cond;
};

static void
thread_func(gpointer data, gpointer user_data)
{
  processor_t *ctx = (processor_t *) user_data;
  parser_t *parser = (parser_t *) data;
  GList *dia = NULL;
  gchar *msg;

  g_assert(data);
  g_assert(user_data);

  for (guint i = 0; i < ctx->processors->len; i++) {
    struct proc_ctx *current;
    GList *resp;

    current = g_ptr_array_index(ctx->processors, i);
    resp = current->func(parser, current->user_data);
    dia = g_list_concat(dia, resp);
  }
  g_message("Handled message of type %d", parser->message->type);

  if (parser->message->type == MESSAGE_TYPE_DIAGNOSTIC) {
    g_message("Sending diagnostics: %d", g_list_length(dia));
    msg = message_diagnostic(parser->message->data.diagnostic.id, parser->file,
                             dia);
    g_async_queue_push(ctx->messages, msg);
    g_list_free_full(dia, message_problem_free);
  }
  if (parser->message->type == MESSAGE_TYPE_OPEN ||
      parser->message->type == MESSAGE_TYPE_CHANGE) {
    g_message("Sending notification diagnostics: %d", g_list_length(dia));
    msg = message_diagnostic(0, parser->file, dia);
    g_async_queue_push(ctx->messages, msg);
    g_list_free_full(dia, message_problem_free);
  }

  if (parser->message->type == MESSAGE_TYPE_INITIALIZE) {
    g_async_queue_push(ctx->messages, g_strdup(dia->data));
    g_list_free_full(dia, g_free);
  }

  parser_unref(parser);
}

static gpointer
message_writer(gpointer data)
{
  processor_t *ctx = (processor_t *) data;
  gchar *resp;
  GError *lerr = NULL;

  g_assert(data);

  while (TRUE) {
    resp = g_async_queue_pop(ctx->messages);

    if (!rpc_write_msg(ctx->out, resp, &lerr)) {
      g_warning("Error writing message: %s", lerr->message);
      g_clear_error(&lerr);
    }
    g_free(resp);
  }

  return NULL;
}

processor_t *
processor_new(GOutputStream *out)
{
  processor_t *ctx;

  g_return_val_if_fail(out != NULL, NULL);

  ctx = g_malloc0(sizeof(*ctx));

  ctx->out = g_object_ref(out);
  ctx->messages = g_async_queue_new();
  ctx->pool = g_thread_pool_new(thread_func, ctx, MAX_THREADS, FALSE, NULL);

  ctx->writer = g_thread_new("response writer", message_writer, ctx);
  ctx->processors = g_ptr_array_new();
  ctx->files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  return ctx;
}

gboolean
processor_handle_message(processor_t *ctx, message_t *msg, GError **err)
{
  struct parser_ctx *parser;

  g_return_val_if_fail(ctx != NULL, FALSE);
  g_return_val_if_fail(msg != NULL, FALSE);
  g_return_val_if_fail(err == NULL || *err == NULL, FALSE);

  parser = parser_new(msg, ctx->files);

  if (!g_thread_pool_push(ctx->pool, parser, err)) {
    return FALSE;
    parser_unref(parser);
  }
  /* Every parsing instance holds their own reference */
  return TRUE;
}

void
processor_add_process(processor_t *ctx, process_func_t func, gpointer user_data)
{
  struct proc_ctx *pctx;

  pctx = g_malloc0(sizeof(*pctx));
  pctx->func = func;
  pctx->user_data = user_data;
  g_ptr_array_add(ctx->processors, pctx);
}
