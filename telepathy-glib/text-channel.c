/*
 * text-channel.h - high level API for Text channels
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION:text-channel
 * @title: TpTextChannel
 * @short_description: proxy object for a text channel
 *
 * #TpTextChannel is a sub-class of #TpChannel providing convenient API
 * to send and receive #TpMessage.
 */

/**
 * TpTextChannel:
 *
 * Data structure representing a #TpTextChannel.
 *
 * Since: 0.13.10
 */

/**
 * TpTextChannelClass:
 *
 * The class of a #TpTextChannel.
 *
 * Since: 0.13.10
 */

#include "config.h"

#include "telepathy-glib/text-channel.h"

#include <telepathy-glib/cli-channel.h>
#include <telepathy-glib/cli-misc.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gnio-util.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/message-internal.h>
#include <telepathy-glib/proxy-internal.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/signalled-message-internal.h>
#include <telepathy-glib/util-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/automatic-client-factory-internal.h"
#include "telepathy-glib/channel-internal.h"

#include <stdio.h>
#include <glib/gstdio.h>

G_DEFINE_TYPE (TpTextChannel, tp_text_channel, TP_TYPE_CHANNEL)

struct _TpTextChannelPrivate
{
  GStrv supported_content_types;
  TpMessagePartSupportFlags message_part_support_flags;
  TpDeliveryReportingSupportFlags delivery_reporting_support;
  GArray *message_types;

  GSimpleAsyncResult *pending_messages_result;
  guint n_preparing_pending_messages;

  /* queue of owned TpSignalledMessage */
  GQueue *pending_messages;
  gboolean got_initial_messages;

  gboolean is_sms_channel;
  gboolean sms_flash;

  /* TpHandle => TpChannelChatState */
  GHashTable *chat_states;
};

enum
{
  PROP_SUPPORTED_CONTENT_TYPES = 1,
  PROP_MESSAGE_PART_SUPPORT_FLAGS,
  PROP_DELIVERY_REPORTING_SUPPORT,
  PROP_MESSAGE_TYPES,
  PROP_IS_SMS_CHANNEL,
  PROP_SMS_FLASH,
};

enum /* signals */
{
  SIG_MESSAGE_RECEIVED,
  SIG_PENDING_MESSAGE_REMOVED,
  SIG_MESSAGE_SENT,
  SIG_CHAT_STATE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void
tp_text_channel_dispose (GObject *obj)
{
  TpTextChannel *self = (TpTextChannel *) obj;

  tp_clear_pointer (&self->priv->supported_content_types, g_strfreev);
  tp_clear_pointer (&self->priv->message_types, g_array_unref);

  g_queue_foreach (self->priv->pending_messages, (GFunc) g_object_unref, NULL);
  tp_clear_pointer (&self->priv->pending_messages, g_queue_free);

  tp_clear_pointer (&self->priv->chat_states, g_hash_table_unref);

  G_OBJECT_CLASS (tp_text_channel_parent_class)->dispose (obj);
}

static void
tp_text_channel_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpTextChannel *self = (TpTextChannel *) object;

  switch (property_id)
    {
      case PROP_SUPPORTED_CONTENT_TYPES:
        g_value_set_boxed (value,
            tp_text_channel_get_supported_content_types (self));
        break;

      case PROP_MESSAGE_PART_SUPPORT_FLAGS:
        g_value_set_uint (value,
            tp_text_channel_get_message_part_support_flags (self));
        break;

      case PROP_DELIVERY_REPORTING_SUPPORT:
        g_value_set_uint (value,
            tp_text_channel_get_delivery_reporting_support (self));
        break;

      case PROP_MESSAGE_TYPES:
        g_value_set_boxed (value,
            tp_text_channel_get_message_types (self));
        break;

      case PROP_IS_SMS_CHANNEL:
        g_value_set_boolean (value, tp_text_channel_is_sms_channel (self));
        break;

      case PROP_SMS_FLASH:
        g_value_set_boolean (value, tp_text_channel_get_sms_flash (self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static TpContact *
dup_sender (TpTextChannel *self,
    const GPtrArray *message)
{
  const GHashTable *header;
  TpHandle handle;
  const gchar *sender_id = NULL;
  TpConnection *conn;

  header = g_ptr_array_index (message, 0);

  handle = tp_asv_get_uint32 (header, "message-sender", NULL);
  if (handle == 0)
    {
      DEBUG ("Message received on Channel %s doesn't have message-sender",
          tp_proxy_get_object_path (self));
      return NULL;
    }

  sender_id = tp_asv_get_string (header, "message-sender-id");
  if (tp_str_empty (sender_id))
    {
      DEBUG ("Message received on Channel %s doesn't have message-sender-id",
          tp_proxy_get_object_path (self));
      return NULL;
    }

  conn = tp_channel_get_connection ((TpChannel *) self);

  return tp_connection_dup_contact_if_possible (conn, handle, sender_id);
}

static void
prepare_sender_async (TpTextChannel *self,
    const GPtrArray *message,
    gboolean fallback_to_self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *channel = (TpChannel *) self;
  TpContact *contact;

  contact = dup_sender (self, message);
  if (contact == NULL && fallback_to_self)
    {
      TpConnection *conn;

      conn = tp_channel_get_connection ((TpChannel *) self);

      DEBUG ("Failed to get our self contact, please fix CM (%s)",
          tp_proxy_get_object_path (conn));

      /* Use the connection self contact as a fallback */
      contact = tp_connection_get_self_contact (conn);
      if (contact != NULL)
        g_object_ref (contact);
    }

  if (contact != NULL)
    {
      GPtrArray *contacts = g_ptr_array_new_with_free_func (g_object_unref);

      /* Give contact ref to the array */
      g_ptr_array_add (contacts, contact);
      _tp_channel_contacts_queue_prepare_async (channel,
          contacts, callback, user_data);
      g_ptr_array_unref (contacts);
    }
  else
    {
      /* No sender. Still need to go through the queue to prevent reordering */
      _tp_channel_contacts_queue_prepare_async (channel,
          NULL, callback, user_data);
    }
}

static TpContact *
prepare_sender_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  GPtrArray *contacts;
  TpContact *sender = NULL;

  _tp_channel_contacts_queue_prepare_finish ((TpChannel *) self, result,
      &contacts, error);

  if (contacts != NULL && contacts->len > 0)
    sender = g_object_ref (g_ptr_array_index (contacts, 0));

  tp_clear_pointer (&contacts, g_ptr_array_unref);

  return sender;
}

static GPtrArray *
copy_parts (const GPtrArray *parts)
{
  return g_boxed_copy (TP_ARRAY_TYPE_MESSAGE_PART_LIST, parts);
}

static void
free_parts (GPtrArray *parts)
{
  g_boxed_free (TP_ARRAY_TYPE_MESSAGE_PART_LIST, parts);
}

typedef struct
{
  GPtrArray *parts;
  guint flags;
  gchar *token;
} MessageSentData;

static void
message_sent_sender_ready_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpTextChannel *self = (TpTextChannel *) object;
  MessageSentData *data = user_data;
  TpContact *sender;
  TpMessage *msg;

  sender = prepare_sender_finish (self, result, NULL);
  msg = _tp_signalled_message_new (data->parts, sender);

  g_signal_emit (self, signals[SIG_MESSAGE_SENT], 0, msg, data->flags,
      data->token);

  g_object_unref (msg);
  free_parts (data->parts);
  g_free (data->token);
  g_slice_free (MessageSentData, data);
}

static void
message_sent_cb (TpChannel *channel,
    const GPtrArray *parts,
    guint flags,
    const gchar *token,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) channel;
  MessageSentData *data;

  DEBUG ("New message sent");

  data = g_slice_new (MessageSentData);
  data->parts = copy_parts (parts);
  data->flags = flags;
  data->token = tp_str_empty (token) ? NULL : g_strdup (token);

  prepare_sender_async (self, parts, TRUE,
      message_sent_sender_ready_cb, data);
}

static void
chat_state_changed_cb (TpChannel *channel,
    TpHandle handle,
    TpChannelChatState state,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) channel;
  TpConnection *conn;
  TpContact *contact;

