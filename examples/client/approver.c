/*
 * approver
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <glib.h>
#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

GMainLoop *mainloop = NULL;

static void
cdo_finished_cb (TpProxy *self,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  g_print ("ChannelDispatchOperation has been invalidated\n");

  g_object_unref (self);
  g_main_loop_quit (mainloop);
}

static void
handle_with_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannelDispatchOperation *cdo = TP_CHANNEL_DISPATCH_OPERATION (source);
  GError *error;

  if (!tp_channel_dispatch_operation_handle_with_finish (cdo, result, &error))
    {
      g_print ("HandleWith() failed: %s\n", error->message);
      g_error_free (error);
      return;
    }

  g_print ("HandleWith() succeeded\n");
}

static void
close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  TpChannelDispatchOperation *cdo = TP_CHANNEL_DISPATCH_OPERATION (source);
  GError *error;

  if (!tp_channel_dispatch_operation_close_channel_finish (cdo, result, &error))
    {
      g_print ("Rejecting channels failed: %s\n", error->message);
      g_error_free (error);
      return;
    }

  g_print ("Rejected all the things!\n");
}


static void
add_dispatch_operation_cb (TpSimpleApprover *self,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    TpChannelDispatchOperation *cdo,
    TpAddDispatchOperationContext *context,
    gpointer user_data)
{
  GStrv possible_handlers;
  gchar c;

  g_print ("Approving channel %s with %s\n",
      tp_channel_get_channel_type (channel),
      tp_channel_get_identifier (channel));

  g_signal_connect (cdo, "invalidated", G_CALLBACK (cdo_finished_cb), NULL);

  possible_handlers = tp_channel_dispatch_operation_get_possible_handlers (
      cdo);
  if (possible_handlers[0] == NULL)
    {
      g_print ("\nNo possible handler suggested\n");
    }
  else
    {
      guint i;

      g_print ("\npossible handlers:\n");
      for (i = 0; possible_handlers[i] != NULL; i++)
        g_print ("  %s\n", possible_handlers[i]);
    }

  g_object_ref (cdo);

  tp_add_dispatch_operation_context_accept (context);

  g_print ("Approve? [y/n]\n");

  c = fgetc (stdin);

  if (c == 'y' || c == 'Y')
    {
      g_print ("Approve channel\n");

      tp_channel_dispatch_operation_handle_with_async (cdo, NULL,
          TP_USER_ACTION_TIME_CURRENT_TIME, handle_with_cb, NULL);
    }
  else if (c == 'n' || c == 'N')
    {
      g_print ("Reject channel\n");

      tp_channel_dispatch_operation_close_channel_async (cdo, close_cb, NULL);
    }
  else
    {
      g_print ("Ignore channel\n");
    }
}

int
main (int argc,
      char **argv)
{
  GError *error = NULL;
  TpBaseClient *approver;

  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  approver = tp_simple_approver_new (NULL, "ExampleApprover",
      FALSE, add_dispatch_operation_cb, NULL, NULL);

  /* contact text chat */
  tp_base_client_add_approver_filter (approver,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT));

  /* calls */
  tp_base_client_add_approver_filter (approver,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_CALL1,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT));

  /* room text chat */
  tp_base_client_add_approver_filter (approver,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_ROOM));

  /* file transfer */
  tp_base_client_add_approver_filter (approver,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER1,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT));

  if (!tp_base_client_register (approver, &error))
    {
      g_warning ("Failed to register Approver: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  g_print ("Start approving\n");

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

out:
  g_object_unref (approver);

  return 0;
}
