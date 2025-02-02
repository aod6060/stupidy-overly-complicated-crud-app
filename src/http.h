#ifndef HTTP_H
#define HTTP_H

/*
    This is a small lightweight web server
*/
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <SDL.h>
#include <SDL_net.h>
#include "inja.hpp"

namespace http {

    /*
        IPersistanceCallback ~ This is an interface that will allow you to
        initialize and release all of your database/rest api interface. This 
        will be called during an internal thead.
    */
    struct IPersistanceCallback {
        virtual void init() = 0;
        virtual void release() = 0;
    };

    struct Request {
        std::string method;
        std::string path;
        std::string http;

        std::map<std::string, std::string> header;

        // These will be explained later and will require the
        // Header to be parsed first before getting 
        // This information.
        std::map<std::string, std::string> get_params;
        std::map<std::string, std::string> post_params;

        bool isStaticContent = false;


        void parseRequestString(std::string request);
    };

    struct Response {
        // This represents the content being sent to the client. default: "text/plain" for now!
        std::string contentType = "text/plain";

        void send(std::string message);

        void redirect(std::string path);

        void render(std::string view, inja::json root);
    };

    void init();
    void release();
    
    // Call this before you call the listener
    void setPersistanceCallback(IPersistanceCallback* cb);

    void listen(int port);

    void get(std::string path, std::function<void(Request*, Response*)> callback);
    void post(std::string path, std::function<void(Request*, Response*)> callback);

    std::string urldecode(std::string str);
}

#endif