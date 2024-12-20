#include <glib.h>

struct test {
  gchar *param_ok;   /**< This is a param */
  gchar *param_bad1; /* This one is not the right syntax */
  gchar *param_bad2;
  /** This one is for the param bellow */
  gchar *param_ok2;
  gchar *param_bad3; /** Missing "same line" indicator */
};
