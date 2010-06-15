/*
 * channel.c - proxy for a Telepathy channel
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#include "telepathy-glib/channel-internal.h"

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/proxy-internal.h"
#include "telepathy-glib/_gen/signals-marshal.h"

#include "_gen/tp-cli-channel-body.h"

/**
 * SECTION:channel
 * @title: TpChannel
 * @short_description: proxy object for a Telepathy channel
 * @see_also: #TpConnection, channel-group, channel-text, channel-media
 *
 * #TpChannel objects provide convenient access to Telepathy channels.
 *
 * Compared with a simple proxy for method calls, they add the following
 * features:
 *
 * * calling GetChannelType(), GetInterfaces(), GetHandles() automatically
 *
 * This section also documents the auto-generated C wrappers for the
 * Channel D-Bus interface. Of these, in general, only
 * tp_cli_channel_call_close() and tp_cli_channel_run_close() are useful (the
 * #TpChannel object provides a more convenient API for the rest).
 *
 * Since: 0.7.1
 */


/**
 * TpChannelClass:
 * @parent_class: parent class
 *
 * The class of a #TpChannel. In addition to @parent_class there are four
 * pointers reserved for possible future use.
 *
 * (Changed in 0.7.12: the layout of the structure is visible, allowing
 * subclassing.)
 *
 * Since: 0.7.1
 */


/**
 * TpChannel:
 *
 * A proxy object for a Telepathy channel.
 * A proxy object for a Telepathy channel. There are no interesting
 * public struct fields.
 *
 * (Changed in 0.7.12: the layout of the structure is visible, allowing
 * subclassing.)
 *
 * Since: 0.7.1
 */


enum
{
  PROP_CONNECTION = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_IDENTIFIER,
  PROP_CHANNEL_READY,
  PROP_CHANNEL_PROPERTIES,
  PROP_GROUP_SELF_HANDLE,
  PROP_GROUP_FLAGS,
  N_PROPS
};

enum {
  SIGNAL_GROUP_FLAGS_CHANGED,
  SIGNAL_GROUP_MEMBERS_CHANGED,
  SIGNAL_GROUP_MEMBERS_CHANGED_DETAILED,
  SIGNAL_CHAT_STATE_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };


G_DEFINE_TYPE_WITH_CODE (TpChannel,
    tp_channel,
    TP_TYPE_PROXY,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL));

/**
 * TP_CHANNEL_FEATURE_CORE:
 *
 * Expands to a call to a function that returns a quark for the "core" feature
 * on a #TpChannel.
 *
 * When this feature is prepared, the basic Channel properties of the
 * Channel have been retrieved and are available for use.
 *
 * Specifically, this implies that:
 *
 * - #TpChannel:channel-type is set
 * - #TpChannel:handle-type and #TpChannel:handle are set
 * - any extra interfaces will have been set up in TpProxy (i.e.
 *   #TpProxy:interfaces contains at least all extra Channel interfaces)
 *
 * (These are a subset of the guarantees offered by the older #TpChannel:ready
 * and tp_channel_call_when_ready() mechanisms, which are now equivalent to
 * (%TP_CHANNEL_FEATURE_CORE, %TP_CHANNEL_FEATURE_GROUP) if the channel is
 * a group, or just %TP_CHANNEL_FEATURE_CORE otherwise.)
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.11.3
 */

GQuark
tp_channel_get_feature_quark_core (void)
{
  return g_quark_from_static_string ("tp-channel-feature-core");
}

/**
 * TP_CHANNEL_FEATURE_GROUP:
 *
 * Expands to a call to a function that returns a quark representing the Group
 * features of a TpChannel.
 *
 * When this feature is prepared, the Group properties of the
 * Channel have been retrieved and are available for use, and
 * change-notification has been set up for those that can change:
 *
 * - the initial value of the #TpChannel:group-self-handle property will
 *   have been fetched and change notification will have been set up
 * - the initial value of the #TpChannel:group-flags property will
 *   have been fetched and change notification will have been set up
 *
 * (These are the same guarantees offered for Group channels by the older
 * #TpChannel:ready and tp_channel_call_when_ready() mechanisms.)
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.11.3
 */

GQuark
tp_channel_get_feature_quark_group (void)
{
  return g_quark_from_static_string ("tp-channel-feature-group");
}

/**
 * TP_CHANNEL_FEATURE_CHAT_STATES:
 *
 * Expands to a call to a function that returns a quark representing the
 * chat states feature on a #TpChannel.
 *
 * When this feature is prepared, tp_channel_get_chat_state() and the
 * #TpChannel::chat-state-changed signal become useful.
 *
 * One can ask for a feature to be prepared using the
 * tp_proxy_prepare_async() function, and waiting for it to callback.
 *
 * Since: 0.11.3
 */

GQuark
tp_channel_get_feature_quark_chat_states (void)
{
  return g_quark_from_static_string ("tp-channel-feature-chat-states");
}


/* Convenient property accessors for C (these duplicate the properties) */


/**
 * tp_channel_get_channel_type:
 * @self: a channel
 *
 * Get the D-Bus interface name representing this channel's type,
 * if it has been discovered.
 *
 * This is the same as the #TpChannel:channel-type property; it isn't
 * guaranteed to be non-%NULL until the %TP_CHANNEL_FEATURE_CORE feature has
 * been prepared.
 *
 * Returns: the channel type, if the channel is ready; either the channel
 *  type or %NULL, if the channel is not yet ready.
 * Since: 0.7.12
 */
const gchar *
tp_channel_get_channel_type (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return g_quark_to_string (self->priv->channel_type);
}


/**
 * tp_channel_get_channel_type_id:
 * @self: a channel
 *
 * Get the D-Bus interface name representing this channel's type, as a GQuark,
 * if it has been discovered.
 *
 * This is the same as the #TpChannel:channel-type property, except that it
 * is a GQuark rather than a string. It isn't guaranteed to be nonzero until
 * the %TP_CHANNEL_FEATURE_CORE property is ready.
 *
 * Returns: the channel type, if the channel is ready; either the channel
 *  type or 0, if the channel is not yet ready.
 * Since: 0.7.12
 */
GQuark
tp_channel_get_channel_type_id (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), 0);

  return self->priv->channel_type;
}


/**
 * tp_channel_get_handle:
 * @self: a channel
 * @handle_type: (out): if not %NULL, used to return the type of this handle
 *
 * Get the handle representing the contact, chatroom, etc. with which this
 * channel communicates for its whole lifetime, or 0 if there is no such
 * handle or it has not yet been discovered.
 *
 * This is the same as the #TpChannel:handle property. It isn't guaranteed to
 * have its final value until the %TP_CHANNEL_FEATURE_CORE property is ready.
 *
 * If %handle_type is not %NULL, the type of handle is written into it.
 * This will be %TP_UNKNOWN_HANDLE_TYPE if the handle has not yet been
 * discovered, or %TP_HANDLE_TYPE_NONE if there is no handle with which this
 * channel will always communicate. This is the same as the
 * #TpChannel:handle-type property.
 *
 * Returns: the handle
 * Since: 0.7.12
 */
TpHandle
tp_channel_get_handle (TpChannel *self,
                       TpHandleType *handle_type)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), 0);

  if (handle_type != NULL)
    {
      *handle_type = self->priv->handle_type;
    }

  return self->priv->handle;
}

