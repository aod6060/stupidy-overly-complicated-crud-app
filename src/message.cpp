#include "message.h"
#include <sstream>
#include <string>
#include <vector>


namespace msg {


    static sqlite3* database = nullptr;

    void init() {
        int res = sqlite3_open("test.db", &database);

        if(res) {
            std::cerr << "Error open db: " << sqlite3_errmsg(database) << "\n";
            exit(-1);
        }
    }

    void release() {
        sqlite3_close(database);
        database = nullptr;
    }

    void getAllMessages(std::vector<Message>* messages) {
        sqlite3_exec(
            database, 
            "SELECT * FROM message", 
            [](void* data, int argc, char** argv, char** azColName) {
                std::vector<Message>* messages = (std::vector<Message>*)data;

                Message m;

                m.id = std::stoi(argv[0]);
                m.message = argv[1];

                messages->push_back(m);

                return 0;
            }, 
            (void*)messages,
            nullptr
        );
    }
    
    void getMessage(Message* message) {
        std::stringstream ss;
        ss << "SELECT * FROM message WHERE id=" << message->id;

        std::string sql = ss.str();

        sqlite3_exec(
            database,
            sql.data(),
            [](void* data, int argc, char** argv, char** azColName) {
                Message* message = (Message*) data;
                message->id = std::stoi(argv[0]);
                message->message = argv[1];
                return 0;
            },
            message,
            nullptr
        );

    }

    void insertMessage(Message* message) {
        std::stringstream ss;
        ss << "INSERT INTO message(message) VALUES(\""<< message->message <<"\")";
        std::string sql = ss.str();
        sqlite3_exec(database, sql.data(), nullptr, nullptr, nullptr);
    }

    void updateMessage(Message* message) {
        std::stringstream ss;
        ss << "UPDATE message SET message=\""<<message->message<<"\" WHERE id=" << message->id;
        std::string sql = ss.str();
        sqlite3_exec(database, sql.data(), nullptr, nullptr, nullptr);
    }

    void deleteMessage(Message* message) {
        std::stringstream ss;
        ss << "DELETE from message WHERE id=" << message->id;
        std::string sql = ss.str();
        sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr);
    }

}