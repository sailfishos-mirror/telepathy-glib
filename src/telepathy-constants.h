/*
 * telepathy-constants.h - constants used in telepathy
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

#ifndef __TELEPATHY_CONSTANTS_H__
#define __TELEPATHY_CONSTANTS_H__

#include <glib.h>

#include "telepathy-spec-enums.h"

G_BEGIN_DECLS

#define TP_CONN_ALIAS_FLAG_USER_SET TP_CONNECTION_ALIAS_FLAG_USER_SET

#define TP_CONN_CAPABILITY_FLAG_CREATE TP_CONNECTION_CAPABILITY_FLAG_CREATE
#define TP_CONN_CAPABILITY_FLAG_INVITE TP_CONNECTION_CAPABILITY_FLAG_INVITE

#define TP_CONN_PRESENCE_TYPE_UNSET TP_CONNECTION_PRESENCE_TYPE_UNSET
#define TP_CONN_PRESENCE_TYPE_OFFLINE TP_CONNECTION_PRESENCE_TYPE_OFFLINE
#define TP_CONN_PRESENCE_TYPE_AVAILABLE TP_CONNECTION_PRESENCE_TYPE_AVAILABLE
#define TP_CONN_PRESENCE_TYPE_AWAY TP_CONNECTION_PRESENCE_TYPE_AWAY
#define TP_CONN_PRESENCE_TYPE_EXTENDED_AWAY TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY
#define TP_CONN_PRESENCE_TYPE_HIDDEN TP_CONNECTION_PRESENCE_TYPE_HIDDEN

#define TP_CONN_STATUS_CONNECTED TP_CONNECTION_STATUS_CONNECTED
#define TP_CONN_STATUS_CONNECTING TP_CONNECTION_STATUS_CONNECTING
#define TP_CONN_STATUS_DISCONNECTED TP_CONNECTION_STATUS_DISCONNECTED
/* this is internal to Gabble and behaves like a member of TpConnectionStatus */
#define TP_CONNECTION_STATUS_NEW ((TpConnectionStatus)(LAST_TP_CONNECTION_STATUS + 1))
#define TP_CONN_STATUS_NEW TP_CONNECTION_STATUS_NEW

#define TP_CONN_STATUS_REASON_NONE_SPECIFIED TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED
#define TP_CONN_STATUS_REASON_REQUESTED TP_CONNECTION_STATUS_REASON_REQUESTED
#define TP_CONN_STATUS_REASON_NETWORK_ERROR TP_CONNECTION_STATUS_REASON_NETWORK_ERROR
#define TP_CONN_STATUS_REASON_AUTHENTICATION_FAILED TP_CONNECTION_STATUS_REASON_AUTHENTICATION_FAILED
#define TP_CONN_STATUS_REASON_ENCRYPTION_ERROR TP_CONNECTION_STATUS_REASON_ENCRYPTION_ERROR
#define TP_CONN_STATUS_REASON_NAME_IN_USE TP_CONNECTION_STATUS_REASON_NAME_IN_USE
#define TP_CONN_STATUS_REASON_CERT_NOT_PROVIDED TP_CONNECTION_STATUS_REASON_CERT_NOT_PROVIDED
#define TP_CONN_STATUS_REASON_CERT_UNTRUSTED TP_CONNECTION_STATUS_REASON_CERT_UNTRUSTED
#define TP_CONN_STATUS_REASON_CERT_EXPIRED TP_CONNECTION_STATUS_REASON_CERT_EXPIRED
#define TP_CONN_STATUS_REASON_CERT_NOT_ACTIVATED TP_CONNECTION_STATUS_REASON_CERT_NOT_ACTIVATED
#define TP_CONN_STATUS_REASON_CERT_HOSTNAME_MISMATCH TP_CONNECTION_STATUS_REASON_CERT_HOSTNAME_MISMATCH
#define TP_CONN_STATUS_REASON_CERT_FINGERPRINT_MISMATCH TP_CONNECTION_STATUS_REASON_CERT_FINGERPRINT_MISMATCH
#define TP_CONN_STATUS_REASON_CERT_SELF_SIGNED TP_CONNECTION_STATUS_REASON_CERT_SELF_SIGNED
#define TP_CONN_STATUS_REASON_CERT_OTHER_ERROR TP_CONNECTION_STATUS_REASON_CERT_OTHER_ERROR

G_END_DECLS


#endif
