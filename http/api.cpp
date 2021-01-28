#include "config.h"
#include "api.hpp"
#include <cstdlib>
#include <list>
#include <memory>
#include <iostream>
#include <zlib.h>
#include "file-endpoint.hpp"
#include "playlist-request.hpp"

struct HTTPAPI::Private: public HLSOutput::Delegate
{
    SoupServer *http_server;
    std::shared_ptr<HLSOutput> hlsOutput;
    void removeProcessedRequests();
    virtual void onPartialSegment() override final;
    virtual void onSegment() override final;
    std::list<std::shared_ptr<PlaylistRequest>> playlistRequests;
};

void HTTPAPI::Private::removeProcessedRequests()
{
    playlistRequests.remove_if([](std::shared_ptr<PlaylistRequest> &request) { return request->processed; });
}

static void message_body_append_compressed_text(SoupMessageBody *message_body, std::string text)
{
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = (uInt)text.size();
    zs.next_in = (Bytef *)text.c_str();
    char chunk[PLAYLIST_CHUNK_SIZE];
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
    do
    {
        zs.avail_out = (uInt)PLAYLIST_CHUNK_SIZE;
        zs.next_out = (Bytef *)chunk;
        deflate(&zs, Z_FINISH);
        int have = PLAYLIST_CHUNK_SIZE - zs.avail_out;
        soup_message_body_append(message_body, SOUP_MEMORY_COPY, chunk, have);
    }
    while (zs.avail_out == 0);
    deflateEnd(&zs);
}

void HTTPAPI::Private::onPartialSegment()
{
    for (auto &request: playlistRequests)
    {
        if (request->partIndex != -1)
        {
            if (hlsOutput->partialSegmentReady(request->mediaSequenceNumber, request->partIndex))
            {
                std::string playlist = hlsOutput->getPlaylist(true, request->skip);
                message_body_append_compressed_text(request->msg->response_body, playlist);

                soup_message_set_status(request->msg, SOUP_STATUS_OK);
                soup_message_body_complete(request->msg->response_body);

                soup_server_unpause_message(http_server, request->msg);

                std::cerr << "We have partial segment " << request->mediaSequenceNumber << "." <<
                    request->partIndex << ", unpausing blocked playlist request" << std::endl;

                request->processed = true;
            }
        }
    }
    removeProcessedRequests();
}

void HTTPAPI::Private::onSegment()
{
    for (auto &request: playlistRequests)
    {
        if (request->partIndex == -1)
        {
            if (hlsOutput->segmentReady(request->mediaSequenceNumber))
            {
                std::string playlist = hlsOutput->getPlaylist(true, request->skip);
                message_body_append_compressed_text(request->msg->response_body, playlist);

                soup_message_set_status(request->msg, SOUP_STATUS_OK);
                soup_message_body_complete(request->msg->response_body);

                soup_server_unpause_message(http_server, request->msg);

                std::cerr << "We have segment " << request->mediaSequenceNumber <<
                    ", unpausing blocked playlist request" << std::endl;

                request->processed = true;
            }
        }
    }
    removeProcessedRequests();
}

