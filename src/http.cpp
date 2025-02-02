#include "http.h"
#include "SDL.h"
#include "SDL_endian.h"
#include "SDL_events.h"
#include "SDL_net.h"
#include "SDL_scancode.h"
#include "SDL_surface.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "inja.hpp"
#include <fstream>
#include <functional>
#include <ios>
#include <sstream>
#include <string>
#include <optional>
#include <vector>

namespace http {
    const int MAX_REQUEST_SIZE = 8192;

    std::map<std::string, std::string> contentTypes = {
        {"txt", "text/plain"},
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "text/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"pdf", "application/pdf"},
        {"bmp", "image/bmp"},
        {"gif", "image/gif"},
        {"jpeg", "image/jpeg"},
        {"jpg", "image/jpeg"},
        {"png", "image/png"},
        {"svg", "image/svg+xml"},
        {"tif", "image/tiff"},
        {"tiff", "image/tiff"},
        {"ico", "image/vnd.microsoft.icon"},
        {"map", "application/json"},
    };

    std::optional<std::string> getTextContent(std::string path);
    std::optional<std::string> getBinaryContent(std::string path);

    std::map<std::string, std::function<std::optional<std::string>(std::string)>> contentTypeCB = {
        {"text/plain", getTextContent},
        {"text/html", getTextContent},
        {"text/css", getTextContent},
        {"text/javascript", getTextContent},
        {"application/json", getTextContent},
        {"application/xml", getTextContent},
        {"application/pdf", getBinaryContent},
        {"image/bmp", getBinaryContent},
        {"image/gif", getBinaryContent},
        {"image/jpeg", getBinaryContent},
        {"image/png", getBinaryContent},
        {"image/svg+xml", getTextContent},
        {"image/tiff", getBinaryContent},
        {"image/vnd.microsoft.icon", getBinaryContent},
    };

    SDL_Window* window = nullptr;

    static TCPsocket server = nullptr, client = nullptr;
    static IPaddress ip;

    static std::map<std::string, std::function<void(Request*, Response*)>> get_callbacks;
    static std::map<std::string, std::function<void(Request*, Response*)>> post_callbacks;

    static bool isRunning = true;

    IPersistanceCallback* persistance = nullptr;

    void init() {
        SDL_Init(SDL_INIT_EVERYTHING);
        SDLNet_Init();


        window = SDL_CreateWindow("HTTP_SERVER", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320, 240, SDL_WINDOW_SHOWN);
    }

    void release() {

        SDL_DestroyWindow(window);

        SDLNet_Quit();
        SDL_Quit();
    }

    // This is were I'll be listening for client related stuff
    // This is the most hackish thing I've done in a while lol
    std::string urldecode(std::string str) {
        std::stringstream ss;

        for(int i = 0; i < str.length(); i++) {
            if(str[i] == '+') {
                ss << " ";
            } else if(str[i] == '%') {
                std::stringstream num;
                ++i;
                num << str[i];
                ++i;
                num << str[i];

                //ss << char(std::stoi(num.str()));
                
                int value = 0;
                num >> std::hex >> value;

                ss << char(value);

            } else {
                ss << str[i];
            }
        }

        return ss.str();
    }

    std::string grabData(std::ifstream& in) {
        std::string temp;
        in.seekg(0, std::ios::end);
        temp.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read((char*)temp.data(), temp.size());
        return temp;
    }

    std::optional<std::string> getTextContent(std::string path) {
        std::ifstream in(path);
        std::optional<std::string> content;

        if(!in.is_open()) {
            return content;
        }

        std::string temp = grabData(in);
        content = temp;
        in.close();

        return content;
    }

    std::optional<std::string> getBinaryContent(std::string path) {
        std::ifstream in(path, std::ios::binary);
        std::optional<std::string> content;

        if(!in.is_open()) {
            return content;
        }

        std::string temp = grabData(in);
        content = temp;
        in.close();

        return content;
    }

    void splitString(std::string str, char delim, std::vector<std::string>& args) {
        std::stringstream parse(str);
        std::string temp;
        while(std::getline(parse, temp, delim)) {
            args.push_back(temp);
        }
    }

    std::string getContentType(std::string path) {
        std::vector<std::string> args;
        splitString(path, '.', args);

        std::string contentType = "";

        if(contentTypes.find(args.at(args.size() - 1)) != contentTypes.end()) {
            contentType = contentTypes.at(args.at(args.size() - 1));
        }

        return contentType;
    }

