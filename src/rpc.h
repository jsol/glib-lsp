#pragma once

#include <gio/gio.h>
#include <glib.h>
#include "glibconfig.h"

#include "message.h"
G_BEGIN_DECLS

message_t *rpc_read_message(GDataInputStream *in, GError **err);

gboolean rpc_write_msg(GOutputStream *out, const gchar *json, GError **err);

G_END_DECLS
