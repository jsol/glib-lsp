#include <glib.h>

#include "message.h"
#include "parser.h"
#include "process_init.h"

GList *
process_init_do(parser_t *parser, struct process_ctx *ctx)
{
  GList *list = NULL;
  g_return_val_if_fail(ctx != NULL, NULL);

  if (parser->message->type == MESSAGE_TYPE_INITIALIZE) {
    gchar *resp;
    resp = message_init_response(parser->message->data.init.id, &ctx->conf,
                                 ctx->name, ctx->version);
    list = g_list_prepend(list, resp);
  }
  return list;
}
