#include <glib.h>

struct test {
  gchar *param_ok;   /**< This is a param */
  struct {
    gchar *a; /**< Commented */
    gchar *b;
  } oops;
};