    std::optional<std::string> getResource(std::string path) {
        std::stringstream filePath;
        filePath << "." << path;

        path = filePath.str();

        std::optional<std::string> ret;

        std::string contentType = getContentType(path);
        std::cout << contentType << "\n";

        ret = contentTypeCB[contentType](path);

        return ret;
    }

    void main_thread() {
        SDL_Event e;

        while(isRunning) {
            while(SDL_PollEvent(&e)) {
                if(e.type == SDL_QUIT) {
                    isRunning = false;
                }

                if(e.type == SDL_KEYDOWN) {
                    if(e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        isRunning = false;
                    }
                }
            }

            SDL_UpdateWindowSurface(window);
        }
    }

    void close_client() {
        SDLNet_TCP_Close(client);
        client = nullptr;
    }

    void send404() {
        std::stringstream ss;

        std::optional<std::string> content = getTextContent("error/404.html");

        ss << "HTTP/1.1 404 Not Found\n";
        ss << "Server: Derf Web Server\n";
        ss << "Cache-Control: no-store\n";
        ss << "Contnet-Type: " << "text/html" << "\n";
        ss << "\n";
        ss << content->c_str() << "\n";
        ss << " "; // Need to be sent correctly

        std::string response = ss.str();

        SDLNet_TCP_Send(client, (void*)response.data(), response.length());
    }

    void sendContent(std::string content, std::string contentType) {
        std::stringstream ss;

        ss << "HTTP/1.1 200 OK\n";
        ss << "Server: Derf Web Server\n";
        ss << "Cache-Control: no-store\n";
        ss << "Contnet-Type: " << contentType << "\n";
        ss << "\n";
        ss << content << "\n";
        ss << " "; // Need to be sent correctly

        std::string response = ss.str();

        SDLNet_TCP_Send(client, (void*)response.data(), response.length());
    }

    int client_thread(void* data) {

        if(persistance) {
            persistance->init();
        }

        while(isRunning) {
            if(!isRunning) {
                break;
            }

            //client = SDLNet_TCP_Accept(server);
            client = SDLNet_TCP_Accept(server);

            //std::cout << client << "Have a client connection...\n";

            if(!client) {
                continue;
            }

            IPaddress* remoteip;
            remoteip = SDLNet_TCP_GetPeerAddress(client);

            uint32_t ipaddr = SDL_Swap32(remoteip->host);

            // Build Request
            std::string request;
            request.resize(MAX_REQUEST_SIZE);
            int len = SDLNet_TCP_Recv(client, (void*)request.data(), request.length());

            if(!len) {
                continue;
            }

            Request req;
            Response res;

            req.parseRequestString(request);

            // Handle Callbacks or Static content

            if(req.isStaticContent) {
                // Do nothing for the moment...
                std::string contentType = getContentType(req.path);
                std::optional<std::string> content = getResource(req.path);

                if(content.has_value()) {
                    sendContent(content.value(), contentType);
                } else {
                    send404();
                }

            } else {
                if(req.method == "GET") {
                    //get_callbacks[req.path](&req, &res);
                    if(get_callbacks.find(req.path) != get_callbacks.end()) {
                        get_callbacks.at(req.path)(&req, &res);
                    } else {
                        send404();
                    }
                } else if(req.method == "POST") {
                    //post_callbacks[req.path](&req, &res);
                    if(post_callbacks.find(req.path) != post_callbacks.end()) {
                        post_callbacks.at(req.path)(&req, &res);
                    } else {
                        send404();
                    }
                } else {
                    // This will send back a 404 page when I get around to implement
                    // it
                    send404();
                }
            }
            // Close Client
            close_client();
        }

        if(client) {
            close_client();
        }

        if(persistance) {
            persistance->release();
        }

        persistance = nullptr;
        
        return 0;
    }

    void setPersistanceCallback(IPersistanceCallback* cb) {
        persistance = cb;
    }