/**
 * tp_channel_get_identifier:
 * @self: a channel
 *
 * This channel's associated identifier, or the empty string if no identifier
 * or unknown.
 *
 * This is the same as the #TpChannel:identifier property, and isn't guaranteed
 * to be set until the %TP_CHANNEL_FEATURE_CORE property is ready.
 *
 * Changed in 0.11.4: as with #TpChannel:identifier, this could
 * previously either be %NULL or the empty string if there was no suitable
 * value. It is now non-%NULL in all cases.
 *
 * Returns: the identifier
 * Since: 0.7.21
 */
const gchar *
tp_channel_get_identifier (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  if (self->priv->identifier == NULL)
    return "";

  return self->priv->identifier;
}

/**
 * tp_channel_is_ready: (skip)
 * @self: a channel
 *
 * Returns the same thing as the #TpChannel:channel-ready property.
 *
 * New code should use tp_proxy_is_prepared(), which is a more general form of
 * this method.
 *
 * For group channels, this method is equivalent to checking for the
 * combination of %TP_CHANNEL_FEATURE_CORE and %TP_CHANNEL_FEATURE_GROUP; for
 * non-group channels, it's equivalent to checking for
 * %TP_CHANNEL_FEATURE_CORE.
 *
 * One important difference is that after #TpChannel::invalidated is
 * signalled, #TpChannel:channel-ready keeps its current value - which might
 * be %TRUE, if the channel was successfully prepared before it became
 * invalidated - but tp_proxy_is_prepared() returns %FALSE for all features.
 *
 * Returns: %TRUE if introspection has completed
 * Since: 0.7.12
 */
gboolean
tp_channel_is_ready (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), FALSE);

  return self->priv->ready;
}


/**
 * tp_channel_borrow_connection:
 * @self: a channel
 *
 * Returns the connection for this channel. The returned pointer is only valid
 * while this channel is valid - reference it with g_object_ref() if needed.
 *
 * Returns: (transfer none): the value of #TpChannel:connection
 * Since: 0.7.12
 */
TpConnection *
tp_channel_borrow_connection (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->connection;
}


/**
 * tp_channel_borrow_immutable_properties:
 * @self: a channel
 *
 * Returns the immutable D-Bus properties of this channel, the same as
 * #TpChannel:channel-properties.
 *
 * The returned hash table should not be altered, and is not necessarily
 * valid after the main loop is next re-entered. Copy it with
 * g_boxed_copy() (its type is %TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP)
 * if a copy that remains valid must be kept.
 *
 * If the #TpChannel:channel-properties property was not set during
 * construction (e.g. by calling tp_channel_new_from_properties()), a
 * reasonable but possibly incomplete version will be made up from the values
 * of individual properties; reading this property repeatedly may yield
 * progressively more complete values until the %TP_CHANNEL_FEATURE_CORE
 * feature is prepared.
 *
 * Returns: (transfer none) (element-type utf8 GObject.Value): a #GHashTable
 *  where the keys are strings,
 *  D-Bus interface name + "." + property name, and the values are #GValue
 *  instances
 */
GHashTable *
tp_channel_borrow_immutable_properties (TpChannel *self)
{
  g_return_val_if_fail (TP_IS_CHANNEL (self), NULL);

  return self->priv->channel_properties;
}


static void
tp_channel_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->connection);
      break;
    case PROP_CHANNEL_READY:
      g_value_set_boolean (value, self->priv->ready);
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value,
          g_quark_to_string (self->priv->channel_type));
      break;
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, self->priv->handle_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, self->priv->handle);
      break;
    case PROP_IDENTIFIER:
      g_value_set_string (value, tp_channel_get_identifier (self));
      break;
    case PROP_CHANNEL_PROPERTIES:
      g_value_set_boxed (value, self->priv->channel_properties);
      break;
    case PROP_GROUP_SELF_HANDLE:
      g_value_set_uint (value, self->priv->group_self_handle);
      break;
    case PROP_GROUP_FLAGS:
      g_value_set_uint (value, self->priv->group_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/**
 * tp_channel_get_chat_state:
 * @self: a channel
 * @contact: a contact handle
 *
 * Return the chat state for the given contact. If tp_proxy_is_prepared()
 * would return %FALSE for the feature %TP_CHANNEL_FEATURE_CHAT_STATES,
 * the result will always be %TP_CHANNEL_CHAT_STATE_INACTIVE.
 *
 * Returns: the chat state for @contact, or %TP_CHANNEL_CHAT_STATE_INACTIVE
 *  if their chat state is not known
 * Since: 0.11.3
 */
TpChannelChatState
tp_channel_get_chat_state (TpChannel *self,
    TpHandle contact)
{
  gpointer value;

  g_return_val_if_fail (TP_IS_CHANNEL (self), 0);

  if (self->priv->chat_states != NULL &&
      g_hash_table_lookup_extended (self->priv->chat_states,
        GUINT_TO_POINTER (contact), NULL, &value))
    {
      return GPOINTER_TO_UINT (value);
    }

  return TP_CHANNEL_CHAT_STATE_INACTIVE;
}


/* These functions, maybe_set_whatever, ignore attempts to set a null value.
 * This means we can indiscriminately set everything from every source
 * (channel-properties, other construct-time properties, GetAll fast path,
 * 0.16.x slow path), and if only one of the sources supplied a value, it'll
 * still all be fine. */


static void
_tp_channel_maybe_set_channel_type (TpChannel *self,
                                    const gchar *type)
{
  GQuark q = g_quark_from_string (type);

  if (type == NULL)
    return;

  self->priv->channel_type = q;
  g_hash_table_insert (self->priv->channel_properties,
      g_strdup (TP_PROP_CHANNEL_CHANNEL_TYPE),
      tp_g_value_slice_new_static_string (g_quark_to_string (q)));
}


static void
_tp_channel_maybe_set_handle (TpChannel *self,
                              TpHandle handle,
                              gboolean valid)
{
  if (valid)
    {
      self->priv->handle = handle;
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_HANDLE),
          tp_g_value_slice_new_uint (handle));
    }
}


static void
_tp_channel_maybe_set_handle_type (TpChannel *self,
                                   TpHandleType handle_type,
                                   gboolean valid)
{
  if (valid)
    {
      self->priv->handle_type = handle_type;
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_HANDLE_TYPE),
          tp_g_value_slice_new_uint (handle_type));
    }
}


static void
_tp_channel_maybe_set_identifier (TpChannel *self,
                                  const gchar *identifier)
{
  if (identifier != NULL && self->priv->identifier == NULL)
    {
      self->priv->identifier = g_strdup (identifier);
      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_ID),
          tp_g_value_slice_new_string (identifier));
    }
}

static void
_tp_channel_maybe_set_interfaces (TpChannel *self,
                                  const gchar **interfaces)
{
  const gchar **iter;

  if (interfaces == NULL)
    return;

  for (iter = interfaces; *iter != NULL; iter++)
    {
      DEBUG ("- %s", *iter);

      if (tp_dbus_check_valid_interface_name (*iter, NULL))
        {
          GQuark q = g_quark_from_string (*iter);
          tp_proxy_add_interface_by_id ((TpProxy *) self, q);
        }
      else
        {
          DEBUG ("\tInterface %s not valid, ignoring it", *iter);
        }
    }

  g_hash_table_insert (self->priv->channel_properties,
      g_strdup (TP_PROP_CHANNEL_INTERFACES),
      tp_g_value_slice_new_boxed (G_TYPE_STRV, interfaces));
}