  /* Ignore signal if we did not receive initial states */
  if (self->priv->chat_states == NULL)
    return;

  g_hash_table_insert (self->priv->chat_states,
      GUINT_TO_POINTER (handle),
      GUINT_TO_POINTER (state));

  /* We only guarantee to emit chat-state-changed for contacts that exist.
   * In particular, for 1-1 channels, the peer TpContact already exists, and for
   * Group channels, the GROUP feature makes all members exist. */
  conn = tp_channel_get_connection (channel);
  contact = tp_connection_dup_contact_if_possible (conn, handle, NULL);
  if (contact == NULL)
    return;

  g_signal_emit (self, signals[SIG_CHAT_STATE_CHANGED], 0, contact, state);

  g_object_unref (contact);
}

static void
got_chat_states_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) proxy;
  GSimpleAsyncResult *result = user_data;

  self->priv->chat_states = g_hash_table_new (NULL, NULL);

  if (error == NULL && G_VALUE_HOLDS (value, TP_HASH_TYPE_CHAT_STATE_MAP))
    {
      tp_g_hash_table_update (self->priv->chat_states,
          g_value_get_boxed (value), NULL, NULL);
    }
  /* else just ignore it and assume everyone was initially in the default
   * Inactive state. */

  g_simple_async_result_complete (result);
}

static void
tp_text_channel_prepare_chat_states_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *channel = (TpChannel *) proxy;
  GSimpleAsyncResult *result;
  GError *error = NULL;

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      tp_text_channel_prepare_chat_states_async);

  tp_cli_channel_interface_chat_state1_connect_to_chat_state_changed (channel,
      chat_state_changed_cb, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  tp_cli_dbus_properties_call_get (channel, -1,
      TP_IFACE_CHANNEL_INTERFACE_CHAT_STATE1, "ChatStates",
      got_chat_states_cb,
      result, g_object_unref, NULL);
}

static void
tp_text_channel_constructed (GObject *obj)
{
  TpTextChannel *self = (TpTextChannel *) obj;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_text_channel_parent_class)->constructed;
  TpChannel *chan = (TpChannel *) obj;
  GHashTable *props;
  gboolean valid;
  GError *err = NULL;

  if (chain_up != NULL)
    chain_up (obj);

  if (tp_channel_get_channel_type_id (chan) !=
      TP_IFACE_QUARK_CHANNEL_TYPE_TEXT)
    {
      GError error = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "Channel is not of type Text" };

      DEBUG ("Channel %s is not of type Text: %s",
          tp_proxy_get_object_path (self), tp_channel_get_channel_type (chan));

      tp_proxy_invalidate (TP_PROXY (self), &error);
      return;
    }

  props = _tp_channel_get_immutable_properties (TP_CHANNEL (self));

  self->priv->supported_content_types = (GStrv) tp_asv_get_strv (props,
      TP_PROP_CHANNEL_TYPE_TEXT_SUPPORTED_CONTENT_TYPES);
  if (self->priv->supported_content_types == NULL)
    {
      const gchar * const plain[] = { "text/plain", NULL };

      DEBUG ("Channel %s doesn't have SupportedContentTypes in its "
          "immutable properties", tp_proxy_get_object_path (self));

      /* spec mandates that plain text is always allowed. */
      self->priv->supported_content_types = g_strdupv ((GStrv) plain);
    }
  else
    {
      self->priv->supported_content_types = g_strdupv (
          self->priv->supported_content_types);
    }

  self->priv->message_part_support_flags = tp_asv_get_uint32 (props,
      TP_PROP_CHANNEL_TYPE_TEXT_MESSAGE_PART_SUPPORT_FLAGS, &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have MessagePartSupportFlags in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->delivery_reporting_support = tp_asv_get_uint32 (props,
      TP_PROP_CHANNEL_TYPE_TEXT_DELIVERY_REPORTING_SUPPORT, &valid);
  if (!valid)
    {
      DEBUG ("Channel %s doesn't have DeliveryReportingSupport in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  self->priv->message_types = tp_asv_get_boxed (props,
      TP_PROP_CHANNEL_TYPE_TEXT_MESSAGE_TYPES, DBUS_TYPE_G_UINT_ARRAY);
  if (self->priv->message_types != NULL)
    {
      self->priv->message_types = g_boxed_copy (DBUS_TYPE_G_UINT_ARRAY,
          self->priv->message_types);
    }
  else
    {
      self->priv->message_types = g_array_new (FALSE, FALSE,
          sizeof (TpChannelTextMessageType));

      DEBUG ("Channel %s doesn't have MessageTypes in its "
          "immutable properties", tp_proxy_get_object_path (self));
    }

  tp_cli_channel_type_text_connect_to_message_sent (chan,
      message_sent_cb, NULL, NULL, NULL, &err);
  if (err != NULL)
    {
      WARNING ("Failed to connect to MessageSent on %s: %s",
          tp_proxy_get_object_path (self), err->message);
      g_error_free (err);
    }

  /* SMS */
  self->priv->sms_flash = tp_asv_get_boolean (props,
      TP_PROP_CHANNEL_INTERFACE_SMS1_FLASH, NULL);
}

static void
add_message_received (TpTextChannel *self,
    const GPtrArray *parts,
    TpContact *sender,
    gboolean fire_received)
{
  TpMessage *msg;

  msg = _tp_signalled_message_new (parts, sender);

  g_queue_push_tail (self->priv->pending_messages, msg);

  if (fire_received)
    g_signal_emit (self, signals[SIG_MESSAGE_RECEIVED], 0, msg);
}

static void
message_received_sender_ready_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpTextChannel *self = (TpTextChannel *) object;
  GPtrArray *parts = user_data;
  TpContact *sender;

  sender = prepare_sender_finish (self, result, NULL);
  add_message_received (self, parts, sender, TRUE);

  free_parts (parts);
}

