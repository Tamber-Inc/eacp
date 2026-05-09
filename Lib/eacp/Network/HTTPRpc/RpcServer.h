#pragma once

#include <eacp/Network/HTTPServer/HttpServer.h>

#include <Miro/Miro.h>

#include <string>

namespace eacp::HTTP::Rpc
{

// Mounts a Miro::Bridge onto an HTTP::Server as a single POST endpoint
// at `basePath` (default "/rpc"). Wire protocol mirrors the WebView
// bridge: request body is { "command": "...", "payload": {...} },
// success response is { "result": <JSON> }, error response is the
// throwing handler's status code (or 404 for unknown commands) with
// body { "error": "..." }.
//
// Bridge ownership lies with the caller; the same Bridge can be wired
// to a WebView transport at the same time so a single set of typed
// handlers (including those declared via MIRO_EXPORT_COMMAND) is
// served over both wires.
//
// Lifetime: the Server must outlive the HTTP::Server it was attached
// to — the registered route handler captures `this`.
class Server
{
public:
    Server(eacp::HTTP::Server& server,
           Miro::Bridge& bridge,
           std::string basePath = "/rpc");

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

private:
    Response handle(const Request& req);

    Miro::Bridge& bridge;
    std::string basePath;
};

} // namespace eacp::HTTP::Rpc
