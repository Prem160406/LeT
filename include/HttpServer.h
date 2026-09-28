#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "Engine.h"
#include <string>

/**
 * @brief HTTP REST Server exposing Engine functionality to the web frontend.
 */
class HttpServer {
private:
    Engine& engine;
    int port;

public:
    HttpServer(Engine& eng, int p = 8080);
    ~HttpServer();

    /**
     * @brief Starts the synchronous single-threaded HTTP listener loop.
     */
    void start();
};

#endif // HTTP_SERVER_H