static void
tp_channel_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  TpChannel *self = TP_CHANNEL (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      self->priv->connection = TP_CONNECTION (g_value_dup_object (value));
      break;

    case PROP_CHANNEL_TYPE:
      _tp_channel_maybe_set_channel_type (self, g_value_get_string (value));
      break;

    case PROP_HANDLE_TYPE:
      _tp_channel_maybe_set_handle_type (self, g_value_get_uint (value),
          (g_value_get_uint (value) != TP_UNKNOWN_HANDLE_TYPE));
      break;

    case PROP_HANDLE:
      _tp_channel_maybe_set_handle (self, g_value_get_uint (value),
          (g_value_get_uint (value) != 0));
      break;

    case PROP_CHANNEL_PROPERTIES:
        {
          GHashTable *asv = g_value_get_boxed (value);
          gboolean valid;

          /* default value at construct time is NULL, we need to ignore that */
          if (asv != NULL)
            {
              guint u;

              /* no need to emit GObject::notify for any of these since this
               * can only happen at construct time, before anyone has
               * connected to it */

              tp_g_hash_table_update (self->priv->channel_properties,
                  asv, (GBoxedCopyFunc) g_strdup,
                  (GBoxedCopyFunc) tp_g_value_slice_dup);

              u = tp_asv_get_uint32 (self->priv->channel_properties,
                  TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, &valid);
              _tp_channel_maybe_set_handle_type (self, u, valid);

              u = tp_asv_get_uint32 (self->priv->channel_properties,
                  TP_PROP_CHANNEL_TARGET_HANDLE, &valid);
              _tp_channel_maybe_set_handle (self, u, valid);

              _tp_channel_maybe_set_identifier (self,
                  tp_asv_get_string (self->priv->channel_properties,
                      TP_PROP_CHANNEL_TARGET_ID));

              _tp_channel_maybe_set_channel_type (self,
                  tp_asv_get_string (self->priv->channel_properties,
                      TP_PROP_CHANNEL_CHANNEL_TYPE));

              _tp_channel_maybe_set_interfaces (self,
                  tp_asv_get_boxed (self->priv->channel_properties,
                      TP_PROP_CHANNEL_INTERFACES,
                      G_TYPE_STRV));
            }
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


/* Introspection etc. */


void
_tp_channel_abort_introspection (TpChannel *self,
                                 const gchar *debug,
                                 const GError *error)
{
  DEBUG ("%p: Introspection failed: %s: %s", self, debug, error->message);

  g_assert (self->priv->introspect_needed != NULL);
  g_queue_free (self->priv->introspect_needed);
  self->priv->introspect_needed = NULL;
  tp_proxy_invalidate ((TpProxy *) self, error);
}

static void
tp_channel_chat_state_changed_cb (TpChannel *self,
    guint contact,
    guint state,
    gpointer unused G_GNUC_UNUSED,
    GObject *object G_GNUC_UNUSED)
{
  g_hash_table_insert (self->priv->chat_states,
      GUINT_TO_POINTER (contact), GUINT_TO_POINTER (state));

  /* Don't emit the signal until we've had the initial state */
  if (!tp_proxy_is_prepared (self, TP_CHANNEL_FEATURE_CHAT_STATES))
    return;

  g_signal_emit (self, signals[SIGNAL_CHAT_STATE_CHANGED], 0, contact, state);
}

static void
tp_channel_get_initial_chat_states_cb (TpProxy *proxy,
    const GValue *value,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (error == NULL && G_VALUE_HOLDS (value, TP_HASH_TYPE_CHAT_STATE_MAP))
    {
      tp_g_hash_table_update (self->priv->chat_states,
          g_value_get_boxed (value), NULL, NULL);
    }
  /* else just ignore it and assume everyone was initially in the default
   * Inactive state, unless we already saw a signal for them */

  _tp_proxy_set_feature_prepared (proxy, TP_CHANNEL_FEATURE_CHAT_STATES, TRUE);
}

static void
tp_channel_maybe_prepare_chat_states (TpProxy *proxy)
{
  TpChannel *self = (TpChannel *) proxy;

  if (self->priv->chat_states != NULL)
    return;   /* already done */

  if (!_tp_proxy_is_preparing (proxy, TP_CHANNEL_FEATURE_CHAT_STATES))
    return;   /* not interested right now */

  if (!self->priv->ready)
    return;   /* will try again when ready */

  if (!tp_proxy_has_interface_by_id (proxy,
        TP_IFACE_QUARK_CHANNEL_INTERFACE_CHAT_STATE))
    {
      /* not going to happen */
      _tp_proxy_set_feature_prepared (proxy, TP_CHANNEL_FEATURE_CHAT_STATES,
          FALSE);
      return;
    }

  /* chat states? yes please! */
  self->priv->chat_states = g_hash_table_new (NULL, NULL);
  tp_cli_channel_interface_chat_state_connect_to_chat_state_changed (
      self, tp_channel_chat_state_changed_cb, NULL, NULL, NULL,
      NULL);

  tp_cli_dbus_properties_call_get (self, -1,
      TP_IFACE_CHANNEL_INTERFACE_CHAT_STATE, "ChatStates",
      tp_channel_get_initial_chat_states_cb,
      NULL, NULL, NULL);
}

void
_tp_channel_continue_introspection (TpChannel *self)
{
  DEBUG ("%p", self);

  g_assert (self->priv->introspect_needed != NULL);

  if (tp_proxy_get_invalidated (self))
    {
      DEBUG ("invalidated; giving up");

      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;
    }
  else if (g_queue_peek_head (self->priv->introspect_needed) == NULL)
    {
      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;

      DEBUG ("%p: channel ready", self);
      self->priv->ready = TRUE;
      g_object_notify ((GObject *) self, "channel-ready");

      /* for now, we only have one introspection queue, so CORE and
       * (if supported) GROUP turn up simultaneously */
      _tp_proxy_set_feature_prepared ((TpProxy *) self,
          TP_CHANNEL_FEATURE_CORE, TRUE);
      _tp_proxy_set_feature_prepared ((TpProxy *) self,
          TP_CHANNEL_FEATURE_GROUP,
          tp_proxy_has_interface_by_id (self,
            TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP));

      tp_channel_maybe_prepare_chat_states ((TpProxy *) self);
    }
  else
    {
      TpChannelProc next = g_queue_pop_head (self->priv->introspect_needed);

      next (self);
    }
}


static void
tp_channel_got_interfaces_cb (TpChannel *self,
                              const gchar **interfaces,
                              const GError *error,
                              gpointer unused,
                              GObject *unused2)
{
  if (error != NULL)
    {
      _tp_channel_abort_introspection (self, "GetInterfaces() failed", error);
      return;
    }

  self->priv->exists = TRUE;
  _tp_channel_maybe_set_interfaces (self, interfaces);

  /* FIXME: give subclasses a chance to influence the definition of "ready"
   * now that we have our interfaces? */

  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_interfaces (TpChannel *self)
{
  DEBUG ("%p", self);

  if (tp_asv_lookup (self->priv->channel_properties,
          TP_PROP_CHANNEL_INTERFACES) != NULL &&
      (self->priv->exists ||
       tp_proxy_has_interface_by_id (self,
          TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP)))
    {
      /* If we already know the channel's interfaces, and either have already
       * successfully called a method on the channel (so know it's alive) or
       * are going to call one on it when we introspect the Group properties,
       * then we don't need to do anything here.
       */
      _tp_channel_continue_introspection (self);
    }
  else
    {
      /* either we don't know the Interfaces, or we just want to verify the
       * channel's existence */
      tp_cli_channel_call_get_interfaces (self, -1,
          tp_channel_got_interfaces_cb, NULL, NULL, NULL);
    }
}


static void
tp_channel_got_channel_type_cb (TpChannel *self,
                                const gchar *channel_type,
                                const GError *error,
                                gpointer unused,
                                GObject *unused2)
{
  GError *err2 = NULL;

  if (error != NULL)
    {
      _tp_channel_abort_introspection (self, "GetChannelType failed", error);
    }
  else if (tp_dbus_check_valid_interface_name (channel_type, &err2))
    {
      self->priv->exists = TRUE;
      DEBUG ("%p: Introspected channel type %s", self, channel_type);
      _tp_channel_maybe_set_channel_type (self, channel_type);
      g_object_notify ((GObject *) self, "channel-type");

      tp_proxy_add_interface_by_id ((TpProxy *) self,
          self->priv->channel_type);

      _tp_channel_continue_introspection (self);
    }
  else
    {
      _tp_channel_abort_introspection (self,
          "GetChannelType returned invalid type", err2);
      g_error_free (err2);
    }
}


static void
_tp_channel_get_channel_type (TpChannel *self)
{
  if (self->priv->channel_type == 0)
    {
      DEBUG ("%p: calling GetChannelType", self);
      tp_cli_channel_call_get_channel_type (self, -1,
          tp_channel_got_channel_type_cb, NULL, NULL, NULL);
    }
  else
    {
      DEBUG ("%p: channel type %s already determined", self,
          g_quark_to_string (self->priv->channel_type));
      tp_proxy_add_interface_by_id ((TpProxy *) self,
          self->priv->channel_type);
      _tp_channel_continue_introspection (self);
    }
}


static void
tp_channel_got_handle_cb (TpChannel *self,
                          guint handle_type,
                          guint handle,
                          const GError *error,
                          gpointer unused,
                          GObject *unused2)
{
  if (error == NULL)
    {
      self->priv->exists = TRUE;

      DEBUG ("%p: Introspected handle #%d of type %d", self, handle,
          handle_type);
      self->priv->handle_type = handle_type;
      self->priv->handle = handle;

      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_HANDLE_TYPE),
          tp_g_value_slice_new_uint (handle_type));

      g_hash_table_insert (self->priv->channel_properties,
          g_strdup (TP_PROP_CHANNEL_TARGET_HANDLE),
          tp_g_value_slice_new_uint (handle));

      g_object_notify ((GObject *) self, "handle-type");
      g_object_notify ((GObject *) self, "handle");

      _tp_channel_continue_introspection (self);
    }
  else
    {
      _tp_channel_abort_introspection (self, "GetHandle failed", error);
    }
}


static void
_tp_channel_get_handle (TpChannel *self)
{
  if (self->priv->handle_type == TP_UNKNOWN_HANDLE_TYPE
      || (self->priv->handle == 0 &&
          self->priv->handle_type != TP_HANDLE_TYPE_NONE))
    {
      DEBUG ("%p: calling GetHandle", self);
      tp_cli_channel_call_get_handle (self, -1,
          tp_channel_got_handle_cb, NULL, NULL, NULL);
    }
  else
    {
      DEBUG ("%p: handle already known to be %u of type %u", self,
          self->priv->handle, self->priv->handle_type);
      _tp_channel_continue_introspection (self);
    }
}



static void
tp_channel_got_identifier_cb (TpConnection *connection,
                              const gchar **identifier,
                              const GError *error,
                              gpointer user_data,
                              GObject *unused2)
{
  TpChannel *self = user_data;

  if (error != NULL)
    {
      _tp_channel_abort_introspection (self, "InspectHandles failed", error);
      goto finally;
    }

  if (identifier == NULL || identifier[0] == NULL || identifier[1] != NULL)
    {
      GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_INCONSISTENT,
          "CM is broken: InspectHandles(CONTACT, [TargetHandle]) returned "
          "non-1 length" };

      _tp_channel_abort_introspection (self, "InspectHandles inconsistent",
          &e);
      goto finally;
    }

  DEBUG ("%p: Introspected identifier %s", self, identifier[0]);
  _tp_channel_maybe_set_identifier (self, identifier[0]);
  g_object_notify ((GObject *) self, "identifier");

  _tp_channel_continue_introspection (self);

finally:
  g_object_unref (self);
}


