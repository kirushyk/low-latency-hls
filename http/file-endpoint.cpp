#include "config.h"
#include "file-endpoint.hpp"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libsoup/soup.h>

void set_error_message(SoupMessage *msg, guint16 code)
{
    const char *phrase = soup_status_get_phrase(code);
    soup_message_set_response(msg, "text/plain", SOUP_MEMORY_STATIC, phrase, strlen(phrase));
    soup_message_set_status_full(msg, code, phrase);
}

static void do_get(SoupServer *server, SoupMessage *msg, const char *path)
{
    const char *slash;
    struct stat st;

    if (stat(path, &st) == -1)
    {
        if (errno == EPERM)
        {
            set_error_message(msg, SOUP_STATUS_FORBIDDEN);
        }
        else if (errno == ENOENT)
        {
            set_error_message(msg, SOUP_STATUS_NOT_FOUND);
        }
        else
        {
            set_error_message(msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
        }
        return;
    }

    if (S_ISDIR(st.st_mode))
    {
        char *index_path;

        slash = strrchr(path, '/');
        if (!slash || slash[1])
        {
            char *redir_uri = g_strdup_printf("%s/", soup_message_get_uri(msg)->path);
            soup_message_set_redirect(msg, SOUP_STATUS_MOVED_PERMANENTLY, redir_uri);
            g_free(redir_uri);
            return;
        }

        index_path = g_strdup_printf("%s/index.html", path);
        if (stat(index_path, &st) != -1)
        {
            do_get(server, msg, index_path);
            g_free(index_path);
            return;
        }
        g_free(index_path);

        soup_message_set_status(msg, SOUP_STATUS_OK);
        return;
    }

    if (msg->method == SOUP_METHOD_GET)
    {
        GMappedFile *mapping;
        SoupBuffer *buffer;

        mapping = g_mapped_file_new(path, FALSE, NULL);
        if (!mapping)
        {
            set_error_message(msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
            return;
        }

        buffer = soup_buffer_new_with_owner(g_mapped_file_get_contents(mapping), g_mapped_file_get_length(mapping), mapping, (GDestroyNotify)g_mapped_file_unref);
        if (buffer->length > 0)
        {
            soup_message_body_append_buffer(msg->response_body, buffer);
            soup_message_headers_append(msg->response_headers, "Cache-Control", "public, max-age=86400");
            soup_message_set_status(msg, SOUP_STATUS_OK);
        }
        else
        {
            soup_message_set_status(msg, SOUP_STATUS_NO_CONTENT);
        }
        soup_buffer_free(buffer);
    }
    else if (msg->method == SOUP_METHOD_HEAD)
    {
        gchar *length = g_strdup_printf("%lu", (gulong)st.st_size);
        soup_message_headers_append(msg->response_headers, "Content-Length", length);
        g_free(length);
        soup_message_set_status(msg, SOUP_STATUS_OK);
    }
    else
    {
        set_error_message(msg, SOUP_STATUS_NOT_IMPLEMENTED);
    }
}

gchar * g_strreplace(const gchar *string, const gchar *search, const gchar *replacement)
{
    gchar *str, **arr;
    g_return_val_if_fail(string != NULL, NULL);
    g_return_val_if_fail(search != NULL, NULL);

    if (replacement == NULL)
        replacement = "";

    arr = g_strsplit(string, search, -1);
    if (arr != NULL && arr[0] != NULL)
        str = g_strjoinv(replacement, arr);
    else
        str = g_strdup(string);

    g_strfreev(arr);

    return str;
}

void file_callback(SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext *context, gpointer data)
{
    char *file_path;

    char *relative_path = g_strreplace(path, "/ui", NULL);
    file_path = g_strdup_printf("%s%s", UI_SOURCE_PATH, relative_path);
    g_free(relative_path);

    if (msg->method == SOUP_METHOD_GET || msg->method == SOUP_METHOD_HEAD)
    {
        do_get(server, msg, file_path);
    }
    else
    {
        set_error_message(msg, SOUP_STATUS_NOT_IMPLEMENTED);
    }

    g_free(file_path);
}