HTTPAPI::HTTPAPI(const int port, std::shared_ptr<HLSOutput> hlsOutput):
    priv(std::make_shared<Private>())
{
    priv->hlsOutput = hlsOutput;
    hlsOutput->setDelegate(priv);

    priv->http_server = soup_server_new(SOUP_SERVER_SERVER_HEADER, APPLICATION_NAME, NULL);
    soup_server_add_handler(priv->http_server, NULL, [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(priv->http_server, "/api/hack.ts", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        set_error_message(msg, SOUP_STATUS_NOT_FOUND);
    }, NULL, NULL);
    soup_server_add_handler(priv->http_server, "/api/segments/", [](SoupServer *, SoupMessage *msg, const char *path, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        int segmentNumber = 0;
        sscanf(path + 14, "%d.ts", &segmentNumber);
        std::shared_ptr<HLSOutput> hlsOutput = reinterpret_cast<HTTPAPI::Private *>(user_data)->hlsOutput;
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
            std::cerr << "" << path << ", sent " << segment->data.size() << " chunks" << std::endl;
            soup_message_body_complete(msg->response_body);
        }
        else
        {
            std::cerr << "" << path << ", segment not found" << std::endl;
            set_error_message(msg, SOUP_STATUS_NOT_FOUND);
        }
    }, priv.get(), NULL);
    soup_server_add_handler(priv->http_server, "/api/partial/", [](SoupServer *, SoupMessage *msg, const char *path, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        int segmentNumber = 0;
        int partialSegmentNumber = 0;
        sscanf(path + 13, "%d.%d.ts", &segmentNumber, &partialSegmentNumber);
        std::shared_ptr<HLSOutput> hlsOutput = reinterpret_cast<HTTPAPI::Private *>(user_data)->hlsOutput;
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
    }, priv.get(), NULL);
    soup_server_add_handler(priv->http_server, "/api/plain.m3u8", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer user_data)
    {
        std::shared_ptr<HLSOutput> hlsOutput = reinterpret_cast<HTTPAPI::Private *>(user_data)->hlsOutput;
        std::string playlist = hlsOutput->getPlaylist(false, false);
        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        soup_message_headers_append(msg->response_headers, "Content-Type", "application/vnd.apple.mpegURL");
        soup_message_headers_append(msg->response_headers, "Content-Encoding", "gzip");

        message_body_append_compressed_text(msg->response_body, playlist);

        soup_message_set_status(msg, SOUP_STATUS_OK);
	    soup_message_body_complete(msg->response_body);
    }, priv.get(), NULL);
    soup_server_add_handler(priv->http_server, "/api/lhls.m3u8", [](SoupServer *server, SoupMessage *msg, const char *, GHashTable *query, SoupClientContext *, gpointer user_data)
    {
        HTTPAPI::Private *priv = reinterpret_cast<HTTPAPI::Private *>(user_data);
        std::shared_ptr<HLSOutput> hlsOutput = priv->hlsOutput;

        soup_message_headers_append(msg->response_headers, "Cache-Control", "no-cache, no-store, must-revalidate");
        soup_message_headers_append(msg->response_headers, "Pragma", "no-cache");
        soup_message_headers_append(msg->response_headers, "Content-Type", "application/vnd.apple.mpegURL");
        soup_message_headers_append(msg->response_headers, "Content-Encoding", "gzip");

        const gchar *_HLS_msn = NULL;
        const gchar *_HLS_part = NULL;
        const gchar *_HLS_skip = NULL;
        bool skip = false;
        if (query)
        {
            _HLS_msn = (const gchar *)g_hash_table_lookup(query, "_HLS_msn");
            _HLS_part = (const gchar *)g_hash_table_lookup(query, "_HLS_part");
            _HLS_skip = (const gchar *)g_hash_table_lookup(query, "_HLS_skip");
            if (_HLS_msn)
            {
                std::cerr << "_HLS_msn=" << _HLS_msn;
            }
            if (_HLS_part)
            {
                std::cerr << "_HLS_part=" << _HLS_part;
            }
            if (_HLS_skip)
            {
                std::cerr << "_HLS_skip=" << _HLS_skip;
            }
            skip = (_HLS_skip != NULL);
            std::cerr << std::endl;
            int requestedMediaSequenceNumber = -1;
            int requestedPartIndex = -1;
            bool playlistReady = true;
            if (_HLS_msn)
            {
                requestedMediaSequenceNumber = std::atoi(_HLS_msn);
                if (hlsOutput->msnWrong(requestedMediaSequenceNumber))
                {
                    soup_message_set_status(msg, SOUP_STATUS_BAD_REQUEST);
                    soup_message_body_complete(msg->response_body);
                    return;
                }
                if (_HLS_part)
                {
                    requestedPartIndex = std::atoi(_HLS_part);
                    playlistReady = hlsOutput->partialSegmentReady(requestedMediaSequenceNumber, requestedPartIndex);
                }
                else
                {
                    playlistReady = hlsOutput->segmentReady(requestedMediaSequenceNumber);
                }
            }
            if (playlistReady)
            {
                std::cerr << "Playlist ready" << std::endl;
            }
            else
            {
                std::cerr << "Blocking playlist response...";
                std::cerr << std::endl;
                soup_server_pause_message(server, msg);
                auto playlistRequest = std::make_shared<PlaylistRequest>();
                playlistRequest->msg = msg;
                playlistRequest->mediaSequenceNumber = requestedMediaSequenceNumber;
                playlistRequest->partIndex = requestedPartIndex;
                playlistRequest->skip = skip;
                playlistRequest->processed = false;
                priv->playlistRequests.push_back(playlistRequest);
                return;
            }
        }

        std::string playlist = hlsOutput->getPlaylist(true, skip);
        
        message_body_append_compressed_text(msg->response_body, playlist);

        soup_message_set_status(msg, SOUP_STATUS_OK);
	    soup_message_body_complete(msg->response_body);
    }, priv.get(), NULL);
    soup_server_add_handler(priv->http_server, "/ui", file_callback, NULL, NULL);
    soup_server_add_handler(priv->http_server, "/", [](SoupServer *, SoupMessage *msg, const char *, GHashTable *, SoupClientContext *, gpointer)
    {
        const char *phrase = soup_status_get_phrase(SOUP_STATUS_MOVED_PERMANENTLY);
        soup_message_headers_append(msg->response_headers, "Location", "/ui");
        soup_message_set_response(msg, "text/plain", SOUP_MEMORY_STATIC, phrase, strlen(phrase));
        soup_message_set_status_full(msg, SOUP_STATUS_MOVED_PERMANENTLY, phrase);
    }, NULL, NULL);
    if (soup_server_listen_all(priv->http_server, port, static_cast<SoupServerListenOptions>(0), 0))
    {
        std::cout << "server listening on port " << port << std::endl;
    }
    else
    {
        std::cerr << "port " << port << " could not be bound" << std::endl;
    }
}

HTTPAPI::~HTTPAPI()
{
    soup_server_disconnect(priv->http_server);
}