static void
message_received_cb (TpChannel *proxy,
    const GPtrArray *message,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = user_data;

  /* If we are still retrieving pending messages, no need to add the message,
   * it will be in the initial set of messages retrieved. */
  if (!self->priv->got_initial_messages)
    return;

  DEBUG ("New message received");

  prepare_sender_async (self, message, FALSE,
      message_received_sender_ready_cb,
      copy_parts (message));
}

static gint
find_msg_by_id (gconstpointer a,
    gconstpointer b)
{
  TpMessage *msg = TP_MESSAGE (a);
  guint id = GPOINTER_TO_UINT (b);
  gboolean valid;
  guint msg_id;

  msg_id = _tp_signalled_message_get_pending_message_id (msg, &valid);
  if (!valid)
    return 1;

  return msg_id != id;
}

static void
pending_messages_removed_ready_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpTextChannel *self = (TpTextChannel *) object;
  TpChannel *channel = (TpChannel *) self;
  GArray *ids = user_data;
  guint i;

  _tp_channel_contacts_queue_prepare_finish (channel, result, NULL, NULL);

  for (i = 0; i < ids->len; i++)
    {
      guint id = g_array_index (ids, guint, i);
      GList *link_;
      TpMessage *msg;

      link_ = g_queue_find_custom (self->priv->pending_messages,
          GUINT_TO_POINTER (id), find_msg_by_id);

      if (link_ == NULL)
        {
          DEBUG ("Unable to find pending message having id %d", id);
          continue;
        }

      msg = link_->data;

      g_queue_delete_link (self->priv->pending_messages, link_);

      g_signal_emit (self, signals[SIG_PENDING_MESSAGE_REMOVED], 0, msg);

      g_object_unref (msg);
    }

  g_array_unref (ids);
}

static void
pending_messages_removed_cb (TpChannel *channel,
    const GArray *ids,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) channel;
  GArray *ids_dup;

  if (!self->priv->got_initial_messages)
    return;

  /* We have nothing to prepare, but still we have to hook into the queue
   * to preserve order. Messages removed here could still be in that queue. */
  ids_dup = g_array_sized_new (FALSE, FALSE, sizeof (guint), ids->len);
  g_array_append_vals (ids_dup, ids->data, ids->len);

  _tp_channel_contacts_queue_prepare_async (channel, NULL,
      pending_messages_removed_ready_cb, ids_dup);
}

static void
pending_message_sender_ready_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpTextChannel *self = (TpTextChannel *) object;
  GPtrArray *parts = user_data;
  TpContact *sender;

  sender = prepare_sender_finish (self, result, NULL);
  add_message_received (self, parts, sender, FALSE);

  self->priv->n_preparing_pending_messages--;
  if (self->priv->n_preparing_pending_messages == 0)
    {
      g_simple_async_result_complete (self->priv->pending_messages_result);
      g_clear_object (&self->priv->pending_messages_result);
    }

  free_parts (parts);
}

/* There is no TP_ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST_LIST (fdo #32433) */
#define ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST_LIST dbus_g_type_get_collection (\
    "GPtrArray", TP_ARRAY_TYPE_MESSAGE_PART_LIST)

static void
get_pending_messages_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) proxy;
  GPtrArray *messages;
  guint i;

  self->priv->got_initial_messages = TRUE;

  if (error != NULL)
    {
      DEBUG ("Failed to get PendingMessages property: %s", error->message);

      g_simple_async_result_set_error (self->priv->pending_messages_result,
          error->domain, error->code,
          "Failed to get PendingMessages property: %s", error->message);

      g_simple_async_result_complete (self->priv->pending_messages_result);
      g_clear_object (&self->priv->pending_messages_result);
      return;
    }

  if (!G_VALUE_HOLDS (value, ARRAY_TYPE_PENDING_TEXT_MESSAGE_LIST_LIST))
    {
      DEBUG ("PendingMessages property is of the wrong type");

      g_simple_async_result_set_error (self->priv->pending_messages_result,
          TP_ERROR, TP_ERROR_CONFUSED,
          "PendingMessages property is of the wrong type");

      g_simple_async_result_complete (self->priv->pending_messages_result);
      g_clear_object (&self->priv->pending_messages_result);
      return;
    }

  messages = g_value_get_boxed (value);

  if (messages->len == 0)
    {
      g_simple_async_result_complete (self->priv->pending_messages_result);
      g_clear_object (&self->priv->pending_messages_result);
      return;
    }

  self->priv->n_preparing_pending_messages = messages->len;
  for (i = 0; i < messages->len; i++)
    {
      GPtrArray *parts = g_ptr_array_index (messages, i);

      prepare_sender_async (self, parts, FALSE,
          pending_message_sender_ready_cb,
          copy_parts (parts));
    }
}

