/*
 * message-mixin.c - Source for TpMessageMixin
 * Copyright (C) 2006-2008 Collabora Ltd.
 * Copyright (C) 2006-2008 Nokia Corporation
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
 * SECTION:message-mixin
 * @title: TpMessageMixin
 * @short_description: a mixin implementation of the text channel type
 * @see_also: #TpSvcChannelTypeText
 *  <link linkend="telepathy-glib-dbus-properties-mixin">TpDBusPropertiesMixin</link>
 *
 * This mixin can be added to a channel GObject class to implement the
 * text channel type in a general way.  The channel class should also
 * have a #TpDBusPropertiesMixinClass.
 *
 * To use the messages mixin, include a #TpMessageMixin somewhere in your
 * instance structure, and call tp_message_mixin_init() from your
 * constructor function, and tp_message_mixin_finalize() from your dispose
 * or finalize function. In the class_init function, call
 * tp_message_mixin_init_dbus_properties() to hook this mixin into the D-Bus
 * properties mixin class. Finally, include the following in the fourth
 * argument of G_DEFINE_TYPE_WITH_CODE():
 *
 * <informalexample><programlisting>
 *  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
 *    tp_message_mixin_iface_init);
 * </programlisting></informalexample>
 *
 * To support sending messages, you must call
 * tp_message_mixin_implement_sending() in the constructor function. If you do
 * not, any attempt to send a message will fail with NotImplemented.
 *
 * To support chat state, you must call
 * tp_message_mixin_implement_send_chat_state() in the constructor function, and
 * include the following in the fourth argument of G_DEFINE_TYPE_WITH_CODE():
 *
 * <informalexample><programlisting>
 *  G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_CHAT_STATE1,
 *    tp_message_mixin_chat_state_iface_init);
 * </programlisting></informalexample>
 *
 * Since: 0.7.21
 */

#include "config.h"

#include <telepathy-glib/message-mixin.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <string.h>

#include <telepathy-glib/base-channel.h>
#include <telepathy-glib/cm-message.h>
#include <telepathy-glib/cm-message-internal.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/group-mixin.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/message-internal.h>
#include <telepathy-glib/svc-channel.h>

#define DEBUG_FLAG TP_DEBUG_IM

#include "debug-internal.h"

/**
 * TpMessageMixin:
 *
 * Structure to be included in the instance structure of objects that
 * use this mixin. Initialize it with tp_message_mixin_init().
 *
 * There are no public fields.
 *
 * Since: 0.7.21
 */

struct _TpMessageMixinPrivate
{
  TpBaseConnection *connection;

  /* Sending */
  TpMessageMixinSendImpl send_message;
  GArray *msg_types;
  TpMessagePartSupportFlags message_part_support_flags;
  TpDeliveryReportingSupportFlags delivery_reporting_support_flags;
  gchar **supported_content_types;

  /* Receiving */
  guint recv_id;
  GQueue *pending;

  /* ChatState */

  /* TpHandle -> TpChannelChatState */
  GHashTable *chat_states;
  TpMessageMixinSendChatStateImpl send_chat_state;
  /* FALSE unless at least one chat state notification has been sent; <gone/>
   * will only be sent when the channel closes if this is TRUE. This prevents
   * opening a channel and closing it immediately sending a spurious <gone/> to
   * the peer.
   */
  gboolean send_gone;
};


static const char * const forbidden_keys[] = {
    "pending-message-id",
    NULL
};


static const char * const body_only[] = {
    "alternative",
    "content-type",
    "content",
    "identifier",
    "needs-retrieval",
    "truncated",
    "size",
    NULL
};


static const char * const body_only_incoming[] = {
    "needs-retrieval",
    "truncated",
    "size",
    NULL
};


static const char * const headers_only[] = {
    "message-type",
    "message-sender",
    "message-sender-id",
    "message-sent",
    "message-received",
    "message-token",
    NULL
};


static const char * const headers_only_incoming[] = {
    "message-sender",
    "message-sender-id",
    "message-sent",
    "message-received",
    NULL
};