static void
_tp_channel_get_identifier (TpChannel *self)
{
  if (self->priv->identifier == NULL &&
      (self->priv->handle == 0 ||
       self->priv->handle_type == TP_HANDLE_TYPE_NONE))
    {
      /* no need to emit GObject::notify here since the initial value was "" */
      _tp_channel_maybe_set_identifier (self, "");
    }

  if (self->priv->identifier == NULL)
    {
      GArray handles = {(gchar *) &self->priv->handle, 1};

      DEBUG ("%p: calling InspectHandles", self);
      tp_cli_connection_call_inspect_handles (self->priv->connection, -1,
          self->priv->handle_type, &handles,
          tp_channel_got_identifier_cb, g_object_ref (self), NULL, NULL);
    }
  else
    {
      DEBUG ("%p: identifier already known to be %s", self,
          self->priv->identifier);
      _tp_channel_continue_introspection (self);
    }
}


static void
_tp_channel_got_properties (TpProxy *proxy,
                            GHashTable *asv,
                            const GError *error,
                            gpointer unused G_GNUC_UNUSED,
                            GObject *object G_GNUC_UNUSED)
{
  TpChannel *self = TP_CHANNEL (proxy);

  if (error == NULL)
    {
      gboolean valid;
      guint u;
      const gchar *s;
      gboolean b;

      DEBUG ("Received %u channel properties", g_hash_table_size (asv));

      self->priv->exists = TRUE;

      _tp_channel_maybe_set_channel_type (self,
          tp_asv_get_string (asv, "ChannelType"));
      _tp_channel_maybe_set_interfaces (self,
          tp_asv_get_boxed (asv, "Interfaces", G_TYPE_STRV));

      u = tp_asv_get_uint32 (asv, "TargetHandleType", &valid);
      _tp_channel_maybe_set_handle_type (self, u, valid);

      u = tp_asv_get_uint32 (asv, "TargetHandle", &valid);
      _tp_channel_maybe_set_handle (self, u, valid);

      _tp_channel_maybe_set_identifier (self,
          tp_asv_get_string (asv, "TargetID"));

      u = tp_asv_get_uint32 (asv, "InitiatorHandle", &valid);

      if (valid)
        {
          g_hash_table_insert (self->priv->channel_properties,
              g_strdup (TP_PROP_CHANNEL_INITIATOR_HANDLE),
              tp_g_value_slice_new_uint (u));
        }

      s = tp_asv_get_string (asv, "InitiatorID");

      if (s != NULL)
        {
          g_hash_table_insert (self->priv->channel_properties,
              g_strdup (TP_PROP_CHANNEL_INITIATOR_ID),
              tp_g_value_slice_new_string (s));
        }

      b = tp_asv_get_boolean (asv, "Requested", &valid);

      if (valid)
        {
          g_hash_table_insert (self->priv->channel_properties,
              g_strdup (TP_PROP_CHANNEL_REQUESTED),
              tp_g_value_slice_new_boolean (b));
        }

      g_object_notify ((GObject *) self, "channel-type");
      g_object_notify ((GObject *) self, "interfaces");
      g_object_notify ((GObject *) self, "handle-type");
      g_object_notify ((GObject *) self, "handle");
      g_object_notify ((GObject *) self, "identifier");
    }
  else
    {
      /* GetAll failed; it's not mandatory, so continue with the separate
       * (spec 0.16.x-style) method calls */
      DEBUG ("GetAll failed, falling back to 0.16 API:"
          " %s", error->message);
    }

  /* Either way, we'll fill in any other gaps in the properties, then
   * continue with any other introspection */
  _tp_channel_continue_introspection (self);
}