static void
tp_text_channel_prepare_incoming_messages_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpTextChannel *self = (TpTextChannel *) proxy;
  TpChannel *channel = (TpChannel *) self;
  GError *error = NULL;

  tp_cli_channel_type_text_connect_to_message_received (channel,
      message_received_cb, proxy, NULL, G_OBJECT (proxy), &error);
  if (error != NULL)
    {
      g_simple_async_report_take_gerror_in_idle ((GObject *) self,
          callback, user_data, error);
      return;
    }

  tp_cli_channel_type_text_connect_to_pending_messages_removed (
      channel, pending_messages_removed_cb, proxy, NULL, G_OBJECT (proxy),
      &error);
  if (error != NULL)
    {
      g_simple_async_report_take_gerror_in_idle ((GObject *) self,
          callback, user_data, error);
      return;
    }

  g_assert (self->priv->pending_messages_result == NULL);
  self->priv->pending_messages_result = g_simple_async_result_new (
      (GObject *) proxy, callback, user_data,
      tp_text_channel_prepare_incoming_messages_async);


  tp_cli_dbus_properties_call_get (proxy, -1,
      TP_IFACE_CHANNEL_TYPE_TEXT, "PendingMessages",
      get_pending_messages_cb, NULL, NULL, G_OBJECT (proxy));
}

static void
get_sms_channel_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) proxy;
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to get SMSChannel property: %s", error->message);

      g_simple_async_result_set_error (result, error->domain, error->code,
          "Failed to get SMSChannel property: %s", error->message);
      goto out;
    }

  if (!G_VALUE_HOLDS (value, G_TYPE_BOOLEAN))
    {
      DEBUG ("SMSChannel property is of the wrong type");

      g_simple_async_result_set_error (result, TP_ERROR, TP_ERROR_CONFUSED,
          "SMSChannel property is of the wrong type");
      goto out;
    }

  self->priv->is_sms_channel = g_value_get_boolean (value);

  /* self->priv->is_sms_channel is set to FALSE by default, so only notify the
   * property change is it is now set to TRUE. */
  if (self->priv->is_sms_channel)
    g_object_notify (G_OBJECT (self), "is-sms-channel");

out:
  g_simple_async_result_complete (result);
  g_object_unref (result);
}

static void
sms_channel_changed_cb (TpChannel *proxy,
    gboolean sms,
    gpointer user_data,
    GObject *weak_object)
{
  TpTextChannel *self = (TpTextChannel *) proxy;

  if (self->priv->is_sms_channel == sms)
    return;

  self->priv->is_sms_channel = sms;
  g_object_notify (weak_object, "is-sms-channel");
}

static void
tp_text_channel_prepare_sms_async (TpProxy *proxy,
    const TpProxyFeature *feature,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;
  GError *error = NULL;

  result = g_simple_async_result_new ((GObject *) proxy, callback, user_data,
      tp_text_channel_prepare_sms_async);

  tp_cli_channel_interface_sms1_connect_to_sms_channel_changed (
      (TpChannel *) proxy, sms_channel_changed_cb, NULL, NULL,
      G_OBJECT (proxy), &error);
  if (error != NULL)
    {
      WARNING ("Failed to connect to SMS.SMSChannelChanged: %s",
          error->message);
      g_error_free (error);
    }

  tp_cli_dbus_properties_call_get (proxy, -1,
      TP_IFACE_CHANNEL_INTERFACE_SMS1, "SMSChannel",
      get_sms_channel_cb, result, NULL, G_OBJECT (proxy));
}

enum {
    FEAT_INCOMING_MESSAGES,
    FEAT_SMS,
    FEAT_CHAT_STATES,
    N_FEAT
};

