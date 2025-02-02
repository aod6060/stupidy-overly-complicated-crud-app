#ifndef MESSAGE_H
#define MESSAGE_H




#include "sqlite3.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


namespace msg {
    struct Message {
        int id;
        std::string message;

        Message() {}

        Message(int id, std::string message) : id(id), message(message) {}

    };

    void init();

    void release();

    void getAllMessages(std::vector<Message>* messages);
    
    void getMessage(Message* message);

    void insertMessage(Message* message);

    void updateMessage(Message* message);

    void deleteMessage(Message* message);

}
#endif