static void
_tp_channel_get_properties (TpChannel *self)
{
  /* skip it if we already have all the details we want */
  if (self->priv->handle_type != TP_UNKNOWN_HANDLE_TYPE
      && (self->priv->handle != 0 ||
          self->priv->handle_type == TP_HANDLE_TYPE_NONE)
      && self->priv->channel_type != 0
      /* currently we always re-fetch the interfaces later, so don't check:
      && tp_asv_get_boxed (self->priv->channel_properties,
        TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV) != NULL
       */
      && tp_asv_get_string (self->priv->channel_properties,
        TP_PROP_CHANNEL_TARGET_ID) != NULL
      && tp_asv_get_string (self->priv->channel_properties,
        TP_PROP_CHANNEL_INITIATOR_ID) != NULL
      )
    {
      gboolean valid;

      tp_asv_get_uint32 (self->priv->channel_properties,
          TP_PROP_CHANNEL_INITIATOR_HANDLE, &valid);

      if (!valid)
        goto missing;

      tp_asv_get_boolean (self->priv->channel_properties,
          TP_PROP_CHANNEL_REQUESTED, &valid);

      if (!valid)
        goto missing;

      _tp_channel_continue_introspection (self);
      return;
    }

missing:
  tp_cli_dbus_properties_call_get_all (self, -1,
      TP_IFACE_CHANNEL, _tp_channel_got_properties, NULL, NULL, NULL);
}


static void
tp_channel_closed_cb (TpChannel *self,
                      gpointer user_data,
                      GObject *weak_object)
{

  if (self->priv->group_remove_error != NULL)
    {
      /* use the error provided by the Group code */
      tp_proxy_invalidate ((TpProxy *) self, self->priv->group_remove_error);
      g_clear_error (&self->priv->group_remove_error);
    }
  else
    {
      GError e = { TP_DBUS_ERRORS, TP_DBUS_ERROR_OBJECT_REMOVED,
          "Channel was closed" };

      tp_proxy_invalidate ((TpProxy *) self, &e);
    }
}

static void
tp_channel_connection_invalidated_cb (TpConnection *conn,
                                      guint domain,
                                      guint code,
                                      gchar *message,
                                      TpChannel *self)
{
  const GError e = { domain, code, message };

  g_signal_handler_disconnect (conn, self->priv->conn_invalidated_id);
  self->priv->conn_invalidated_id = 0;

  /* tp_proxy_invalidate and g_object_notify call out to user code - add a
   * temporary ref to ensure that we don't become finalized while doing so */
  g_object_ref (self);

  tp_proxy_invalidate ((TpProxy *) self, &e);

  /* this channel's handle is now meaningless */
  if (self->priv->handle != 0)
    {
      self->priv->handle = 0;
      g_object_notify ((GObject *) self, "handle");
    }

  g_object_unref (self);
}

static GObject *
tp_channel_constructor (GType type,
                        guint n_params,
                        GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_channel_parent_class;
  TpChannel *self = TP_CHANNEL (object_class->constructor (type,
        n_params, params));
  GError *error = NULL;
  TpProxySignalConnection *sc;

  /* If our TpConnection dies, so do we. */
  self->priv->conn_invalidated_id = g_signal_connect (self->priv->connection,
      "invalidated", G_CALLBACK (tp_channel_connection_invalidated_cb),
      self);

  /* Connect to my own Closed signal and self-destruct when it arrives.
   * The channel hasn't had a chance to become invalid yet (it was just
   * constructed!), so we assert that this signal connection will work */
  sc = tp_cli_channel_connect_to_closed (self, tp_channel_closed_cb, NULL, NULL,
      NULL, &error);

  if (sc == NULL)
    {
      CRITICAL ("Couldn't connect to Closed: %s", error->message);
      g_assert_not_reached ();
      g_error_free (error);
      return NULL;
    }

  DEBUG ("%p: constructed with channel type \"%s\", handle #%d of type %d",
      self,
      (self->priv->channel_type != 0)
          ? g_quark_to_string (self->priv->channel_type)
          : "(null)",
      self->priv->handle, self->priv->handle_type);

  self->priv->introspect_needed = g_queue_new ();

  /* this does nothing if we already know all the Channel properties this
   * code is aware of */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_properties);

  /* this does nothing if we already know the handle */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_handle);

  /* this does nothing if we already know the identifier */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_identifier);

  /* this does nothing if we already know the channel type */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_channel_type);

  /* This makes a call unless (a) we already know the Interfaces by now, and
   * (b) priv->exists is TRUE (i.e. either GetAll, GetHandle or GetChannelType
   * has succeeded).
   *
   * This means the channel never becomes ready until we re-enter the
   * main loop, and we always verify that the channel does actually exist. */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_interfaces);

  /* this needs doing *after* GetInterfaces so we know whether we're a group */
  g_queue_push_tail (self->priv->introspect_needed,
      _tp_channel_get_group_properties);

  _tp_channel_continue_introspection (self);

  return (GObject *) self;
}

static void
tp_channel_init (TpChannel *self)
{
  DEBUG ("%p", self);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CHANNEL,
      TpChannelPrivate);
  self->priv->channel_type = 0;
  self->priv->handle_type = TP_UNKNOWN_HANDLE_TYPE;
  self->priv->handle = 0;
  self->priv->channel_properties = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free);
}

static void
tp_channel_dispose (GObject *object)
{
  TpChannel *self = (TpChannel *) object;

  DEBUG ("%p", self);

  if (self->priv->connection == NULL)
    goto finally;

  if (self->priv->conn_invalidated_id != 0)
    g_signal_handler_disconnect (self->priv->connection,
        self->priv->conn_invalidated_id);

  self->priv->conn_invalidated_id = 0;

  tp_clear_object (&self->priv->connection);

finally:
  ((GObjectClass *) tp_channel_parent_class)->dispose (object);
}