static const TpProxyFeature *
tp_text_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };
  static GQuark need_sms[2] = {0, 0};
  static GQuark need_chat_states[2] = {0, 0};

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_INCOMING_MESSAGES].name =
    TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES;
  features[FEAT_INCOMING_MESSAGES].prepare_async =
    tp_text_channel_prepare_incoming_messages_async;

  features[FEAT_SMS].name =
    TP_TEXT_CHANNEL_FEATURE_SMS;
  features[FEAT_SMS].prepare_async =
    tp_text_channel_prepare_sms_async;
  need_sms[0] = TP_IFACE_QUARK_CHANNEL_INTERFACE_SMS1;
  features[FEAT_SMS].interfaces_needed = need_sms;

  features[FEAT_CHAT_STATES].name =
    TP_TEXT_CHANNEL_FEATURE_CHAT_STATES;
  features[FEAT_CHAT_STATES].prepare_async =
    tp_text_channel_prepare_chat_states_async;
  need_chat_states[0] = TP_IFACE_QUARK_CHANNEL_INTERFACE_CHAT_STATE1;
  features[FEAT_CHAT_STATES].interfaces_needed = need_chat_states;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_text_channel_class_init (TpTextChannelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GParamSpec *param_spec;

  gobject_class->constructed = tp_text_channel_constructed;
  gobject_class->get_property = tp_text_channel_get_property;
  gobject_class->dispose = tp_text_channel_dispose;

  proxy_class->list_features = tp_text_channel_list_features;

  /**
   * TpTextChannel:supported-content-types:
   *
   * A #GStrv containing the MIME types supported by this channel, with more
   * preferred MIME types appearing earlier in the array.
   *
   * Since: 0.13.10
   */
  param_spec = g_param_spec_boxed ("supported-content-types",
      "SupportedContentTypes",
      "The SupportedContentTypes property of the channel",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SUPPORTED_CONTENT_TYPES,
      param_spec);

  /**
   * TpTextChannel:message-part-support-flags:
   *
   * A #TpMessagePartSupportFlags indicating the level of support for
   * message parts on this channel.
   *
   * Since: 0.13.10
   */
  param_spec = g_param_spec_uint ("message-part-support-flags",
      "MessagePartSupportFlags",
      "The MessagePartSupportFlags property of the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_MESSAGE_PART_SUPPORT_FLAGS, param_spec);

  /**
   * TpTextChannel:delivery-reporting-support:
   *
   * A #TpDeliveryReportingSupportFlags indicating features supported
   * by this channel.
   *
   * Since: 0.13.10
   */
  param_spec = g_param_spec_uint ("delivery-reporting-support",
      "DeliveryReportingSupport",
      "The DeliveryReportingSupport property of the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_DELIVERY_REPORTING_SUPPORT, param_spec);

  /**
   * TpTextChannel:message-types:
   *
   * A #GArray containing the #TpChannelTextMessageType which may be sent on
   * this channel.
   *
   * Since: 0.13.16
   */
  param_spec = g_param_spec_boxed ("message-types",
      "MessageTypes",
      "The MessageTypes property of the channel",
      DBUS_TYPE_G_UINT_ARRAY,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class,
      PROP_MESSAGE_TYPES, param_spec);

 /**
   * TpTextChannel:is-sms-channel:
   *
   * %TRUE if messages sent and received on this channel are transmitted
   * via SMS.
   *
   * This property is not guaranteed to have a meaningful value until
   * TP_TEXT_CHANNEL_FEATURE_SMS has been prepared.
   *
   * Since: 0.15.1
   */
  param_spec = g_param_spec_boolean ("is-sms-channel",
      "is SMS channel",
      "The SMS.SMSChannel property of the channel",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_IS_SMS_CHANNEL,
      param_spec);

 /**
   * TpTextChannel:sms-flash:
   *
   * %TRUE if this channel is exclusively for receiving class 0 SMSes
   * (and no SMSes can be sent using tp_text_channel_send_message_async()
   * on this channel). If %FALSE, no incoming class 0 SMSes will appear
   * on this channel.
   *
   * Since: 0.15.1
   */
  param_spec = g_param_spec_boolean ("sms-flash",
      "SMS flash",
      "The SMS.Flash property of the channel",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_SMS_FLASH, param_spec);

  /**
   * TpTextChannel::message-received:
   * @self: the #TpTextChannel
   * @message: a #TpSignalledMessage
   *
   * The ::message-received signal is emitted when a new message has been
   * received on @self.
   *
   * The same @message object will be used by the
   * #TpTextChannel::pending-message-removed signal once @message has been
   * acked so you can simply compare pointers to identify the message.
   *
   * Note that this signal is only fired once the
   * #TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES has been prepared.
   *
   * It is guaranteed that @message's #TpSignalledMessage:sender has all of the
   * features previously passed to
   * tp_client_factory_add_contact_features() prepared.
   *
   * Since: 0.13.10
   */
  signals[SIG_MESSAGE_RECEIVED] = g_signal_new ("message-received",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE,
      1, TP_TYPE_SIGNALLED_MESSAGE);

  /**
   * TpTextChannel::pending-message-removed:
   * @self: the #TpTextChannel
   * @message: a #TpSignalledMessage
   *
   * The ::pending-message-removed signal is emitted when @message
   * has been acked and so removed from the pending messages list.
   *
   * Note that this signal is only fired once the
   * #TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES has been prepared.
   *
   * It is guaranteed that @message's #TpSignalledMessage:sender has all of the
   * features previously passed to
   * tp_client_factory_add_contact_features() prepared.
   *
   * Since: 0.13.10
   */
  signals[SIG_PENDING_MESSAGE_REMOVED] = g_signal_new (
      "pending-message-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE,
      1, TP_TYPE_SIGNALLED_MESSAGE);

  /**
   * TpTextChannel::message-sent:
   * @self: the #TpTextChannel
   * @message: a #TpSignalledMessage
   * @flags: the #TpMessageSendingFlags affecting how the message was sent
   * @token: an opaque token used to match any incoming delivery or failure
   * reports against this message, or %NULL if the message is not
   * readily identifiable.
   *
   * The ::message-sent signal is emitted when @message
   * has been submitted for sending.
   *
   * It is guaranteed that @message's #TpSignalledMessage:sender has all of the
   * features previously passed to
   * tp_client_factory_add_contact_features() prepared.
   *
   * Since: 0.13.10
   */
  signals[SIG_MESSAGE_SENT] = g_signal_new (
      "message-sent",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL,
      G_TYPE_NONE,
      3, TP_TYPE_SIGNALLED_MESSAGE, G_TYPE_UINT, G_TYPE_STRING);

  g_type_class_add_private (gobject_class, sizeof (TpTextChannelPrivate));

  /**
   * TpTextChannel::chat-state-changed:
   * @self: a channel
   * @contact: a #TpContact for the local user or another contact
   * @state: the new #TpChannelChatState for the contact
   *
   * Emitted when a contact's chat state changes after tp_proxy_prepare_async()
   * has finished preparing features %TP_TEXT_CHANNEL_FEATURE_CHAT_STATES.
   *
   * For group chats, it is not guaranteed this signal will be emitted for every
   * members, until %TP_CHANNEL_FEATURE_GROUP has ben prepared using
   * tp_proxy_prepare_async().
   *
   * Since: 0.99.1
   */
  signals[SIG_CHAT_STATE_CHANGED] = g_signal_new (
      "chat-state-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL, NULL,
      G_TYPE_NONE, 2, TP_TYPE_CONTACT, G_TYPE_UINT);
}

static void
tp_text_channel_init (TpTextChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), TP_TYPE_TEXT_CHANNEL,
      TpTextChannelPrivate);

  self->priv->pending_messages = g_queue_new ();
}

TpTextChannel *
_tp_text_channel_new_with_factory (
    TpClientFactory *factory,
    TpConnection *conn,
    const gchar *object_path,
    const GHashTable *immutable_properties,
    GError **error)
{
  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    return NULL;

  return g_object_new (TP_TYPE_TEXT_CHANNEL,
      "connection", conn,
       "dbus-daemon", tp_proxy_get_dbus_daemon (conn),
       "bus-name", tp_proxy_get_bus_name (conn),
       "object-path", object_path,
       "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
       "channel-properties", immutable_properties,
       "factory", factory,
       NULL);
}

/**
 * tp_text_channel_get_supported_content_types:
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:supported-content-types property
 *
 * Returns: (transfer none) :
 * the value of #TpTextChannel:supported-content-types
 *
 * Since: 0.13.10
 */
const gchar * const *
tp_text_channel_get_supported_content_types (TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), NULL);

  return (const gchar * const *) self->priv->supported_content_types;
}

/**
 * tp_text_channel_get_message_part_support_flags:
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:message-part-support-flags property
 *
 * Returns: the value of #TpTextChannel:message-part-support-flags
 *
 * Since: 0.13.10
 */
TpMessagePartSupportFlags
tp_text_channel_get_message_part_support_flags (
    TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), 0);

  return self->priv->message_part_support_flags;
}

/**
 * tp_text_channel_get_delivery_reporting_support:
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:delivery-reporting-support property
 *
 * Returns: the value of #TpTextChannel:delivery-reporting-support property
 *
 * Since: 0.13.10
 */
TpDeliveryReportingSupportFlags
tp_text_channel_get_delivery_reporting_support (
    TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), 0);

  return self->priv->delivery_reporting_support;
}

/**
 * TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES:
 *
 * Expands to a call to a function that returns a quark representing the
 * incoming messages features of a #TpTextChannel.
 *
 * When this feature is prepared, tp_text_channel_dup_pending_messages() will
 * return a non-empty list if any unacknowledged messages are waiting, and the
 * #TpTextChannel::message-received and #TpTextChannel::pending-message-removed
 * signals will be emitted.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.13.10
 */
