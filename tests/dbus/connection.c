/* Feature test for https://bugs.freedesktop.org/show_bug.cgi?id=15300
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/cli-connection.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/contacts-conn.h"
#include "tests/lib/util.h"

typedef struct {
    TpDBusDaemon *dbus;
    TpTestsSimpleConnection *service_conn;
    TpBaseConnection *service_conn_as_base;
    gchar *conn_name;
    gchar *conn_path;
    TpConnection *conn;

    GAsyncResult *prepare_result;
} Test;

static GError invalidated_for_test = { 0, TP_ERROR_PERMISSION_DENIED,
      "No connection for you!" };

static void
connection_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  Test *test = user_data;

  g_message ("%p prepared", object);
  g_assert (test->prepare_result == NULL);
  test->prepare_result = g_object_ref (res);
}

static void
setup (Test *test,
    gconstpointer data)
{
  GError *error = NULL;

  invalidated_for_test.domain = TP_ERROR;

  g_type_init ();
  tp_debug_set_flags ("all");
  test->dbus = tp_tests_dbus_daemon_dup_or_die ();

  test->service_conn = TP_TESTS_SIMPLE_CONNECTION (
    tp_tests_object_new_static_class (
        TP_TESTS_TYPE_CONTACTS_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple-protocol",
        NULL));
  test->service_conn_as_base = TP_BASE_CONNECTION (test->service_conn);
  g_assert (test->service_conn != NULL);
  g_assert (test->service_conn_as_base != NULL);

  g_assert (tp_base_connection_register (test->service_conn_as_base, "simple",
        &test->conn_name, &test->conn_path, &error));
  g_assert_no_error (error);
}

static void
teardown (Test *test,
    gconstpointer data)
{
  TpConnection *conn;
  GError *error = NULL;

  if (test->conn != NULL)
    {
      g_object_unref (test->conn);
      test->conn = NULL;
    }

  /* disconnect the connection so we don't leak it */
  conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (conn != NULL);
  g_assert_no_error (error);

  tp_tests_connection_assert_disconnect_succeeds (conn);
  tp_tests_proxy_run_until_prepared_or_failed (conn, NULL, &error);
  g_assert_error (error, TP_ERROR, TP_ERROR_CANCELLED);
  g_clear_error (&error);

  test->service_conn_as_base = NULL;
  g_object_unref (test->service_conn);
  g_free (test->conn_name);
  g_free (test->conn_path);

  g_object_unref (test->dbus);
  test->dbus = NULL;
}