static void
tp_channel_finalize (GObject *object)
{
  TpChannel *self = (TpChannel *) object;

  DEBUG ("%p", self);

  if (self->priv->group_remove_error != NULL)
    g_clear_error (&self->priv->group_remove_error);

  if (self->priv->group_local_pending_info != NULL)
    {
      g_hash_table_destroy (self->priv->group_local_pending_info);
      self->priv->group_local_pending_info = NULL;
    }

  if (self->priv->group_members != NULL)
    {
      tp_intset_destroy (self->priv->group_members);
      self->priv->group_members = NULL;
    }

  if (self->priv->group_local_pending != NULL)
    {
      tp_intset_destroy (self->priv->group_local_pending);
      self->priv->group_local_pending = NULL;
    }

  if (self->priv->group_remote_pending != NULL)
    {
      tp_intset_destroy (self->priv->group_remote_pending);
      self->priv->group_remote_pending = NULL;
    }

  if (self->priv->group_handle_owners != NULL)
    {
      g_hash_table_destroy (self->priv->group_handle_owners);
      self->priv->group_handle_owners = NULL;
    }

  if (self->priv->introspect_needed != NULL)
    {
      g_queue_free (self->priv->introspect_needed);
      self->priv->introspect_needed = NULL;
    }

  if (self->priv->chat_states != NULL)
    {
      g_hash_table_destroy (self->priv->chat_states);
      self->priv->chat_states = NULL;
    }

  g_assert (self->priv->channel_properties != NULL);
  g_hash_table_destroy (self->priv->channel_properties);

  g_free (self->priv->identifier);

  ((GObjectClass *) tp_channel_parent_class)->finalize (object);
}

enum {
    FEAT_CORE,
    FEAT_GROUP,
    FEAT_CHAT_STATES,
    N_FEAT
};

static const TpProxyFeature *
tp_channel_list_features (TpProxyClass *cls G_GNUC_UNUSED)
{
  static TpProxyFeature features[N_FEAT + 1] = { { 0 } };

  if (G_LIKELY (features[0].name != 0))
    return features;

  features[FEAT_CORE].name = TP_CHANNEL_FEATURE_CORE;
  features[FEAT_CORE].core = TRUE;

  features[FEAT_GROUP].name = TP_CHANNEL_FEATURE_GROUP;

  features[FEAT_CHAT_STATES].name = TP_CHANNEL_FEATURE_CHAT_STATES;
  features[FEAT_CHAT_STATES].start_preparing =
    tp_channel_maybe_prepare_chat_states;

  /* assert that the terminator at the end is there */
  g_assert (features[N_FEAT].name == 0);

  return features;
}