GQuark
tp_text_channel_get_feature_quark_incoming_messages (void)
{
  return g_quark_from_static_string (
      "tp-text-channel-feature-incoming-messages");
}

/**
 * tp_text_channel_dup_pending_messages:
 * @self: a #TpTextChannel
 *
 * Return a newly allocated list of unacknowledged #TpSignalledMessage
 * objects.
 *
 * It is guaranteed that the #TpSignalledMessage:sender of each
 * #TpSignalledMessage has all of the features previously passed to
 * tp_simple_client_factory_add_contact_features() prepared.
 *
 * Returns: (transfer full) (element-type TelepathyGLib.SignalledMessage):
 * a #GList of reffed #TpSignalledMessage
 *
 * Since: 0.19.9
 */
GList *
tp_text_channel_dup_pending_messages (TpTextChannel *self)
{
  return _tp_g_list_copy_deep (
      g_queue_peek_head_link (self->priv->pending_messages),
      (GCopyFunc) g_object_ref, NULL);
}

static void
send_message_cb (TpChannel *proxy,
    const gchar *token,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to send message: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_set_op_res_gpointer (result,
      tp_str_empty (token) ? NULL : g_strdup (token), g_free);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_text_channel_send_message_async:
 * @self: a #TpTextChannel
 * @message: a #TpClientMessage
 * @flags: flags affecting how the message is sent
 * @callback: a callback to call when the message has been submitted to the
 * server
 * @user_data: data to pass to @callback
 *
 * Submit a message to the server for sending. Once the message has been
 * submitted to the sever, @callback will be called. You can then call
 * tp_text_channel_send_message_finish() to get the result of the operation.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_send_message_async (TpTextChannel *self,
    TpMessage *message,
    TpMessageSendingFlags flags,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TP_IS_CLIENT_MESSAGE (message));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_send_message_async);

  tp_cli_channel_type_text_call_send_message (TP_CHANNEL (self),
    -1, message->parts, flags, send_message_cb, result, NULL, NULL);
}

/**
 * tp_text_channel_send_message_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult passed to the callback for tp_text_channel_send_message_async()
 * @token: (out) (transfer full): if not %NULL, used to return the
 * token of the sent message
 * @error: a #GError to fill
 *
 * Completes a call to tp_text_channel_send_message_async().
 *
 * @token can be used to match any incoming delivery or failure reports
 * against the sent message. If this function returns true but the returned
 * token is %NULL, the message was sent successfully but the protocol does not
 * provide a way to identify it later.
 *
 * Returns: %TRUE if the message has been submitted to the server, %FALSE
 * otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_send_message_finish (TpTextChannel *self,
    GAsyncResult *result,
    gchar **token,
    GError **error)
{
  _tp_implement_finish_copy_pointer (self, tp_text_channel_send_message_async,
      g_strdup, token);
}

static void
acknowledge_pending_messages_ready_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  TpChannel *channel = (TpChannel *) object;
  GSimpleAsyncResult *result = user_data;

  _tp_channel_contacts_queue_prepare_finish (channel, res, NULL, NULL);

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

static void
acknowledge_pending_messages_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("Failed to ack messages: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
      g_simple_async_result_complete (result);
      g_object_unref (result);
      return;
    }

  /* We should have already got MessagesRemoved signal, but it could still be
   * in the queue. So we have to hook into that queue before complete to be
   * sure messages are removed from pending_messages */
  _tp_channel_contacts_queue_prepare_async (channel, NULL,
      acknowledge_pending_messages_ready_cb, result);
}

/**
 * tp_text_channel_ack_messages_async:
 * @self: a #TpTextChannel
 * @messages: (element-type TelepathyGLib.SignalledMessage): a #GList of
 * #TpSignalledMessage
 * @callback: a callback to call when the message have been acked
 * @user_data: data to pass to @callback
 *
 * Acknowledge all the messages in @messages.
 * Once the messages have been acked, @callback will be called.
 * You can then call tp_text_channel_ack_messages_finish() to get the
 * result of the operation.
 *
 * You should use the #TpSignalledMessage received from
 * tp_text_channel_dup_pending_messages() or the
 * #TpTextChannel::message-received signal.
 *
 * See tp_text_channel_ack_message_async() about acknowledging messages.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_ack_messages_async (TpTextChannel *self,
    const GList *messages,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *chan = (TpChannel *) self;
  GArray *ids;
  GList *l;
  GSimpleAsyncResult *result;

  g_return_if_fail (TP_IS_TEXT_CHANNEL (self));

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_ack_messages_async);

  if (messages == NULL)
    {
      /* Nothing to ack, succeed immediately */
      g_simple_async_result_complete_in_idle (result);

      g_object_unref (result);
      return;
    }

  ids = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      g_list_length ((GList *) messages));

  for (l = (GList *) messages; l != NULL; l = g_list_next (l))
    {
      TpMessage *msg = l->data;
      guint id;
      gboolean valid;

      g_return_if_fail (TP_IS_SIGNALLED_MESSAGE (msg));

      id = _tp_signalled_message_get_pending_message_id (msg, &valid);
      if (!valid)
        {
          DEBUG ("Message doesn't have pending-message-id ?!");
          continue;
        }

      g_array_append_val (ids, id);
    }

  tp_cli_channel_type_text_call_acknowledge_pending_messages (chan, -1, ids,
      acknowledge_pending_messages_cb, result, NULL, G_OBJECT (self));

  g_array_unref (ids);
}

