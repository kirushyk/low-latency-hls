#include "config.h"
#include "http-api.hpp"
#include <iostream>
#include <zlib.h>
#include "file-endpoint.hpp"

HTTPAPI::HTTPAPI(const int port)
{
    http_server = soup_server_new(SOUP_SERVER_SERVER_HEADER, APPLICATION_NAME, NULL);
    soup_server_add_handler(http_server, NULL, [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/api/hack.ts", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(http_server, "/api/segments/", [](SoupServer *, SoupMessage *msg, const char *path, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        int segmentNumber = 0;
        sscanf(path + 14, "%d.ts", &segmentNumber);
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::shared_ptr<HLSSegment> segment = hlsOutput->getSegment(segmentNumber);
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        if (segment)
        {
            soup_message_headers_append(msg->response_headers, "Content-Type", "video/mp2t");
            soup_message_set_status(msg, SOUP_STATUS_OK);
            for (const auto &b: segment->data)
            {
                soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, (gchar *)b.data(), b.size());
            }
            soup_message_body_complete(msg->response_body);
        }
        else
        {
            set_error_message(msg, SOUP_STATUS_NOT_FOUND);
        }
    }, hlsOutput.get(), NULL);
    soup_server_add_handler(http_server, "/api/partial/", [](SoupServer *, SoupMessage *msg, const char *path, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        int segmentNumber = 0;
        int partialSegmentNumber = 0;
        sscanf(path + 13, "%d.%d.ts", &segmentNumber, &partialSegmentNumber);
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::shared_ptr<HLSSegment> segment = hlsOutput->getSegment(segmentNumber);
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        if (segment)
        {
            std::shared_ptr<HLSPartialSegment> partialSegment = segment->getPartialSegment(partialSegmentNumber);
            if (partialSegment)
            {
                soup_message_headers_append(msg->response_headers, "Content-Type", "video/mp2t");
                soup_message_set_status(msg, SOUP_STATUS_OK);
                for (const auto &b: partialSegment->data)
                {
                    soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, (gchar *)b.data(), b.size());
                }
                std::cerr << "" << path << ", sent " << partialSegment->data.size() << " chunks" << std::endl;
                soup_message_body_complete(msg->response_body);
            }
            else
            {
                std::cerr << "" << path << ", partial segment not found" << std::endl;
                set_error_message(msg, SOUP_STATUS_NOT_FOUND);
            }
        }
        else
        {
            std::cerr << "" << path << ", segment not found" << std::endl;
            set_error_message(msg, SOUP_STATUS_NOT_FOUND);
        }
    }, hlsOutput.get(), NULL);
    soup_server_add_handler(http_server, "/api/plain.m3u8", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::string playlist = hlsOutput->getPlaylist();
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        soup_message_headers_append(msg->response_headers, "Content-Type", "application/vnd.apple.mpegURL");
        soup_message_headers_append(msg->response_headers, "Content-Encoding", "gzip");
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = (uInt)playlist.size();
        zs.next_in = (Bytef *)playlist.c_str();
        char chunk[PLAYLIST_CHUNK_SIZE];
        deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
        do
        {
            zs.avail_out = (uInt)PLAYLIST_CHUNK_SIZE;
            zs.next_out = (Bytef *)chunk;
            deflate(&zs, Z_FINISH);
            int have = PLAYLIST_CHUNK_SIZE - zs.avail_out;
            soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, chunk, have);
        }
        while (zs.avail_out == 0);
        deflateEnd(&zs);
        soup_message_set_status(msg, SOUP_STATUS_OK);
	    soup_message_body_complete(msg->response_body);
    }, hlsOutput.get(), NULL);
    soup_server_add_handler(http_server, "/api/lhls.m3u8", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        HLSOutput *hlsOutput = reinterpret_cast<HLSOutput *>(user_data);
        std::string playlist = hlsOutput->getLowLatencyPlaylist();
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        soup_message_headers_append(msg->response_headers, "Content-Type", "application/vnd.apple.mpegURL");
        soup_message_headers_append(msg->response_headers, "Content-Encoding", "gzip");
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = (uInt)playlist.size();
        zs.next_in = (Bytef *)playlist.c_str();
        char chunk[PLAYLIST_CHUNK_SIZE];
        deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
        do
        {
            zs.avail_out = (uInt)PLAYLIST_CHUNK_SIZE;
            zs.next_out = (Bytef *)chunk;
            deflate(&zs, Z_FINISH);
            int have = PLAYLIST_CHUNK_SIZE - zs.avail_out;
            soup_message_body_append(msg->response_body, SOUP_MEMORY_COPY, chunk, have);
        }
        while (zs.avail_out == 0);
        deflateEnd(&zs);
        soup_message_set_status(msg, SOUP_STATUS_OK);
	    soup_message_body_complete(msg->response_body);
    }, hlsOutput.get(), NULL);
    soup_server_add_handler(http_server, "/ui", file_callback, NULL, NULL);
    soup_server_add_handler(http_server, "/", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        const char *phrase = soup_status_get_phrase(SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(msg->response_headers, "Location", "/ui");
        soup_message_set_response(msg, "text/plain", SOUP_MEMORY_STATIC, phrase, strlen(phrase));
        soup_message_set_status_full(msg, SOUP_STATUS_MOVED_PERMANENTLY, phrase);
    }, NULL, NULL);
    if (soup_server_listen_all(http_server, port, static_cast<SoupServerListenOptions>(0), 0))
        g_print("server listening on port %d\n", port);
    else
        g_printerr("port %d could not be bound\n", port);
}

HTTPAPI::~HTTPAPI()
{
    soup_server_disconnect(http_server);
}

void HTTPAPI::onPartialSegment()
{

}

void HTTPAPI::onSegment()
{

}
