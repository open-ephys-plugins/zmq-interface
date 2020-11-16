
#include "ZMQRegister.h"


bool ZMQRegister::connectSocket(zmq::context_t &context) {
    socket = zmq::socket_t(context, ZMQ_REQ);

    try {
        socket.connect("tcp://" + address + ":" + std::to_string(port));
    } catch (...) {
        std::cout << "Error when creating the socket communication" << std::endl;
    }

    return socket.connected();
}

void ZMQRegister::closeSocket() {
    socket.close();
}


bool ZMQRegister::pingOpenEphys() {
    zmq::message_t message(msg.size());
    std::memcpy(message.data(), msg.data(), msg.size());
    if (zmq_msg_send(reinterpret_cast<zmq_msg_t *>(&message), socket, 0)) {
        zmq::message_t answer;
        if (!zmq_recvmsg(socket,
                         reinterpret_cast<zmq_msg_t *>(&answer),
                         0)) {
            return false;
        } else {
            std::cout << std::string(static_cast<char *>(answer.data()), answer.size()) << std::endl;
            return true;
        }

    }
    return false;

}