#define TP_MESSAGE_MIXIN_OFFSET_QUARK (tp_message_mixin_get_offset_quark ())
#define TP_MESSAGE_MIXIN_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_TYPE (o), \
                                       TP_MESSAGE_MIXIN_OFFSET_QUARK)))
#define TP_MESSAGE_MIXIN(o) \
  ((TpMessageMixin *) tp_mixin_offset_cast (o, TP_MESSAGE_MIXIN_OFFSET (o)))

/**
 * tp_message_mixin_get_offset_quark:
 *
 * <!--no documentation beyond Returns: needed-->
 *
 * Returns: the quark used for storing mixin offset on a GObject
 *
 * Since: 0.7.21
 */
static GQuark
tp_message_mixin_get_offset_quark (void)
{
  static GQuark offset_quark = 0;

  if (G_UNLIKELY (offset_quark == 0))
    offset_quark = g_quark_from_static_string (
        "tp_message_mixin_get_offset_quark@0.7.7");

  return offset_quark;
}


static gint
pending_item_id_equals_data (gconstpointer item,
                             gconstpointer data)
{
  const TpCMMessage *self = item;
  guint id = GPOINTER_TO_UINT (data);

  /* The sense of this comparison is correct: the callback passed to
   * g_queue_find_custom() should return 0 when the desired item is found.
   */
  return (self->incoming_id != id);
}

/**
 * TpMessageMixinSendImpl:
 * @object: An instance of the implementation that uses this mixin
 * @message: An outgoing message
 * @flags: flags with which to send the message
 *
 * Signature of a virtual method which may be implemented to allow messages
 * to be sent. It must arrange for tp_message_mixin_sent() to be called when
 * the message has submitted or when message submission has failed.
 */


/**
 * tp_message_mixin_implement_sending:
 * @object: An instance of the implementation that uses this mixin
 * @send: An implementation of SendMessage()
 * @n_types: Number of supported message types
 * @types: @n_types supported message types
 * @message_part_support_flags: Flags indicating what message part structures
 *  are supported
 * @delivery_reporting_support_flags: Flags indicating what kind of delivery
 *  reports are supported
 * @supported_content_types: The supported content types
 *
 * Set the callback used to implement SendMessage, and the types of message
 * that can be sent. This must be called from the init, constructor or
 * constructed callback, after tp_message_mixin_init(), and may only be called
 * once per object.
 *
 * Since: 0.7.21
 */
void
tp_message_mixin_implement_sending (GObject *object,
                                    TpMessageMixinSendImpl send,
                                    guint n_types,
                                    const TpChannelTextMessageType *types,
                                    TpMessagePartSupportFlags
                                        message_part_support_flags,
                                    TpDeliveryReportingSupportFlags
                                        delivery_reporting_support_flags,
                                    const gchar * const *
                                        supported_content_types)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (mixin->priv->send_message == NULL);
  mixin->priv->send_message = send;

  if (mixin->priv->msg_types->len > 0)
    g_array_remove_range (mixin->priv->msg_types, 0,
        mixin->priv->msg_types->len);

  g_assert (mixin->priv->msg_types->len == 0);
  g_array_append_vals (mixin->priv->msg_types, types, n_types);

  mixin->priv->message_part_support_flags = message_part_support_flags;
  mixin->priv->delivery_reporting_support_flags = delivery_reporting_support_flags;

  g_strfreev (mixin->priv->supported_content_types);
  mixin->priv->supported_content_types = g_strdupv (
      (gchar **) supported_content_types);
}

static TpChannelChatState
lookup_current_chat_state (TpMessageMixin *mixin,
    TpHandle member)
{
  gpointer tmp;

  if (g_hash_table_lookup_extended (mixin->priv->chat_states,
          GUINT_TO_POINTER (member), NULL, &tmp))
    {
      return GPOINTER_TO_UINT (tmp);
    }

  return TP_CHANNEL_CHAT_STATE_INACTIVE;
}

/**
 * tp_message_mixin_change_chat_state:
 * @object: an instance of the implementation that uses this mixin
 * @member: a member of this chat
 * @state: the new state to set
 *
 * Change the current chat state of @member to be @state. This emits
 * ChatStateChanged signal and update ChatStates property.
 *
 * Since: 0.19.0
 */
