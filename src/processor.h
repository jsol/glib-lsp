#pragma once

#include <gio/gio.h>
#include <glib.h>
#include "glibconfig.h"

#include "message.h"
#include "parser.h"

G_BEGIN_DECLS

struct process_ctx {
  /* These are set by the init */
  gchar *version;
  gchar *name;
  struct init_config conf;
};

typedef struct processor processor_t;

typedef GList * (*process_func_t)(parser_t*, struct process_ctx *);

processor_t *processor_new(GOutputStream *out);

gboolean processor_handle_message(processor_t *ctx, message_t *msg, GError **err);
void
processor_add_process(processor_t *ctx, process_func_t func, gpointer user_data);

G_END_DECLS
