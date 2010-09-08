/*
 * stream-tube-chan.h - Simple stream tube channel
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TP_STREAM_TUBE_CHAN_H__
#define __TP_STREAM_TUBE_CHAN_H__

#include <glib-object.h>
#include <telepathy-glib/base-channel.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/text-mixin.h>

G_BEGIN_DECLS

typedef struct _TpTestsStreamTubeChannel TpTestsStreamTubeChannel;
typedef struct _TpTestsStreamTubeChannelClass TpTestsStreamTubeChannelClass;
typedef struct _TpTestsStreamTubeChannelPrivate TpTestsStreamTubeChannelPrivate;

GType tp_tests_stream_tube_channel_get_type (void);

#define TP_TESTS_TYPE_STREAM_TUBE_CHANNEL \
  (tp_tests_stream_tube_channel_get_type ())
#define TP_TESTS_STREAM_TUBE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TESTS_TYPE_STREAM_TUBE_CHANNEL, \
                               TpTestsStreamTubeChannel))
#define TP_TESTS_STREAM_TUBE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TESTS_TYPE_STREAM_TUBE_CHANNEL, \
                            TpTestsStreamTubeChannelClass))
#define TP_TESTS_IS_STREAM_TUBE_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TESTS_TYPE_STREAM_TUBE_CHANNEL))
#define TP_TESTS_IS_STREAM_TUBE_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TESTS_TYPE_STREAM_TUBE_CHANNEL))
#define TP_TESTS_STREAM_TUBE_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TESTS_TYPE_STREAM_TUBE_CHANNEL, \
                              TpTestsStreamTubeChannelClass))

struct _TpTestsStreamTubeChannelClass {
    TpBaseChannelClass parent_class;
    TpTextMixinClass text_class;
    TpDBusPropertiesMixinClass dbus_properties_class;
};

struct _TpTestsStreamTubeChannel {
    TpBaseChannel parent;
    TpTextMixin text;

    TpTestsStreamTubeChannelPrivate *priv;
};

GSocketAddress * tp_tests_stream_tube_channel_get_server_address (
    TpTestsStreamTubeChannel *self);

void tp_tests_stream_tube_channel_peer_connected (
    TpTestsStreamTubeChannel *self,
    GIOStream *stream,
    TpHandle handle);

G_END_DECLS

#endif /* #ifndef __TP_STREAM_TUBE_CHAN_H__ */
