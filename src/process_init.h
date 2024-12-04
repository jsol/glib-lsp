#pragma once

#include <glib.h>

#include "processor.h"

GList *process_init_do(parser_t *parser, struct process_ctx * user_data);

G_END_DECLS
