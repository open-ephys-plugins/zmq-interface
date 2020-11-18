
#include "ZMQReader.h"
#include "ZMQRegister.h"


int main(int argc, char **argv) {

    ZMQReader reader("127.0.0.1", 5556);
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

        std::deque<std::string> msg;
        if (reader.recv(msg)) {
            std::string type = reader.checkHeader(msg[1]);

            if (type == "data") {
                reader.readData(msg[2]);
            } else if (type == "spike" or type == "event") {
                reader.readEvents(msg[2]);
            }
        }

        // use the data
        std::vector<float> data;
        unsigned intchannel = 1;
        unsigned intn_samples = 10;
        if(reader.get_channel_data(channel, n_samples,  data)){
            std::cout << "Use the " << data.size() << " first samples of the channel 1 here." << std::endl;
        }else{
            std::cout << "Not yet enough data to stream." << std::endl;
        }


    }

    reader.closeSocket();
    heartbeat.closeSocket();

    exit(0);
}