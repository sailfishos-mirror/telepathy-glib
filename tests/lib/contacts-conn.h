/*
 * contacts-conn.h - header for a connection with contact info
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TP_TESTS_CONTACTS_CONN_H__
#define __TP_TESTS_CONTACTS_CONN_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

#include "simple-conn.h"
#include "contact-list-manager.h"

G_BEGIN_DECLS

typedef struct _TpTestsContactsConnection TpTestsContactsConnection;
typedef struct _TpTestsContactsConnectionClass TpTestsContactsConnectionClass;
typedef struct _TpTestsContactsConnectionPrivate TpTestsContactsConnectionPrivate;

struct _TpTestsContactsConnectionClass {
    TpTestsSimpleConnectionClass parent_class;

    TpPresenceMixinClass presence_mixin;
    TpDBusPropertiesMixinClass properties_class;
};

struct _TpTestsContactsConnection {
    TpTestsSimpleConnection parent;

    TpPresenceMixin presence_mixin;

    TpTestsContactsConnectionPrivate *priv;
};

GType tp_tests_contacts_connection_get_type (void);

/* Must match my_statuses in the .c */
typedef enum {
    TP_TESTS_CONTACTS_CONNECTION_STATUS_AVAILABLE,
    TP_TESTS_CONTACTS_CONNECTION_STATUS_BUSY,
    TP_TESTS_CONTACTS_CONNECTION_STATUS_AWAY,
    TP_TESTS_CONTACTS_CONNECTION_STATUS_OFFLINE,
    TP_TESTS_CONTACTS_CONNECTION_STATUS_UNKNOWN,
    TP_TESTS_CONTACTS_CONNECTION_STATUS_ERROR
} TpTestsContactsConnectionPresenceStatusIndex;

/* TYPE MACROS */
#define TP_TESTS_TYPE_CONTACTS_CONNECTION \
  (tp_tests_contacts_connection_get_type ())
#define TP_TESTS_CONTACTS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TESTS_TYPE_CONTACTS_CONNECTION, \
                              TpTestsContactsConnection))
#define TP_TESTS_CONTACTS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TESTS_TYPE_CONTACTS_CONNECTION, \
                           TpTestsContactsConnectionClass))
#define TP_TESTS_IS_CONTACTS_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TESTS_TYPE_CONTACTS_CONNECTION))
#define TP_TESTS_IS_CONTACTS_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TESTS_TYPE_CONTACTS_CONNECTION))
#define TP_TESTS_CONTACTS_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TESTS_TYPE_CONTACTS_CONNECTION, \
                              TpTestsContactsConnectionClass))

TpTestsContactListManager *tp_tests_contacts_connection_get_contact_list_manager (
    TpTestsContactsConnection *self);

void tp_tests_contacts_connection_change_aliases (
    TpTestsContactsConnection *self, guint n,
    const TpHandle *handles, const gchar * const *aliases);

void tp_tests_contacts_connection_change_presences (
    TpTestsContactsConnection *self, guint n, const TpHandle *handles,
    const TpTestsContactsConnectionPresenceStatusIndex *indexes,
    const gchar * const *messages);

void tp_tests_contacts_connection_change_avatar_tokens (
    TpTestsContactsConnection *self, guint n, const TpHandle *handles,
    const gchar * const *tokens);

void tp_tests_contacts_connection_change_avatar_data (
    TpTestsContactsConnection *self,
    TpHandle handle,
    GArray *data,
    const gchar *mime_type,
    const gchar *token);

void tp_tests_contacts_connection_change_locations (
    TpTestsContactsConnection *self,
    guint n,
    const TpHandle *handles,
    GHashTable **locations);

void tp_tests_contacts_connection_change_capabilities (
    TpTestsContactsConnection *self, GHashTable *capabilities);

void tp_tests_contacts_connection_change_contact_info (
    TpTestsContactsConnection *self, TpHandle handle, GPtrArray *info);

void tp_tests_contacts_connection_set_default_contact_info (
    TpTestsContactsConnection *self,
    GPtrArray *info);

G_END_DECLS

#endif /* ifndef __TP_TESTS_CONTACTS_CONN_H__ */
