#include "inja.hpp"
#include "message.h"
#include "http.h"
#include <cctype>
#include <ios>
#include <sstream>
#include <vector>
#include <algorithm>




struct MessagePersistanceCallback : public http::IPersistanceCallback {
    virtual void init() {
        msg::init();
    }

    virtual void release() {
        msg::release();
    }

};

int main(int argc, char** argv) {
    MessagePersistanceCallback mpc;

    http::init();

    http::get("/", [&](http::Request* req, http::Response* res) {
        // This will do something in a minute
        res->redirect("/index");
    });

    http::get("/index", [&](http::Request* req, http::Response* res) {
        inja::json root;

        std::vector<msg::Message> messages;

        msg::getAllMessages(&messages);

        root["messages"] = inja::json::array();

        for(int i = 0; i < messages.size(); i++) {
            //root["messages"].push_back()

            inja::json obj = inja::json::object();
            //obj["id"] = messages[i].id;
            //obj["message"] = messages[i].message;

            obj["id"] = inja::json(messages[i].id);
            obj["message"] = inja::json(messages[i].message);
            
            root["messages"].push_back(obj);
        }

        res->render("index", root);
    });

    http::get("/new", [&](http::Request* req, http::Response* res) {
        inja::json root;
        if(req->get_params.find("err") != req->get_params.end()) {
            root["err"] = inja::json(http::urldecode(req->get_params.at("err")));
        } else {
            root["err"] = inja::json(false);
        }
        res->render("new", root);
    });

    http::post("/new", [&](http::Request* req, http::Response* res) {
        
        std::string message = http::urldecode(req->post_params["message"]);

        if(message.empty()) {
            std::string errMessage = "Message must contain a value!";
            res->redirect("/new?err="+errMessage);
        } else {
            msg::Message m;

            m.message = message;

            msg::insertMessage(&m);

            res->redirect("/");
        }
    });

    http::get("/edit", [&](http::Request* req, http::Response* res) {
        inja::json root;

        if(req->get_params.find("err") != req->get_params.end()) {
            root["err"] = inja::json(http::urldecode(req->get_params.at("err")));
        } else {
            root["err"] = inja::json(false);
        }

        msg::Message m;

        if(req->get_params.find("id") == req->get_params.end()) {
            res->redirect("/");
        } else {

            std::string id = http::urldecode(req->get_params.at("id"));

            bool is_digit = std::find_if(id.begin(), id.end(), [](unsigned char c) { return !std::isdigit(c); }) == id.end();
            
            if(id.empty() || !is_digit) {
                res->redirect("/");
            } else {
                m.id = std::stoi(id);

                msg::getMessage(&m);

                if(m.message.empty()) {
                    // Most likely this message doesn't exist
                    res->redirect("/");
                } else {
                    inja::json message;
                    message["id"] = inja::json(m.id);
                    message["message"] = inja::json(m.message);

                    root["message"] = message;

                    res->render("edit", root);
                }
            }
        }
    });

    http::post("/edit", [&](http::Request* req, http::Response* res) {

        msg::Message m;
        
        m.id = std::stoi(http::urldecode(req->get_params.at("id")));
        m.message = http::urldecode(req->post_params["message"]);

        if(m.message.empty()) {
            std::string errMessage = "Message must contain a value!";
            std::stringstream ss;
            ss << "/edit?id=" << m.id << "&err=" << errMessage;

            res->redirect(ss.str());
        } else {
            msg::updateMessage(&m);

            res->redirect("/");
        }
    });

    http::get("/delete", [&](http::Request* req, http::Response* res) {
        inja::json root;

        if(req->get_params.find("id") == req->get_params.end()) {
            res->redirect("/");
        } else {
            std::string id = http::urldecode(req->get_params.at("id"));

            bool is_digit = std::find_if(id.begin(), id.end(), [](unsigned char c) { return !std::isdigit(c); }) == id.end();

            if(id.empty() || !is_digit) {
                res->redirect("/");
            } else {
                msg::Message m;
                m.id = std::stoi(id);

                msg::getMessage(&m);


                if(m.message.empty()) {
                    res->redirect("/");
                } else {
                    inja::json message;

                    
                    message["id"] = inja::json(m.id);
                    message["message"] = inja::json(m.message);

                    root["message"] = message;

                    res->render("delete", root);
                }
            }
        }
    });

    http::post("/delete", [&](http::Request* req, http::Response* res) {
        msg::Message m;

        m.id = std::stoi(http::urldecode(req->get_params.at("id")));
        
        msg::deleteMessage(&m);

        res->redirect("/");
    });

    http::setPersistanceCallback(&mpc);

    http::listen(8080);

    http::release();

    return 0;
}