/**
 * tp_text_channel_ack_messages_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult passed to the callback for tp_text_channel_ack_messages_async()
 * @error: a #GError to fill
 *
 * Finishes acknowledging a list of messages.
 *
 * Returns: %TRUE if the messages have been acked, %FALSE otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_ack_messages_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_text_channel_ack_messages_async)
}

/**
 * tp_text_channel_ack_message_async:
 * @self: a #TpTextChannel
 * @message: a #TpSignalledMessage
 * @callback: a callback to call when the message have been acked
 * @user_data: data to pass to @callback
 *
 * Acknowledge @message. Once the message has been acked, @callback will be
 * called. You can then call tp_text_channel_ack_message_finish() to get the
 * result of the operation.
 *
 * A message should be acknowledged once it has been shown to the user by the
 * Handler of the channel. So Observers and Approvers should NOT acknowledge
 * messages themselves.
 * Once a message has been acknowledged, it is removed from the
 * pending-message queue and so the #TpTextChannel::pending-message-removed
 * signal is fired.
 *
 * You should use the #TpSignalledMessage received from
 * tp_text_channel_dup_pending_messages() or the
 * #TpTextChannel::message-received signal.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_ack_message_async (TpTextChannel *self,
    TpMessage *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  TpChannel *chan = (TpChannel *) self;
  GSimpleAsyncResult *result;
  GArray *ids;
  guint id;
  gboolean valid;

  g_return_if_fail (TP_IS_TEXT_CHANNEL (self));
  g_return_if_fail (TP_IS_SIGNALLED_MESSAGE (message));

  id = _tp_signalled_message_get_pending_message_id (message, &valid);
  if (!valid)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (self), callback, user_data,
          TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "Message doesn't have a pending-message-id");

      return;
    }

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_ack_message_async);

  ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
  g_array_append_val (ids, id);

  tp_cli_channel_type_text_call_acknowledge_pending_messages (chan, -1, ids,
      acknowledge_pending_messages_cb, result, NULL, G_OBJECT (self));

  g_array_unref (ids);
}

/**
 * tp_text_channel_ack_message_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult passed to the callback for tp_text_channel_ack_message_async()
 * @error: a #GError to fill
 *
 * Finishes acknowledging a message.
 *
 * Returns: %TRUE if the message has been acked, %FALSE otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_ack_message_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_text_channel_ack_message_async)
}

/**
 * TP_TEXT_CHANNEL_FEATURE_CHAT_STATES:
 *
 * Expands to a call to a function that returns a quark representing the
 * chat states feature on a #TpTextChannel.
 *
 * When this feature is prepared, tp_text_channel_get_chat_state() and the
 * #TpTextChannel::contact-chat-state-changed signal become useful.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.19.0
 */

GQuark
tp_text_channel_get_feature_quark_chat_states (void)
{
  return g_quark_from_static_string ("tp-text-channel-feature-chat-states");
}

/**
 * tp_text_channel_get_chat_state:
 * @self: a channel
 * @contact: a #TpContact
 *
 * Return the chat state for the given contact. If tp_proxy_is_prepared()
 * would return %FALSE for the feature %TP_TEXT_CHANNEL_FEATURE_CHAT_STATES,
 * the result will always be %TP_CHANNEL_CHAT_STATE_INACTIVE.
 *
 * Returns: the chat state for @contact, or %TP_CHANNEL_CHAT_STATE_INACTIVE
 *  if their chat state is not known
 * Since: 0.19.0
 */
TpChannelChatState
tp_text_channel_get_chat_state (TpTextChannel *self,
    TpContact *contact)
{
  gpointer value;
  TpHandle handle;

  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), 0);

  handle = tp_contact_get_handle (contact);

  if (self->priv->chat_states != NULL &&
      g_hash_table_lookup_extended (self->priv->chat_states,
        GUINT_TO_POINTER (handle), NULL, &value))
    {
      return GPOINTER_TO_UINT (value);
    }

  return TP_CHANNEL_CHAT_STATE_INACTIVE;
}

static void
set_chat_state_cb (TpChannel *proxy,
      const GError *error,
      gpointer user_data,
      GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;

  if (error != NULL)
    {
      DEBUG ("SetChatState failed: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
    }

  g_simple_async_result_complete (result);
  g_object_unref (result);
}

/**
 * tp_text_channel_set_chat_state_async:
 * @self: a #TpTextChannel
 * @state: a #TpChannelChatState to set
 * @callback: a callback to call when the chat state has been set
 * @user_data: data to pass to @callback
 *
 * Set the local state on channel @self to @state.
 * Once the state has been set, @callback will be called.
 * You can then call tp_text_channel_set_chat_state_finish() to get the
 * result of the operation.
 *
 * Since: 0.13.10
 */
void
tp_text_channel_set_chat_state_async (TpTextChannel *self,
    TpChannelChatState state,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (G_OBJECT (self), callback,
      user_data, tp_text_channel_set_chat_state_async);

  tp_cli_channel_interface_chat_state1_call_set_chat_state (TP_CHANNEL (self),
      -1, state, set_chat_state_cb, result, NULL, G_OBJECT (self));
}

/**
 * tp_text_channel_set_chat_state_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult passed to the callback for tp_text_channel_set_chat_state_async()
 * @error: a #GError to fill
 *
 * Completes a call to tp_text_channel_set_chat_state_async().
 *
 * Returns: %TRUE if the chat state has been changed, %FALSE otherwise.
 *
 * Since: 0.13.10
 */
gboolean
tp_text_channel_set_chat_state_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  _tp_implement_finish_void (self, tp_text_channel_set_chat_state_async)
}

/**
 * tp_text_channel_get_message_types:
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:message-types property
 *
 * Returns: (transfer none) (element-type TelepathyGLib.ChannelTextMessageType):
 * the value of #TpTextChannel:message-types
 *
 * Since: 0.13.16
 */
GArray *
tp_text_channel_get_message_types (TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), NULL);

  return self->priv->message_types;
}

/**
 * tp_text_channel_supports_message_type:
 * @self: a #TpTextChannel
 * @message_type: a #TpChannelTextMessageType
 *
 * Check if message of type @message_type can be sent on this channel.
 *
 * Returns: %TRUE if message of type @message_type can be sent on @self, %FALSE
 * otherwise
 *
 * Since: 0.13.16
 */
gboolean
tp_text_channel_supports_message_type (TpTextChannel *self,
    TpChannelTextMessageType message_type)
{
  guint i;

  for (i = 0; i < self->priv->message_types->len; i++)
    {
      TpChannelTextMessageType tmp = g_array_index (self->priv->message_types,
          TpChannelTextMessageType, i);

      if (tmp == message_type)
        return TRUE;
    }

  return FALSE;
}

/**
 * TP_TEXT_CHANNEL_FEATURE_SMS:
 *
 * Expands to a call to a function that returns a quark representing the
 * SMS feature of a #TpTextChannel.
 *
 * When this feature is prepared, the TpTextChannel:is-sms-channel property
 * will have a meaningful value and will be updated when needed.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.15.1
 */
GQuark
tp_text_channel_get_feature_quark_sms (void)
{
  return g_quark_from_static_string ("tp-text-channel-feature-sms");
}

