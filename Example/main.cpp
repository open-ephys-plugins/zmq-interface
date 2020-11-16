
#include "ZMQReader.h"
#include "ZMQRegister.h"


int main(int argc, char **argv) {

    std::vector<int> channels = {1};
    ZMQReader reader(channels, "127.0.0.1", 5556);
    ZMQRegister heartbeat("example", "11111111", "127.0.0.1", 5557);


    zmq::context_t context;
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();


    if (!heartbeat.connectSocket(context)) {
        std::cout << "Impossible to register the app in OpenEphys." << std::endl;
    }

    if (!reader.connectSocket(context)) {
        std::cout << "Impossible to connect to the OpenEphys GUI." << std::endl;
        exit(1);
    }

    std::cout << "--- Start reading message ---" << std::endl;
    bool runcontext = true;


    while (runcontext) {

        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() > 2) {
            if (heartbeat.pingOpenEphys()) {
                begin = std::chrono::steady_clock::now();
            } else {
                std::cout << "Try to send a heartbeat without success" << std::endl;
            }

        }

        zmq_frames msg;
        if (reader.recv(msg)) {
            std::string type = reader.checkHeader(msg[1]);

            if (type == "data") {
                reader.readData(msg[2]);
            } else if (type == "spike" or type == "event") {
                reader.readEvents(msg);
            }
        }


    }

    reader.closeSocket();
    heartbeat.closeSocket();

    exit(0);
}