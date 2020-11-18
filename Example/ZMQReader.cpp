
#include "ZMQReader.h"
#include <cstdlib>
#include <unistd.h>


bool ZMQReader::recv(std::deque<std::string> &frames) {

    zmq::message_t message;
    int64_t rcvmore = 0;
    size_t type_size = sizeof(int64_t);

    do {
        zmq_msg_recv(reinterpret_cast<zmq_msg_t *>(&message), socket, 0);
        frames.push_back(std::string(static_cast<char *>(message.data()),
                                        message.size()));

        socket.getsockopt(ZMQ_RCVMORE, &rcvmore, &type_size);
    } while (rcvmore != 0);

    if (frames.size() > MIN_FRAME_NBR) {
        return true;
    }

    return false;
}

bool ZMQReader::connectSocket(zmq::context_t &context_) {
    socket = zmq::socket_t(context_, ZMQ_SUB);

    try {
        socket.connect("tcp://" + address + ":" + std::to_string(port));
    } catch (...) {
        std::cout << "Error when creating the socket communication" << std::endl;
    }

    std::string source_id_;
    socket.setsockopt(ZMQ_SUBSCRIBE, source_id_.c_str(), source_id_.length());

    return socket.connected();
}

void ZMQReader::closeSocket() {
    std::cout << "Statistics:\n  " << count_packet << " packets received and "
              << count_missed_packet << " packet missed."
              << std::endl;
    socket.close();
}

// Read/Check header - missed packets
std::string ZMQReader::checkHeader(const std::string &header_frame) {
    header = json::parse(header_frame);

    std::cout << "New message: " <<  header << std::endl;

    valid_packet_counter++;

    if (valid_packet_counter == 1) { // first packet
        std::cout << "Received first valid packet"
                  << " (type = " << header["type"] << "TS = " << header["content"]["timestamp"] << ")."
                  << std::endl;

    } else if (last_message_number + 1 !=
               header["message_no"].get<uint>()) { // Missed packet
        uint nbr_missed_packet =
                header["message_no"].get<uint>() - last_message_number + 1;
        std::cout << ". Packet message lost: " << nbr_missed_packet << std::endl;
        count_missed_packet = count_missed_packet + nbr_missed_packet;
    }
    last_message_number = header["message_no"].get<int>();

    return header["type"];
}


void ZMQReader::readData(std::string &data) {


    // First data packet - initiate the structure
    if(samples.empty()){
        for (int chan=0; chan<header["content"]["n_channels"].get<uint>(); chan++) {
            samples[chan] = new std::vector<float>();
        }
        timestamps = new std::vector<uint64_t>();
    }

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

void ZMQReader::readEvents(const std::string &data) {
    Event event;
    // MetaData
    event.sample_num = header["content"]["sample_num"].get<uint>();
    event.event_id = header["content"]["event_id"].get<uint>();
    event.event_channel = header["content"]["event_channel"].get<uint>();
    event.ts = header["content"]["timestamp"].get<uint>();

    copy(data.begin(), data.end(), event.data->begin());

    if (header["type"] == "spike") {
        spikes->push_back(event);

    } else if (header["type"] == "event") {
        events->push_back(event);
    }
}

bool ZMQReader::get_channel_data(uint channel, uint n_samples, std::vector<float> &data){
    if(samples[channel]->size() < n_samples){
        return false;
    }
    auto start = samples[channel]->begin();
    data.insert(data.end(), start, start+ n_samples);
    samples[channel]->erase(start, start + n_samples);
    return true;
}
bool ZMQReader::get_events(uint n_events, std::vector<Event> &event){
    if(events->size() < n_events){
       return false;
    }
    auto start = events->begin();
    event.insert(event.begin(), start, start + n_events);
    return true;
}
bool ZMQReader::get_spikes(uint n_spikes, std::vector<Event> &spike){
    if(spikes->size() < n_spikes){
        return false;
    }
    auto start = spikes->begin();
    spike.insert(spike.begin(), start, start + n_spikes);
    return true;
}