void
tp_message_mixin_change_chat_state (GObject *object,
    TpHandle member,
    TpChannelChatState state)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (state < TP_NUM_CHANNEL_CHAT_STATES);

  if (state == lookup_current_chat_state (mixin, member))
    return;

  if (state == TP_CHANNEL_CHAT_STATE_INACTIVE ||
      state == TP_CHANNEL_CHAT_STATE_GONE)
    {
      g_hash_table_remove (mixin->priv->chat_states,
          GUINT_TO_POINTER (member));
    }
  else
    {
      g_hash_table_insert (mixin->priv->chat_states,
          GUINT_TO_POINTER (member),
          GUINT_TO_POINTER (state));
    }

  tp_svc_channel_interface_chat_state1_emit_chat_state_changed (object,
      member, state);
}

/**
 * TpMessageMixinSendChatStateImpl:
 * @object: an instance of the implementation that uses this mixin
 * @state: a #TpChannelChatState to be send
 * @error: a #GError to fill
 *
 * Signature of a virtual method which may be implemented to allow sending chat
 * state.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 * Since: 0.19.0
 */

/**
 * tp_message_mixin_implement_send_chat_state:
 * @object: an instance of the implementation that uses this mixin
 * @send_chat_state: send our chat state
 *
 * Set the callback used to implement SetChatState. This must be called from the
 * init, constructor or constructed callback, after tp_message_mixin_init(),
 * and may only be called once per object.
 *
 * Since: 0.19.0
 */
void
tp_message_mixin_implement_send_chat_state (GObject *object,
    TpMessageMixinSendChatStateImpl send_chat_state)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (mixin->priv->send_chat_state == NULL);

  mixin->priv->send_chat_state = send_chat_state;
}

/**
 * tp_message_mixin_maybe_send_gone:
 * @object: An instance of the implementation that uses this mixin
 *
 * Send #TP_CHANNEL_CHAT_STATE_GONE if needed. This should be called on private
 * chats when channel is closed.
 *
 * Since: 0.19.0
 */
void
tp_message_mixin_maybe_send_gone (GObject *object)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  if (mixin->priv->send_gone && !TP_HAS_GROUP_MIXIN (object) &&
      mixin->priv->send_chat_state != NULL)
    {
      mixin->priv->send_chat_state (object, TP_CHANNEL_CHAT_STATE_GONE, NULL);
    }

  mixin->priv->send_gone = FALSE;
}

static void
tp_message_mixin_set_chat_state_async (TpSvcChannelInterfaceChatState1 *iface,
    guint state,
    DBusGMethodInvocation *context)
{
  GObject *object = (GObject *) iface;
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  GError *error = NULL;

  if (mixin->priv->send_chat_state == NULL)
    {
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  if (state >= TP_NUM_CHANNEL_CHAT_STATES)
    {
      DEBUG ("invalid chat state %u", state);

      g_set_error (&error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "invalid state: %u", state);
      goto error;
    }

  if (state == TP_CHANNEL_CHAT_STATE_GONE)
    {
      /* We cannot explicitly set the Gone state */
      DEBUG ("you may not explicitly set the Gone state");

      g_set_error (&error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "you may not explicitly set the Gone state");
      goto error;
    }

  if (!mixin->priv->send_chat_state (object, state, &error))
    goto error;

  mixin->priv->send_gone = TRUE;
  tp_message_mixin_change_chat_state (object,
      tp_base_channel_get_self_handle ((TpBaseChannel *) object),
      state);

  tp_svc_channel_interface_chat_state1_return_from_set_chat_state (context);
  return;

error:
  dbus_g_method_return_error (context, error);
  g_clear_error (&error);
}

/**
 * tp_message_mixin_init:
 * @obj: An instance of the implementation that uses this mixin
 * @offset: The byte offset of the TpMessageMixin within the object structure
 * @connection: A #TpBaseConnection
 *
 * Initialize the mixin. Should be called from the implementation's
 * instance init function or constructor like so:
 *
 * <informalexample><programlisting>
 * tp_message_mixin_init ((GObject *) self,
 *     G_STRUCT_OFFSET (SomeObject, message_mixin),
 *     self->connection);
 * </programlisting></informalexample>
 *
 * Since 0.99.1 @obj must be a #TpBaseChannel subclass.
 *
 * Since: 0.7.21
 */
void
tp_message_mixin_init (GObject *obj,
                       gsize offset,
                       TpBaseConnection *connection)
{
  TpMessageMixin *mixin;

  g_assert (TP_IS_BASE_CHANNEL (obj));

  g_type_set_qdata (G_OBJECT_TYPE (obj),
                    TP_MESSAGE_MIXIN_OFFSET_QUARK,
                    GINT_TO_POINTER (offset));

  mixin = TP_MESSAGE_MIXIN (obj);

  mixin->priv = g_slice_new0 (TpMessageMixinPrivate);

  mixin->priv->pending = g_queue_new ();
  mixin->priv->recv_id = 0;
  mixin->priv->msg_types = g_array_sized_new (FALSE, FALSE, sizeof (guint),
      TP_NUM_CHANNEL_TEXT_MESSAGE_TYPES);
  mixin->priv->connection = g_object_ref (connection);

  mixin->priv->supported_content_types = g_new0 (gchar *, 1);

  mixin->priv->chat_states = g_hash_table_new (NULL, NULL);
}


/**
 * tp_message_mixin_clear:
 * @obj: An object with this mixin
 *
 * Clear the pending message queue, deleting all messages without emitting
 * PendingMessagesRemoved.
 */
void
tp_message_mixin_clear (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);
  TpMessage *item;

  while ((item = g_queue_pop_head (mixin->priv->pending)) != NULL)
    {
      g_object_unref (item);
    }
}


