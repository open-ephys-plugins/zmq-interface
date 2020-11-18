
#ifndef ZMQHEARTBEAT_H
#define ZMQHEARTBEAT_H


#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <zmq.hpp>

using json = nlohmann::json;

class ZMQRegister {
public:
    /** Initiate the class and create the heartbeat message to be sent -
     *
     * @param application_name
     * @param uuid string, unique identifier for the app
     * @param address, fixed by OpenEphys for the moment
     * @param port, fixed by OpenEphys for the moment
     */
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

    /** Connect to the socket as REQ/REP communication **/
    bool connectSocket(zmq::context_t &context);

    void closeSocket();

    /** Send the heartbeat message and wait for an answer from OpenEphys **/
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