    void listen(int port) {
        // Resolve Host
        SDLNet_ResolveHost(&ip, nullptr, port);

        server = SDLNet_TCP_Open(&ip);


        if(!server) {
            std::cerr << "SDLNet_TCP_Open: " << SDLNet_GetError() << "\n";
            exit(-2);
        }

        std::cout << "Listening on port: http://localhost:"<< port << "/\n";
        std::cout << "Please press the 'escape' key to exit!\n";

        // The main thread is waiting for SDL Events to exit

        SDL_Thread* clientThread = SDL_CreateThread(client_thread, "client_thread", nullptr);

        main_thread();
        int res = 0;
        SDLNet_TCP_Close(server);

        SDL_WaitThread(clientThread, &res);
    }

    void get(std::string path, std::function<void(Request*, Response*)> callback) {
        get_callbacks[path] = callback;
    }

    void post(std::string path, std::function<void(Request*, Response*)> callback) {
        post_callbacks[path] = callback;
    }


    // Request
    void Request::parseRequestString(std::string request) {
        // This will parse the request string
        //std::cout << request << "\n";

        std::vector<std::string> lines;
        splitString(request, '\n', lines);

        //std::cout << "Lines\n";

        std::vector<std::string> mph;
        splitString(lines[0], ' ', mph);

        this->method = mph[0];
        this->path = mph[1];
        this->http = mph[2];

        //std::cout << "Parsing method path + http\n";

        // Break Question mark
        std::vector<std::string> queryString;
        splitString(this->path, '?', queryString);

        //std::cout << "Grabing Query String!\n";
        //std::cout << queryString.size() << "\n";
        
        //std::cout << "Path: " << queryString[0] << " -> QueryString: " << queryString[1] << "\n";
        // Parse Query String
        if(queryString.size() > 1) {
            std::vector<std::string> queryStringVariables;
            splitString(queryString[1], '&', queryStringVariables);

            for(int i = 0; i < queryStringVariables.size(); i++) {
                std::vector<std::string> variable;
                splitString(queryStringVariables[i], '=', variable);
                this->get_params[variable[0]] = variable[1];
            }
        }

        //std::cout << "Parsed query string\n";
        
        this->path = queryString[0];

        // Handle Headers
        for(int i = 1; i < lines.size(); i++) {
            std::vector<std::string> args;

            splitString(lines[i], ':', args);

            if(args.size() == 2) {
                this->header[args[0]] = args[1];
            } else if(args.size() > 2) {
                std::stringstream ss;

                for(int j = 1; j < args.size(); j++) {
                    ss << args[j];
                }

                this->header[args[0]] = ss.str();
            }
        }


        // Post Check and Grab post_params
        if(this->method == "POST") {
            std::string pstr = lines[lines.size() - 1];
            // Get rid of all the nulls
            std::vector<std::string> helper;
            splitString(pstr, '\0', helper);
            std::cout << helper.size() << "\n";
            std::cout << helper[0] << "\n";
            pstr = helper[0];

            // Brake query
            std::vector<std::string> params;
            splitString(pstr, '&', params);

            for(int i = 0; i < params.size(); i++) {
                std::vector<std::string> pp;
                splitString(params[i], '=', pp);
                
                std::cout << pp.size() << "\n";

                //this->post_params[pp[0]] = pp[1];

                if(pp.size() == 1) {
                    this->post_params[pp[0]] = "";
                } else if(pp.size() == 2) {
                    this->post_params[pp[0]] = pp[1];
                }
            }
        }

        // Check if path is a static path
        // Example static path /static/img/test.png
        std::vector<std::string> pathCheck;
        splitString(this->path, '/', pathCheck);

        if(pathCheck[1] == "static" || pathCheck[1] == "favicon.ico") {
            this->isStaticContent = true;
        }
    }

    // Response
    void Response::send(std::string content) {
        sendContent(content, this->contentType);
    }

    void Response::redirect(std::string path) {
        std::stringstream ss;

        ss << "HTTP/1.1 302 Found\n";
        ss << "Location: " << path << "\n";
        ss << "Content-Type: text/html; charset=UTF-8\n";
        ss << "Content-Length: 0\n";

        std::string response = ss.str();

        SDLNet_TCP_Send(client, (void*)response.data(), response.length());
    }

    void Response::render(std::string view, inja::json root) {
        std::stringstream filePath;
        filePath << "views/" << view << ".html";

        this->contentType = "text/html";

        std::optional<std::string> content = getTextContent(filePath.str());

        std::string actualContent = inja::render(content->c_str(), root);

        this->send(actualContent);
    }
}