
#ifndef ZMQHEARTBEAT_H
#define ZMQHEARTBEAT_H


#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <zmq.hpp>

using json = nlohmann::json;

class ZMQRegister {
public:
    ZMQRegister(const std::string application_name, const std::string uuid, std::string address = "127.0.0.1",
                int port = 5557)
            : application_name(application_name), uuid(uuid), address(address), port(port) {
        json jsonmsg;
        jsonmsg["application"] = application_name;
        jsonmsg["uuid"] = uuid;
        jsonmsg["type"] = "heartbeat";
        msg = jsonmsg.dump();
    };

    ~ZMQRegister() { closeSocket(); }

    // Connect socket
    bool connectSocket(zmq::context_t &context);

    void closeSocket();

    // ping every 2secs
    bool pingOpenEphys();


private:
    int port;
    std::string address;
    std::string application_name;
    std::string uuid;
    zmq::socket_t socket;
    std::string msg;
};


#endif //ZMQHEARTBEAT_H