/**
 * tp_text_channel_is_sms_channel:
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:is-sms-channel property
 *
 * Returns: the value of #TpTextChannel:is-sms-channel property
 *
 * Since: 0.15.1
 */
gboolean
tp_text_channel_is_sms_channel (TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), FALSE);

  return self->priv->is_sms_channel;
}

/**
 * tp_text_channel_get_sms_flash:
 * @self: a #TpTextChannel
 *
 * Return the #TpTextChannel:sms-flash property
 *
 * Returns: the value of #TpTextChannel:sms-flash property
 *
 * Since: 0.15.1
 */
gboolean
tp_text_channel_get_sms_flash (TpTextChannel *self)
{
  g_return_val_if_fail (TP_IS_TEXT_CHANNEL (self), FALSE);

  return self->priv->sms_flash;
}

typedef struct
{
  guint chunks_required;
  gint remaining_characters;
  gint estimated_cost;
} GetSmsLengthReturn;

static GetSmsLengthReturn *
get_sms_length_return_new (guint chunks_required,
    gint remaining_characters,
    gint estimated_cost)
{
  GetSmsLengthReturn *result = g_slice_new (GetSmsLengthReturn);

  result->chunks_required = chunks_required;
  result->remaining_characters = remaining_characters;
  result->estimated_cost = estimated_cost;

  return result;
}

static void
get_sms_length_return_free (GetSmsLengthReturn *r)
{
  g_slice_free (GetSmsLengthReturn, r);
}

static void
get_sms_length_cb (TpChannel *proxy,
    guint chunks_required,
    gint remaining_characters,
    gint estimated_cost,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  GSimpleAsyncResult *result = user_data;
  GetSmsLengthReturn *r;

  if (error != NULL)
    {
      DEBUG ("Failed to get SMS length: %s", error->message);

      g_simple_async_result_set_from_error (result, error);
      goto out;
    }

  r = get_sms_length_return_new (chunks_required, remaining_characters,
      estimated_cost);

  g_simple_async_result_set_op_res_gpointer (result, r,
      (GDestroyNotify) get_sms_length_return_free);

out:
  g_simple_async_result_complete (result);
}

/**
 * tp_text_channel_get_sms_length_async:
 * @self: a #TpTextChannel
 * @message: a #TpClientMessage
 * @callback: a callback to call when the request has been satisfied
 * @user_data: data to pass to @callback
 *
 * Starts an async call to get the number of 140 octet chunks required to
 * send a #message via SMS on #self, as well as the number of remaining
 * characters available in the final chunk and, if possible,
 * an estimate of the cost.
 *
 * Once the request has been satisfied, @callback will be called.
 * You can then call tp_text_channel_get_sms_length_finish() to get the
 * result of the operation.
 *
 * Since: 0.15.1
 */
void
tp_text_channel_get_sms_length_async (TpTextChannel *self,
    TpMessage *message,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new ((GObject *) self, callback, user_data,
      tp_text_channel_get_sms_length_async);

  tp_cli_channel_interface_sms1_call_get_sms_length ((TpChannel *) self, -1,
      message->parts, get_sms_length_cb, result, g_object_unref,
      G_OBJECT (self));
}


/**
 * tp_text_channel_get_sms_length_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult
 * @chunks_required: (out): if not %NULL used to return
 * the number of 140 octet chunks required to send the message.
 * @remaining_characters: (out): if not %NULL used to return
 * the number of further characters that can be fit in the final chunk.
 * A negative value indicates that the message will be truncated by
 * abs(@remaining_characters).
 * The value #G_MININT32 indicates the message will be truncated by
 * an unknown amount.
 * @estimated_cost: (out): if not %NULL used to return
 * the estimated cost of sending this message.
 * The currency and scale of this value are the same as the
 * values of the #TpConnection:balance-scale and
 * #TpConnection:balance-currency properties.
 * A value of -1 indicates the cost could not be estimated.
 * @error: a #GError to fill
 *
 * Finishes an async SMS length request.
 *
 * Returns: %TRUE if the number of 140 octet chunks required to send
 * the message has been retrieved, %FALSE otherwise.
 *
 * Since: 0.15.1
 */
gboolean
tp_text_channel_get_sms_length_finish (TpTextChannel *self,
    GAsyncResult *result,
    guint *chunks_required,
    gint *remaining_characters,
    gint *estimated_cost,
    GError **error)
{
  GSimpleAsyncResult *simple = (GSimpleAsyncResult *) result;
  GetSmsLengthReturn *r;

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
        G_OBJECT (self), tp_text_channel_get_sms_length_async), FALSE);

  r = g_simple_async_result_get_op_res_gpointer (simple);

  if (chunks_required != NULL)
    *chunks_required = r->chunks_required;

  if (remaining_characters != NULL)
    *remaining_characters = r->remaining_characters;

  if (estimated_cost != NULL)
    *estimated_cost = r->estimated_cost;

  return TRUE;
}

/**
 * tp_text_channel_ack_all_pending_messages_async:
 * @self: a #TpTextChannel
 * @callback: a callback to call when the messages have been acked
 * @user_data: data to pass to @callback
 *
 * Acknowledge all the pending messages. This is equivalent of calling
 * tp_text_channel_ack_messages_async() with the list of #TpSignalledMessage
 * returned by tp_text_channel_dup_pending_messages().
 *
 * Once the messages have been acked, @callback will be called.
 * You can then call tp_text_channel_ack_all_pending_messages_finish() to get
 * the result of the operation.
 *
 * See tp_text_channel_ack_message_async() about acknowledging messages.
 *
 * Since: 0.15.3
 */
void
tp_text_channel_ack_all_pending_messages_async (TpTextChannel *self,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  GList *messages;

  messages = g_queue_peek_head_link (self->priv->pending_messages);

  tp_text_channel_ack_messages_async (self, messages,
      callback, user_data);
}

/**
 * tp_text_channel_ack_all_pending_messages_finish:
 * @self: a #TpTextChannel
 * @result: a #GAsyncResult
 * @error: a #GError to fill
 *
 * Finish an asynchronous acknowledgement operation of all messages.
 *
 * Returns: %TRUE if the messages have been acked, %FALSE otherwise.
 *
 * Since: 0.15.3
 */
gboolean
tp_text_channel_ack_all_pending_messages_finish (TpTextChannel *self,
    GAsyncResult *result,
    GError **error)
{
  return tp_text_channel_ack_messages_finish (self, result, error);
}