static void
tp_channel_class_init (TpChannelClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GType au_type = dbus_g_type_get_collection ("GArray", G_TYPE_UINT);

  tp_channel_init_known_interfaces ();

  g_type_class_add_private (klass, sizeof (TpChannelPrivate));

  object_class->constructor = tp_channel_constructor;
  object_class->get_property = tp_channel_get_property;
  object_class->set_property = tp_channel_set_property;
  object_class->dispose = tp_channel_dispose;
  object_class->finalize = tp_channel_finalize;

  proxy_class->interface = TP_IFACE_QUARK_CHANNEL;
  proxy_class->must_have_unique_name = TRUE;
  proxy_class->list_features = tp_channel_list_features;

  /**
   * TpChannel:channel-type:
   *
   * The D-Bus interface representing the type of this channel.
   *
   * Read-only except during construction. If %NULL during construction
   * (default), we ask the remote D-Bus object what its channel type is;
   * reading this property will yield %NULL until we get the reply, or if
   * GetChannelType() fails.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE.
   */
  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");

  /**
   * TpChannel:handle-type:
   *
   * The #TpHandleType of this channel's associated handle, or 0 if no
   * handle, or TP_UNKNOWN_HANDLE_TYPE if unknown.
   *
   * Read-only except during construction. If this is TP_UNKNOWN_HANDLE_TYPE
   * during construction (default), we ask the remote D-Bus object what its
   * handle type is; reading this property will yield TP_UNKNOWN_HANDLE_TYPE
   * until we get the reply.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE.
   */
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");

  /**
   * TpChannel:handle:
   *
   * This channel's associated handle, or 0 if no handle or unknown.
   *
   * Read-only except during construction. If this is 0
   * during construction, and handle-type is not TP_HANDLE_TYPE_NONE (== 0),
   * we ask the remote D-Bus object what its handle type is; reading this
   * property will yield 0 until we get the reply, or if GetHandle()
   * fails.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE.
   */
  g_object_class_override_property (object_class, PROP_HANDLE,
      "handle");

  /**
   * TpChannel:identifier:
   *
   * This channel's associated identifier, or the empty string if it has
   * handle type %TP_HANDLE_TYPE_NONE.
   *
   * For channels where #TpChannel:handle is non-zero, this is the result of
   * inspecting #TpChannel:handle.
   *
   * This is not guaranteed to be set until tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE; until then, it may be
   * the empty string.
   *
   * Changed in 0.11.4: this property is never %NULL. Previously,
   * it was %NULL before an identifier was known, or when a channel
   * with no TargetID D-Bus property had TargetHandleType %TP_HANDLE_TYPE_NONE.
   */
  param_spec = g_param_spec_string ("identifier",
      "The identifier",
      "The identifier of the channel",
      "",
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_IDENTIFIER,
      param_spec);

  /**
   * TpChannel:channel-properties:
   *
   * The immutable D-Bus properties of this channel, represented by a
   * #GHashTable where the keys are D-Bus interface name + "." + property
   * name, and the values are #GValue instances.
   *
   * Read-only except during construction. If this is not provided
   * during construction, a reasonable (but possibly incomplete) version
   * will be made up from the values of individual properties; reading this
   * property repeatedly may yield progressively more complete values until
   * tp_proxy_prepare_async() has finished preparing %TP_CHANNEL_FEATURE_CORE.
   */
  param_spec = g_param_spec_boxed ("channel-properties",
      "Immutable D-Bus properties",
      "A map D-Bus interface + \".\" + property name => GValue",
      TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_PROPERTIES,
      param_spec);

  /**
   * TpChannel:channel-ready:
   *
   * Initially %FALSE; changes to %TRUE when tp_proxy_prepare_async() has
   * finished preparing %TP_CHANNEL_FEATURE_CORE, and if the channel is a
   * group, %TP_CHANNEL_FEATURE_GROUP.
   *
   * This is a less general form of tp_proxy_is_prepared(), which should be
   * used in new code.
   *
   * One important difference is that after #TpChannel::invalidated is
   * signalled, #TpChannel:channel-ready keeps its current value - which might
   * be %TRUE, if the channel was successfully prepared before it became
   * invalidated - but tp_proxy_is_prepared() returns %FALSE for all features.
   *
   * Change notification is via notify::channel-ready.
   */
  param_spec = g_param_spec_boolean ("channel-ready", "Channel ready?",
      "Initially FALSE; changes to TRUE when introspection finishes", FALSE,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_CHANNEL_READY,
      param_spec);

  /**
   * TpChannel:connection:
   *
   * The #TpConnection to which this #TpChannel belongs. Used for e.g.
   * handle manipulation.
   */
  param_spec = g_param_spec_object ("connection", "TpConnection",
      "The connection to which this object belongs.", TP_TYPE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);

  /**
   * TpChannel:group-self-handle:
   *
   * If this channel is a group and %TP_CHANNEL_FEATURE_GROUP has been
   * prepared, and the user is a member of the group, the #TpHandle
   * representing them in this group.
   *
   * Otherwise, the result may be either a handle representing the user, or 0.
   *
   * Change notification is via notify::group-self-handle.
   *
   * Since: 0.7.12
   */
  param_spec = g_param_spec_uint ("group-self-handle", "Group.SelfHandle",
      "Undefined if not a group", 0, G_MAXUINT32, 0,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_GROUP_SELF_HANDLE,
      param_spec);

  /**
   * TpChannel:group-flags:
   *
   * If the %TP_CHANNEL_FEATURE_GROUP feature has been prepared successfully,
   * #TpChannelGroupFlags indicating the capabilities and behaviour of that
   * group.
   *
   * Otherwise, this may be 0.
   *
   * Change notification is via notify::group-flags or
   * TpChannel::group-flags-changed.
   *
   * Since: 0.7.12
   */
  param_spec = g_param_spec_uint ("group-flags", "Group.GroupFlags",
      "0 if not a group", 0, G_MAXUINT32, 0,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_GROUP_FLAGS,
      param_spec);

  /**
   * TpChannel::group-flags-changed:
   * @self: a channel
   * @added: #TpChannelGroupFlags which are newly set
   * @removed: #TpChannelGroupFlags which are no longer set
   *
   * Emitted when the #TpChannel:group-flags property changes while the
   * channel is ready.
   *
   * Since: 0.7.12
   */
  signals[SIGNAL_GROUP_FLAGS_CHANGED] = g_signal_new ("group-flags-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__UINT_UINT,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  /**
   * TpChannel::group-members-changed: (skip)
   * @self: a channel
   * @message: an optional textual message
   * @added: a #GArray of #guint containing the full members added
   * @removed: a #GArray of #guint containing the members (full,
   *  local-pending or remote-pending) removed
   * @local_pending: a #GArray of #guint containing the local-pending
   *  members added
   * @remote_pending: a #GArray of #guint containing the remote-pending
   *  members added
   * @actor: the #TpHandle of the contact causing the change, or 0
   * @reason: the reason for the change as a #TpChannelGroupChangeReason
   *
   * Emitted when the group members change in a Group channel that is ready.
   *
   * Since: 0.7.12
   */
  signals[SIGNAL_GROUP_MEMBERS_CHANGED] = g_signal_new (
      "group-members-changed", G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__STRING_BOXED_BOXED_BOXED_BOXED_UINT_UINT,
      G_TYPE_NONE, 7,
      G_TYPE_STRING, au_type, au_type, au_type, au_type, G_TYPE_UINT,
      G_TYPE_UINT);

  /**
   * TpChannel::group-members-changed-detailed:
   * @self: a channel
   * @added: (type GLib.Array) (element-type uint): a #GArray of #guint
   *  containing the full members added
   * @removed: (type GLib.Array) (element-type uint):  a #GArray of #guint
   *  containing the members (full, local-pending or remote-pending) removed
   * @local_pending: (type GLib.Array) (element-type uint):  a #GArray of
   *  #guint containing the local-pending members added
   * @remote_pending: (type GLib.Array) (element-type uint):  a #GArray of
   *  #guint containing the remote-pending members added
   * @details: (type GLib.HashTable): (element-type utf8 GObject.Value):
   *  a #GHashTable mapping (gchar *) to #GValue containing details
   *  about the change, as described in the specification of the
   *  MembersChangedDetailed signal.
   *
   * Emitted when the group members change in a Group channel that is ready.
   * Contains a superset of the information in the
   * TpChannel::group-members-changed signal, and is emitted at the same time;
   * applications can connect to this signal and ignore the other.
   *
   * Since: 0.7.21
   */
  signals[SIGNAL_GROUP_MEMBERS_CHANGED_DETAILED] = g_signal_new (
      "group-members-changed-detailed", G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__BOXED_BOXED_BOXED_BOXED_BOXED,
      G_TYPE_NONE, 5,
      au_type, au_type, au_type, au_type, TP_HASH_TYPE_STRING_VARIANT_MAP);

  /**
   * TpChannel::chat-state-changed:
   * @self: a channel
   * @contact: a contact handle for the local user or another contact
   * @state: the new #TpChannelChatState for the contact
   *
   * Emitted when a contact's chat state changes after tp_proxy_prepare_async()
   * has finished preparing the feature %TP_CHANNEL_FEATURE_CHAT_STATES.
   *
   * Since: 0.11.3
   */
  signals[SIGNAL_CHAT_STATE_CHANGED] = g_signal_new ("chat-state-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _tp_marshal_VOID__UINT_UINT,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}


/**
 * tp_channel_new_from_properties:
 * @conn: a connection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @immutable_properties: (transfer none) (element-type utf8 GObject.Value):
 *  the immutable properties of the channel,
 *  as signalled by the NewChannel D-Bus signal or returned by the
 *  CreateChannel and EnsureChannel D-Bus methods: a mapping from
 *  strings (D-Bus interface name + "." + property name) to #GValue instances
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new channel proxy, or %NULL on invalid arguments
 *
 * Since: 0.7.19
 */
TpChannel *
tp_channel_new_from_properties (TpConnection *conn,
                                const gchar *object_path,
                                const GHashTable *immutable_properties,
                                GError **error)
{
  TpProxy *conn_proxy = (TpProxy *) conn;
  TpChannel *ret = NULL;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);
  g_return_val_if_fail (immutable_properties != NULL, NULL);

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  /* An unfortunate collision between the default value in
   * TpChannelIface (0), and the default we want (-1), means that
   * we have to pass TP_UNKNOWN_HANDLE_TYPE to the constructor
   * explicitly, even if providing channel-properties. */

  ret = TP_CHANNEL (g_object_new (TP_TYPE_CHANNEL,
        "connection", conn,
        "dbus-daemon", conn_proxy->dbus_daemon,
        "bus-name", conn_proxy->bus_name,
        "object-path", object_path,
        "handle-type", (guint) TP_UNKNOWN_HANDLE_TYPE,
        "channel-properties", immutable_properties,
        NULL));

finally:
  return ret;
}

/**
 * tp_channel_new:
 * @conn: a connection; may not be %NULL
 * @object_path: the object path of the channel; may not be %NULL
 * @optional_channel_type: the channel type if already known, or %NULL if not
 * @optional_handle_type: the handle type if already known, or
 *  %TP_UNKNOWN_HANDLE_TYPE if not
 * @optional_handle: the handle if already known, or 0 if not
 *  (if @optional_handle_type is %TP_UNKNOWN_HANDLE_TYPE or
 *  %TP_HANDLE_TYPE_NONE, this must be 0)
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new channel proxy, or %NULL on invalid arguments.
 *
 * Since: 0.7.1
 */
TpChannel *
tp_channel_new (TpConnection *conn,
                const gchar *object_path,
                const gchar *optional_channel_type,
                TpHandleType optional_handle_type,
                TpHandle optional_handle,
                GError **error)
{
  TpChannel *ret = NULL;
  TpProxy *conn_proxy = (TpProxy *) conn;

  g_return_val_if_fail (TP_IS_CONNECTION (conn), NULL);
  g_return_val_if_fail (object_path != NULL, NULL);

  /* TpConnection always has a unique name, so we can assert this */
  g_assert (tp_dbus_check_valid_bus_name (conn_proxy->bus_name,
        TP_DBUS_NAME_TYPE_UNIQUE, NULL));

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  if (optional_channel_type != NULL &&
      !tp_dbus_check_valid_interface_name (optional_channel_type, error))
    goto finally;

  if (optional_handle_type == TP_UNKNOWN_HANDLE_TYPE ||
      optional_handle_type == TP_HANDLE_TYPE_NONE)
    {
      if (optional_handle != 0)
        {
          /* in the properties, we do actually allow the user to give us an
           * assumed-valid handle of unknown type - but that'd be silly */
          g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
              "Nonzero handle of type NONE or unknown makes no sense");
          goto finally;
        }
    }
  else if (!tp_handle_type_is_valid (optional_handle_type, error))
    {
      goto finally;
    }

  ret = TP_CHANNEL (g_object_new (TP_TYPE_CHANNEL,
        "connection", conn,
        "dbus-daemon", conn_proxy->dbus_daemon,
        "bus-name", conn_proxy->bus_name,
        "object-path", object_path,
        "channel-type", optional_channel_type,
        "handle-type", optional_handle_type,
        "handle", optional_handle,
        NULL));

finally:

  return ret;
}

