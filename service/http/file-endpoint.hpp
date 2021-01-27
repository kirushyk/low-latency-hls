#pragma once
#include <glib.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

void set_error_message(SoupMessage *msg, guint16 code);

void file_callback(SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext *context, gpointer data);

G_END_DECLS