/**
 * tp_message_mixin_finalize:
 * @obj: An object with this mixin.
 *
 * Free resources held by the text mixin.
 *
 * Since: 0.7.21
 */
void
tp_message_mixin_finalize (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);

  DEBUG ("%p", obj);

  tp_message_mixin_clear (obj);
  g_assert (g_queue_is_empty (mixin->priv->pending));
  g_queue_free (mixin->priv->pending);
  g_array_unref (mixin->priv->msg_types);
  g_strfreev (mixin->priv->supported_content_types);

  g_object_unref (mixin->priv->connection);

  g_hash_table_unref (mixin->priv->chat_states);

  g_slice_free (TpMessageMixinPrivate, mixin->priv);
}

static void
tp_message_mixin_acknowledge_pending_messages_async (
    TpSvcChannelTypeText *iface,
    const GArray *ids,
    DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  GPtrArray *links = g_ptr_array_sized_new (ids->len);
  TpIntset *seen = tp_intset_new ();
  guint i;

  for (i = 0; i < ids->len; i++)
    {
      guint id = g_array_index (ids, guint, i);
      GList *link_;

      if (tp_intset_is_member (seen, id))
        {
          gchar *client = dbus_g_method_get_sender (context);

          DEBUG ("%s passed message id %u more than once in one call to "
              "AcknowledgePendingMessages. Foolish pup.", client, id);
          g_free (client);
          continue;
        }

      tp_intset_add (seen, id);
      link_ = g_queue_find_custom (mixin->priv->pending,
          GUINT_TO_POINTER (id), pending_item_id_equals_data);

      if (link_ == NULL)
        {
          GError *error = g_error_new (TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
              "invalid message id %u", id);

          DEBUG ("%s", error->message);
          dbus_g_method_return_error (context, error);
          g_error_free (error);

          g_ptr_array_unref (links);
          tp_intset_destroy (seen);
          return;
        }

      g_ptr_array_add (links, link_);
    }

  tp_svc_channel_type_text_emit_pending_messages_removed (iface,
      ids);

  for (i = 0; i < links->len; i++)
    {
      GList *link_ = g_ptr_array_index (links, i);
      TpMessage *item = link_->data;
      TpCMMessage *cm_msg = link_->data;

      DEBUG ("acknowledging message id %u", cm_msg->incoming_id);

      g_queue_delete_link (mixin->priv->pending, link_);
      g_object_unref (item);
    }

  g_ptr_array_unref (links);
  tp_intset_destroy (seen);
  tp_svc_channel_type_text_return_from_acknowledge_pending_messages (context);
}