/**
 * tp_channel_run_until_ready: (skip)
 * @self: a channel
 * @error: if not %NULL and %FALSE is returned, used to raise an error
 * @loop: if not %NULL, a #GMainLoop is placed here while it is being run
 *  (so calling code can call g_main_loop_quit() to abort), and %NULL is
 *  placed here after the loop has been run
 *
 * If @self is ready for use (introspection has finished, etc.), return
 * immediately. Otherwise, re-enter the main loop until the channel either
 * becomes invalid or becomes ready for use, or until the main loop stored
 * via @loop is cancelled.
 *
 * Returns: %TRUE if the channel has been introspected and is ready for use,
 *  %FALSE if the channel has become invalid.
 *
 * Since: 0.7.1
 * Deprecated: 0.11.0: Use tp_connection_call_when_ready,
 *  or restructure your program in such a way as to avoid re-entering the
 *  main loop.
 */
gboolean
tp_channel_run_until_ready (TpChannel *self,
                            GError **error,
                            GMainLoop **loop)
{
  TpProxy *as_proxy = (TpProxy *) self;
  GMainLoop *my_loop;
  gulong invalidated_id, ready_id;

  g_return_val_if_fail (TP_IS_CHANNEL (self), FALSE);

  if (as_proxy->invalidated)
    goto raise_invalidated;

  if (self->priv->ready)
    return TRUE;

  my_loop = g_main_loop_new (NULL, FALSE);
  invalidated_id = g_signal_connect_swapped (self, "invalidated",
      G_CALLBACK (g_main_loop_quit), my_loop);
  ready_id = g_signal_connect_swapped (self, "notify::channel-ready",
      G_CALLBACK (g_main_loop_quit), my_loop);

  if (loop != NULL)
    *loop = my_loop;

  g_main_loop_run (my_loop);

  if (loop != NULL)
    *loop = NULL;

  g_signal_handler_disconnect (self, invalidated_id);
  g_signal_handler_disconnect (self, ready_id);
  g_main_loop_unref (my_loop);

  if (as_proxy->invalidated)
    goto raise_invalidated;

  g_assert (self->priv->ready);
  return TRUE;

raise_invalidated:
  if (error != NULL)
    {
      g_return_val_if_fail (*error == NULL, FALSE);
      *error = g_error_copy (as_proxy->invalidated);
    }

  return FALSE;
}

typedef struct {
    TpChannelWhenReadyCb callback;
    gpointer user_data;
    gulong invalidated_id;
    gulong ready_id;
} CallWhenReadyContext;

static void
cwr_invalidated (TpChannel *self,
                 guint domain,
                 gint code,
                 gchar *message,
                 gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;
  GError e = { domain, code, message };

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, &e, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

static void
cwr_ready (TpChannel *self,
           GParamSpec *unused G_GNUC_UNUSED,
           gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, NULL, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

/**
 * TpChannelWhenReadyCb:
 * @channel: the channel (which may be in the middle of being disposed,
 *  if error is non-%NULL, error->domain is TP_DBUS_ERRORS and error->code is
 *  TP_DBUS_ERROR_PROXY_UNREFERENCED)
 * @error: %NULL if the channel is ready for use, or the error with which
 *  it was invalidated if it is now invalid
 * @user_data: whatever was passed to tp_channel_call_when_ready()
 *
 * Signature of a callback passed to tp_channel_call_when_ready(), which
 * will be called exactly once, when the channel becomes ready or
 * invalid (whichever happens first)
 */

/**
 * tp_channel_call_when_ready: (skip)
 * @self: a channel
 * @callback: called when the channel becomes ready or invalidated, whichever
 *  happens first
 * @user_data: arbitrary user-supplied data passed to the callback
 *
 * If @self is ready for use or has been invalidated, call @callback
 * immediately, then return. Otherwise, arrange
 * for @callback to be called when @self either becomes ready for use
 * or becomes invalid.
 *
 * This is a less general form of tp_proxy_prepare_async(), which should be
 * used in new code. (One important difference is that this function can call
 * @callback before it has returned, whereas tp_proxy_prepare_async() always
 * calls @callback from the main loop.)
 *
 * Since: 0.7.7
 */
void
tp_channel_call_when_ready (TpChannel *self,
                            TpChannelWhenReadyCb callback,
                            gpointer user_data)
{
  TpProxy *as_proxy = (TpProxy *) self;

  g_return_if_fail (TP_IS_CHANNEL (self));
  g_return_if_fail (callback != NULL);

  if (self->priv->ready || as_proxy->invalidated != NULL)
    {
      DEBUG ("already ready or invalidated");
      callback (self, as_proxy->invalidated, user_data);
    }
  else
    {
      CallWhenReadyContext *ctx = g_slice_new (CallWhenReadyContext);

      DEBUG ("arranging callback later");

      ctx->callback = callback;
      ctx->user_data = user_data;
      ctx->invalidated_id = g_signal_connect (self, "invalidated",
          G_CALLBACK (cwr_invalidated), ctx);
      ctx->ready_id = g_signal_connect (self, "notify::channel-ready",
          G_CALLBACK (cwr_ready), ctx);
    }
}

static gpointer
tp_channel_once (gpointer data G_GNUC_UNUSED)
{
  GType type = TP_TYPE_CHANNEL;

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (type,
      tp_cli_channel_add_signals);
  tp_proxy_subclass_add_error_mapping (type,
      TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

  return NULL;
}

/**
 * tp_channel_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpChannel have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CHANNEL.
 *
 * Since: 0.7.6
 */
void
tp_channel_init_known_interfaces (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, tp_channel_once, NULL);
}