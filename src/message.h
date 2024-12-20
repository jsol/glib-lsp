#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <tree_sitter/api.h>
#include "glibconfig.h"

G_BEGIN_DECLS

enum message_type {
  MESSAGE_TYPE_INITIALIZE = 0,
  MESSAGE_TYPE_INITIALIZED,
  MESSAGE_TYPE_OPEN,
  MESSAGE_TYPE_CHANGE,
  MESSAGE_TYPE_DIAGNOSTIC,
  MESSAGE_TYPE_SAVE
};
#define MESSAGE_ERROR message_error_quark()

struct init_config {
  gint64 sync;
  gboolean hover;
  gboolean definition;
  gboolean codeaction;
  GHashTable *completeprovider;
};

struct document_change {
  gchar *uri;
  gchar *language;
  gint64 version;
  gchar *text;
};

struct range {
  struct {
    gint64 character;
    gint64 line;
  } start;
  struct {
    gint64 character;
    gint64 line;
  } end;
};

struct problem {
  struct range range;
  gint severity;
  gchar *msg;
};

typedef struct message {
  enum message_type type;
  union {
    struct {
      gint64 id;
      gchar *client_name;
      gchar *client_version;
    } init;

    struct document_change open;
    struct document_change change;

    struct {
      gint64 id;
      struct document_change document;
      struct range range;
    } diagnostic;
  } data;
} message_t;

struct problem *message_problem_new(gint severity,
                                    TSNode *start,
                                    TSNode *end,
                                    const gchar *format,
                                    ...);
struct problem *message_problem_new_pos(gint severity,
                                        guint64 start_line,
                                        guint64 start_char,
                                        guint64 end_line,
                                        guint64 end_char,
                                        const gchar *format,
                                        ...);

void message_problem_free(gpointer p);

message_t *message_parse(const gchar *json, gsize len, GError **err);

gchar *message_diagnostic(gint64 id, const gchar *uri, GList *issues);
gchar *message_init_response(gint64 id,
                             struct init_config *c,
                             const gchar *server_name,
                             const gchar *version);
void message_free(message_t *msg);

GQuark message_error_quark(void);

G_END_DECLS