static void
queue_pending (GObject *object, TpMessage *pending)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);

  g_queue_push_tail (mixin->priv->pending, pending);

  tp_svc_channel_type_text_emit_message_received (object,
      pending->parts);
}


/**
 * tp_message_mixin_take_received:
 * @object: a channel with this mixin
 * @message: the message. Its ownership is claimed by the message
 *  mixin, so it must no longer be modified or freed
 *
 * Receive a message into the pending messages queue, where it will stay
 * until acknowledged, and emit the Received and ReceivedMessage signals. Also
 * emit the SendError signal if the message is a failed delivery report.
 *
 * Returns: the message ID
 *
 * Since: 0.7.21
 */
guint
tp_message_mixin_take_received (GObject *object,
                                TpMessage *message)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  TpCMMessage *cm_msg = (TpCMMessage *) message;
  GHashTable *header;

  g_return_val_if_fail (cm_msg->incoming_id == G_MAXUINT32, 0);
  g_return_val_if_fail (message->parts->len >= 1, 0);

  header = g_ptr_array_index (message->parts, 0);

  g_return_val_if_fail (g_hash_table_lookup (header, "pending-message-id")
      == NULL, 0);

  /* FIXME: we don't check for overflow, so in highly pathological cases we
   * might end up with multiple messages with the same ID */
  cm_msg->incoming_id = mixin->priv->recv_id++;

  tp_message_set_uint32 (message, 0, "pending-message-id",
      cm_msg->incoming_id);

  if (tp_asv_get_uint64 (header, "message-received", NULL) == 0)
    tp_message_set_uint64 (message, 0, "message-received",
        time (NULL));

  /* Here we add the message to the incoming queue: Although we have not
   * returned the message ID to the caller directly at this point, we
   * have poked it into the TpMessage, which the caller (and anyone connected
   * to the relevant signals) has access to, so there isn't actually a race
   * between putting the message into the queue and making its ID available.
   */
  queue_pending (object, message);

  return cm_msg->incoming_id;
}


/**
 * tp_message_mixin_has_pending_messages:
 * @object: An object with this mixin
 * @first_sender: If not %NULL, used to store the sender of the oldest pending
 *  message
 *
 * Return whether the channel @obj has unacknowledged messages. If so, and
 * @first_sender is not %NULL, the handle of the sender of the first message
 * is placed in it, without incrementing the handle's reference count.
 *
 * Returns: %TRUE if there are pending messages
 */
gboolean
tp_message_mixin_has_pending_messages (GObject *object,
                                       TpHandle *first_sender)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  TpMessage *msg = g_queue_peek_head (mixin->priv->pending);

  if (msg != NULL && first_sender != NULL)
    {
      GVariant *header = tp_message_dup_part (msg, 0);
      TpHandle h;

      if (g_variant_lookup (header, "message-sender", "u", &h))
        *first_sender = h;
      else
        WARNING ("oldest message's message-sender is mistyped");

      g_variant_unref (header);
    }

  return (msg != NULL);
}


/**
 * tp_message_mixin_set_rescued:
 * @obj: An object with this mixin
 *
 * Mark all pending messages as having been "rescued" from a channel that
 * previously closed.
 */
void
tp_message_mixin_set_rescued (GObject *obj)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (obj);
  GList *cur;

  for (cur = g_queue_peek_head_link (mixin->priv->pending);
       cur != NULL;
       cur = cur->next)
    {
      TpMessage *msg = cur->data;

      tp_message_set_boolean (msg, 0, "rescued", TRUE);
    }
}


/**
 * TpMessageMixinOutgoingMessage:
 * @flags: Flags indicating how this message should be sent
 * @parts: The parts that make up the message (an array of #GHashTable,
 *  with the first one containing message headers)
 * @priv: Pointer to opaque private data used by the messages mixin
 *
 * Structure representing a message which is to be sent.
 *
 * Connection managers may (and should) edit the @parts in-place to remove
 * keys that could not be sent, using g_hash_table_remove(). Connection
 * managers may also alter @parts to include newly allocated GHashTable
 * structures.
 *
 * However, they must not add keys to an existing GHashTable (this is because
 * the connection manager has no way to know how the keys and values will be
 * freed).
 *
 * Since: 0.7.21
 */