static void
test_prepare (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED,
      TP_CONNECTION_FEATURE_CAPABILITIES, 0 };
  TpConnectionStatusReason reason;
  TpCapabilities *caps;
  GPtrArray *classes;
  gchar *cm_name, *protocol_name;
  gboolean parsed;

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  tp_proxy_prepare_async (test->conn, NULL, connection_prepared_cb, test);
  /* this is not synchronous */
  g_assert (test->prepare_result == NULL);
  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));

  while (test->prepare_result == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_proxy_prepare_finish (test->conn, test->prepare_result,
        &error));
  g_assert_no_error (error);
  g_object_unref (test->prepare_result);
  test->prepare_result = NULL;

  g_assert (tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));
  g_assert (tp_connection_get_self_contact (test->conn) == NULL);
  g_assert_cmpint (tp_connection_get_status (test->conn, NULL), ==,
      TP_CONNECTION_STATUS_DISCONNECTED);

  parsed = tp_connection_parse_object_path (test->conn, &protocol_name,
      &cm_name);
  g_assert (parsed);
  g_assert_cmpstr (protocol_name, ==, "simple-protocol");
  g_assert_cmpstr (cm_name, ==, "simple");
  g_free (protocol_name);
  g_free (cm_name);

  g_assert_cmpstr (tp_connection_get_connection_manager_name (test->conn), ==,
          "simple");
  g_assert_cmpstr (tp_connection_get_protocol_name (test->conn), ==,
          "simple-protocol");

  g_object_get (test->conn,
      "connection-manager-name", &cm_name,
      "protocol-name", &protocol_name,
      NULL);
  g_assert_cmpstr (cm_name, ==, "simple");
  g_assert_cmpstr (protocol_name, ==, "simple-protocol");
  g_free (cm_name);
  g_free (protocol_name);

  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);

  tp_proxy_prepare_async (test->conn, features, connection_prepared_cb, test);

  while (test->prepare_result == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (tp_proxy_prepare_finish (test->conn, test->prepare_result,
        &error));
  g_assert_no_error (error);
  g_object_unref (test->prepare_result);
  test->prepare_result = NULL;

  g_assert (tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));
  g_assert (tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CAPABILITIES));
  g_assert (TP_IS_CONTACT (tp_connection_get_self_contact (test->conn)));
  g_assert_cmpint (tp_connection_get_status (test->conn, &reason), ==,
      TP_CONNECTION_STATUS_CONNECTED);
  g_assert_cmpint (reason, ==, TP_CONNECTION_STATUS_REASON_REQUESTED);

  caps = tp_connection_get_capabilities (test->conn);
  g_assert (caps != NULL);
  classes = tp_capabilities_get_channel_classes (caps);
  g_assert (classes != NULL);
  g_assert_cmpint (classes->len, ==, 0);
}

static void
test_fail_to_prepare (Test *test,
    gconstpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;
  GQuark features[] = { TP_CONNECTION_FEATURE_CONNECTED, 0 };
  const GHashTable *asv;

  test->conn = tp_connection_new (test->dbus, test->conn_name, test->conn_path,
      &error);
  g_assert (test->conn != NULL);
  g_assert_no_error (error);

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  tp_proxy_prepare_async (test->conn, NULL, connection_prepared_cb, test);
  tp_cli_connection_call_connect (test->conn, -1, NULL, NULL, NULL, NULL);
  tp_proxy_invalidate ((TpProxy *) test->conn, &invalidated_for_test);
  /* this is not synchronous */
  g_assert (test->prepare_result == NULL);
  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));

  while (test->prepare_result == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (!tp_proxy_prepare_finish (test->conn, test->prepare_result,
        &error));
  g_assert_error (error, TP_ERROR, TP_ERROR_PERMISSION_DENIED);
  g_clear_error (&error);
  g_object_unref (test->prepare_result);
  test->prepare_result = NULL;

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  /* it's not synchronous even if we were already invalidated */
  tp_proxy_prepare_async (test->conn, features, connection_prepared_cb, test);
  g_assert (test->prepare_result == NULL);

  while (test->prepare_result == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert (!tp_proxy_prepare_finish (test->conn, test->prepare_result,
        &error));
  g_assert_error (error, TP_ERROR, TP_ERROR_PERMISSION_DENIED);
  g_clear_error (&error);
  g_object_unref (test->prepare_result);
  test->prepare_result = NULL;

  g_assert (!tp_proxy_is_prepared (test->conn, TP_CONNECTION_FEATURE_CORE));
  g_assert (!tp_proxy_is_prepared (test->conn,
        TP_CONNECTION_FEATURE_CONNECTED));

  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, NULL), ==,
      TP_ERROR_STR_PERMISSION_DENIED);
  g_assert_cmpstr (tp_connection_get_detailed_error (test->conn, &asv), ==,
      TP_ERROR_STR_PERMISSION_DENIED);
  g_assert (asv != NULL);
}

int
main (int argc,
      char **argv)
{
  tp_tests_init (&argc, &argv);

  g_test_add ("/conn/prepare", Test, NULL, setup, test_prepare, teardown);
  g_test_add ("/conn/fail_to_prepare", Test, NULL, setup, test_fail_to_prepare,
      teardown);

  return g_test_run ();
}
