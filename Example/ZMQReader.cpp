
#include "ZMQReader.h"
#include <cstdlib>
#include <unistd.h>


ZMQReader::ZMQReader(const std::vector<int> &channel, std::string address, int port)
        : port_(port),
          address_(address) {

    for (auto &chan : channel) {
        samples[chan] = new std::vector<float>();
    }
    timestamps = new std::vector<uint64_t>();
}

bool ZMQReader::recv(zmq_frames &frames) {

    zmq::message_t message;
    int64_t rcvmore = 0;
    size_t type_size = sizeof(int64_t);

    do {
        if (!zmq_recvmsg(socket_,
                         reinterpret_cast<zmq_msg_t *>(&message),
                         0)) {
            return false;
        }

        frames.push_back(std::string(static_cast<char *>(message.data()),
                                        message.size()));

        socket_.getsockopt(ZMQ_RCVMORE, &rcvmore, &type_size);
    } while (rcvmore != 0);

    if (frames.size() > MIN_FRAME_NBR) {
        return true;
    }

    return false;
}

bool ZMQReader::connectSocket(zmq::context_t &context_) {
    socket_ = zmq::socket_t(context_, ZMQ_SUB);

    try {
        socket_.connect("tcp://" + address_ + ":" + std::to_string(port_));
    } catch (...) {
        std::cout << "Error when creating the socket communication" << std::endl;
    }

    std::string source_id_;
    socket_.setsockopt(ZMQ_SUBSCRIBE, source_id_.c_str(), source_id_.length());

    return socket_.connected();
}

void ZMQReader::closeSocket() {
    std::cout << "Statistics:\n  " << count_packet_ << " packets received and "
              << count_missed_packet_ << " packet missed."
              << std::endl;
    socket_.close();
}

// Read/Check header - missed packets
std::string ZMQReader::checkHeader(const std::string &header_frame) {
    header = json::parse(header_frame);

    std::cout << "New message: " <<  header << std::endl;

    valid_packet_counter_++;

    if (valid_packet_counter_ == 1) { // first packet
        std::cout << "Received first valid data packet"
                  << " (TS = " << header["content"]["timestamp"] << ")."
                  << std::endl;
    } else if (last_message_number + 1 !=
               header["message_no"].get<uint>()) { // Missed packet
        uint nbr_missed_packet =
                header["message_no"].get<uint>() - last_message_number + 1;
        std::cout << ". Packet message lost: " << nbr_missed_packet << std::endl;
        count_missed_packet_ = count_missed_packet_ + nbr_missed_packet;
    }
    last_message_number = header["message_no"].get<int>();

    return header["type"];
}

// Read data
void ZMQReader::readData(std::string data) {

    // Data
    uint n_samples = header["content"]["n_samples"].get<uint>();
    uint n_real_samples = header["content"]["n_real_samples"].get<uint>();
    uint sample_rate = header["content"]["sample_rate"].get<uint>();
    uint init_ts = header["content"]["timestamp"].get<uint>();

    // copy data onto buffers for each configured channel group
    for (auto &it : samples) {
        auto start_index = data.begin() + n_samples * it.first;

        it.second->insert(it.second->end(), start_index,
                          start_index + n_real_samples);
    }

    auto ts_increase = [&init_ts, &sample_rate, idx = 0]() mutable {
        ++idx;
        // return (init_ts + idx) * static_cast<uint64_t>(1000000 / sample_rate_);
        // // microseconds timestamps
        return init_ts + idx; // OpenEphys timestamp index
    };

    std::vector<uint64_t> ts(n_real_samples);
    ts.reserve(n_real_samples);
    std::generate_n(ts.begin(), n_real_samples, ts_increase);

    timestamps->insert(timestamps->end(), ts.begin(), ts.end());

    std::cout  << timestamps->size() << " samples has been collected in the buffer for future processing." << std::endl;

}


// Read Events
void ZMQReader::readEvents(const zmq_frames &frames) {
    Event event;
    // MetaData
    event.sample_num = header["content"]["sample_num"].get<int>();
    event.event_id = header["content"]["event_id"].get<int>();
    event.event_channel = header["content"]["event_channel"];
    event.ts = header["content"]["timestamp"].get<int>();

    copy(frames[1].begin(), frames[1].end(), event.data->begin());

    if (header["type"] == "spike") {
        spikes->push_back(event);

    } else if (header["type"] == "event") {
        events->push_back(event);
    }
}