struct _TpMessageMixinOutgoingMessagePrivate {
    DBusGMethodInvocation *context;
    gboolean messages:1;
};

/**
 * tp_message_mixin_sent:
 * @object: An object implementing the Text interface with this mixin
 * @message: The outgoing message
 * @flags: The flags used when sending the message, which may be a subset of
 *  those passed to the #TpMessageMixinSendImpl implementation if not all are
 *  supported, or 0 on error.
 * @token: A token representing the sent message (see the Telepathy D-Bus API
 *  specification), or an empty string if no suitable identifier is available,
 *  or %NULL on error
 * @error: %NULL on success, or the error with which message submission failed
 *
 * Indicate to the message mixin that message submission to the IM server has
 * succeeded or failed. This should be called as soon as the CM determines
 * it's theoretically possible to send the message (e.g. the parameters are
 * supported and correct).
 *
 * After this function is called, @message will have been freed, and must not
 * be dereferenced.
 *
 * Since: 0.7.21
 */
void
tp_message_mixin_sent (GObject *object,
                       TpMessage *message,
                       TpMessageSendingFlags flags,
                       const gchar *token,
                       const GError *error)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (object);
  TpCMMessage *cm_msg = (TpCMMessage *) message;

  g_return_if_fail (mixin != NULL);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (TP_IS_CM_MESSAGE (message));
  g_return_if_fail (message->parts != NULL);
  g_return_if_fail (cm_msg->outgoing_context != NULL);
  g_return_if_fail (token == NULL || error == NULL);
  g_return_if_fail (token != NULL || error != NULL);

  if (error != NULL)
    {
      GError *e = g_error_copy (error);

      dbus_g_method_return_error (cm_msg->outgoing_context, e);
      g_error_free (e);
    }
  else
    {
      GHashTable *header = g_ptr_array_index (message->parts, 0);

      mixin->priv->send_gone = TRUE;

      if (tp_asv_get_uint64 (header, "message-sent", NULL) == 0)
        tp_message_set_uint64 (message, 0, "message-sent", time (NULL));

      tp_cm_message_set_sender (message,
          tp_base_channel_get_self_handle ((TpBaseChannel *) object));

      /* emit Sent and MessageSent */

      tp_svc_channel_type_text_emit_message_sent (object,
          message->parts, flags, token);

      /* return successfully */

      tp_svc_channel_type_text_return_from_send_message (
          cm_msg->outgoing_context, token);
    }

  cm_msg->outgoing_context = NULL;
  g_object_unref (message);
}

