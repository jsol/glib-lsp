#pragma once

#include <glib.h>

/**
 * @brief A helper function
 *
 * @param self
 *
 * @return TRUE on success
 *         FALSE otherwise
 */
gboolean declares_func_ok(gpointer self);

/**
 * @brief A helper function
 *
 * @param self
 */
void declares_void_return_ok(gpointer self);

gboolean declares_func_no_comment(gpointer self);

/**
 * @brief Another func
 *
 * @return stuff
 */
gboolean declares_func_not_all_params(gpointer self);

/**
 * @brief Another func
 *
 * @param self
 */
gboolean declares_func_no_return_doc(gpointer self);

/**
 * @brief Another func
 *
 * @param self
 * @param removed
 *
 */
void declares_func_extra_doc(gpointer self);

static gboolean non_public_declare(gpointer self);

gboolean
declare_function_impl(gpointer self)
{
  return FALSE;
}