static void
tp_message_mixin_send_message_async (TpSvcChannelTypeText *iface,
                                     const GPtrArray *parts,
                                     guint flags,
                                     DBusGMethodInvocation *context)
{
  TpMessageMixin *mixin = TP_MESSAGE_MIXIN (iface);
  TpMessage *message;
  TpCMMessage *cm_msg;
  GHashTable *header;
  guint i;
  const char * const *iter;

  if (mixin->priv->send_message == NULL)
    {
      tp_dbus_g_method_return_not_implemented (context);
      return;
    }

  /* it must have at least a header part */
  if (parts->len < 1)
    {
      GError e = { TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
        "Cannot send a message that does not have at least one part" };

      dbus_g_method_return_error (context, &e);
      return;
    }

  header = g_ptr_array_index (parts, 0);

  for (i = 0; i < parts->len; i++)
    {
      for (iter = forbidden_keys; *iter != NULL; iter++)
        {
          if (g_hash_table_lookup (header, *iter) != NULL)
            {
              GError *error = g_error_new (TP_ERROR,
                  TP_ERROR_INVALID_ARGUMENT,
                  "Key '%s' not allowed in a sent message", *iter);

              dbus_g_method_return_error (context, error);
              return;
            }
        }
    }

  for (iter = body_only; *iter != NULL; iter++)
    {
      if (g_hash_table_lookup (header, *iter) != NULL)
        {
          GError *error = g_error_new (TP_ERROR,
              TP_ERROR_INVALID_ARGUMENT,
              "Key '%s' not allowed in a message header", *iter);

          dbus_g_method_return_error (context, error);
          return;
        }
    }

  for (iter = headers_only_incoming; *iter != NULL; iter++)
    {
      if (g_hash_table_lookup (header, *iter) != NULL)
        {
          GError *error = g_error_new (TP_ERROR,
              TP_ERROR_INVALID_ARGUMENT,
              "Key '%s' not allowed in an outgoing message header", *iter);

          dbus_g_method_return_error (context, error);
          return;
        }
    }

  for (i = 1; i < parts->len; i++)
    {
      for (iter = headers_only; *iter != NULL; iter++)
        {
          if (g_hash_table_lookup (g_ptr_array_index (parts, i), *iter)
              != NULL)
            {
              GError *error = g_error_new (TP_ERROR,
                  TP_ERROR_INVALID_ARGUMENT,
                  "Key '%s' not allowed in a message body", *iter);

              dbus_g_method_return_error (context, error);
              return;
            }
        }
    }

  message = tp_cm_message_new (mixin->priv->connection, parts->len);
  cm_msg = (TpCMMessage *) message;

  for (i = 0; i < parts->len; i++)
    {
      tp_g_hash_table_update (g_ptr_array_index (message->parts, i),
          g_ptr_array_index (parts, i),
          (GBoxedCopyFunc) g_strdup,
          (GBoxedCopyFunc) tp_g_value_slice_dup);
    }

  cm_msg->outgoing_context = context;

  mixin->priv->send_message ((GObject *) iface, message, flags);
}


/**
 * tp_message_mixin_init_dbus_properties:
 * @cls: The class of an object with this mixin
 *
 * Set up a #TpDBusPropertiesMixinClass to use this mixin's implementation
 * of the text channel interface's properties.
 *
 * This uses tp_message_mixin_get_dbus_property() as the property getter
 * and sets a list of the supported properties for it.
 */
void
tp_message_mixin_init_dbus_properties (GObjectClass *cls)
{
  static TpDBusPropertiesMixinPropImpl props[] = {
      { "PendingMessages", NULL, NULL },
      { "SupportedContentTypes", NULL, NULL },
      { "MessagePartSupportFlags", NULL, NULL },
      { "MessageTypes", NULL, NULL },
      { "DeliveryReportingSupport", NULL, NULL },
      { NULL }
  };
  static TpDBusPropertiesMixinPropImpl chat_state_props[] = {
      { "ChatStates", NULL, NULL },
      { NULL }
  };
  GType type = G_OBJECT_CLASS_TYPE (cls);

  g_return_if_fail (g_type_is_a (type, TP_TYPE_SVC_CHANNEL_TYPE_TEXT));

  tp_dbus_properties_mixin_implement_interface (cls,
      TP_IFACE_QUARK_CHANNEL_TYPE_TEXT,
      tp_message_mixin_get_dbus_property, NULL, props);

  if (g_type_is_a (type, TP_TYPE_SVC_CHANNEL_INTERFACE_CHAT_STATE1))
    {
      tp_dbus_properties_mixin_implement_interface (cls,
          TP_IFACE_QUARK_CHANNEL_INTERFACE_CHAT_STATE1,
          tp_message_mixin_get_dbus_property, NULL, chat_state_props);
    }
}


/**
 * tp_message_mixin_get_dbus_property:
 * @object: An object with this mixin
 * @interface: Must be %TP_IFACE_QUARK_CHANNEL_TYPE_TEXT
 * @name: A quark representing the D-Bus property name, either
 *  "PendingMessages", "SupportedContentTypes" or "MessagePartSupportFlags"
 * @value: A GValue pre-initialized to the right type, into which to put
 *  the value
 * @unused: Ignored
 *
 * An implementation of #TpDBusPropertiesMixinGetter which assumes
 * that the @object has the messages mixin. It can only be used for
 * the Text channel type interface.
 */
void
tp_message_mixin_get_dbus_property (GObject *object,
                                    GQuark interface,
                                    GQuark name,
                                    GValue *value,
                                    gpointer unused G_GNUC_UNUSED)
{
  TpMessageMixin *mixin;
  static GQuark q_pending_messages = 0;
  static GQuark q_supported_content_types = 0;
  static GQuark q_message_part_support_flags = 0;
  static GQuark q_delivery_reporting_support_flags = 0;
  static GQuark q_message_types = 0;
  static GQuark q_chat_states = 0;

  if (G_UNLIKELY (q_pending_messages == 0))
    {
      q_pending_messages = g_quark_from_static_string ("PendingMessages");
      q_supported_content_types =
          g_quark_from_static_string ("SupportedContentTypes");
      q_message_part_support_flags =
          g_quark_from_static_string ("MessagePartSupportFlags");
      q_delivery_reporting_support_flags =
          g_quark_from_static_string ("DeliveryReportingSupport");
      q_message_types =
          g_quark_from_static_string ("MessageTypes");
      q_chat_states =
          g_quark_from_static_string ("ChatStates");
    }

  mixin = TP_MESSAGE_MIXIN (object);

  g_return_if_fail (interface == TP_IFACE_QUARK_CHANNEL_TYPE_TEXT ||
      interface == TP_IFACE_QUARK_CHANNEL_INTERFACE_CHAT_STATE1);
  g_return_if_fail (object != NULL);
  g_return_if_fail (name != 0);
  g_return_if_fail (value != NULL);
  g_return_if_fail (mixin != NULL);

  if (name == q_pending_messages)
    {
      GPtrArray *arrays = g_ptr_array_sized_new (g_queue_get_length (
            mixin->priv->pending));
      GList *l;
      GType type = dbus_g_type_get_collection ("GPtrArray",
          TP_HASH_TYPE_MESSAGE_PART);

      for (l = g_queue_peek_head_link (mixin->priv->pending);
           l != NULL;
           l = g_list_next (l))
        {
          TpMessage *msg = l->data;

          g_ptr_array_add (arrays, g_boxed_copy (type, msg->parts));
        }

      g_value_take_boxed (value, arrays);
    }
  else if (name == q_message_part_support_flags)
    {
      g_value_set_uint (value, mixin->priv->message_part_support_flags);
    }
  else if (name == q_delivery_reporting_support_flags)
    {
      g_value_set_uint (value, mixin->priv->delivery_reporting_support_flags);
    }
  else if (name == q_supported_content_types)
    {
      g_value_set_boxed (value, mixin->priv->supported_content_types);
    }
  else if (name == q_message_types)
    {
      g_value_set_boxed (value, mixin->priv->msg_types);
    }
  else if (name == q_chat_states)
    {
      g_value_set_boxed (value, mixin->priv->chat_states);
    }
}


/**
 * tp_message_mixin_iface_init:
 * @g_iface: A pointer to the #TpSvcChannelTypeTextClass in an object class
 * @iface_data: Ignored
 *
 * Fill in this mixin's Text method implementations in the given interface
 * vtable.
 *
 * Since: 0.7.21
 */
void
tp_message_mixin_iface_init (gpointer g_iface,
                             gpointer iface_data)
{
  TpSvcChannelTypeTextClass *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_type_text_implement_##x (klass,\
    tp_message_mixin_##x##_async)
  IMPLEMENT (acknowledge_pending_messages);
  IMPLEMENT (send_message);
#undef IMPLEMENT
}

/**
 * tp_message_mixin_chat_state_iface_init:
 * @g_iface: A pointer to the #TpSvcChannelInterfaceChatState1Class in an object
 *  class
 * @iface_data: Ignored
 *
 * Fill in this mixin's ChatState method implementations in the given interface
 * vtable.
 *
 * Since: 0.19.0
 */
void
tp_message_mixin_chat_state_iface_init (gpointer g_iface,
                                        gpointer iface_data)
{
  TpSvcChannelInterfaceChatState1Class *klass = g_iface;

#define IMPLEMENT(x) tp_svc_channel_interface_chat_state1_implement_##x (\
    klass, tp_message_mixin_##x##_async)
  IMPLEMENT (set_chat_state);
#undef IMPLEMENT
}
