
#ifndef ZMQ_READER_H
#define ZMQ_READER_H

#include <deque>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <zmq.hpp>

typedef std::deque<std::string> zmq_frames;
using json = nlohmann::json;

const int MIN_FRAME_NBR = 2;

bool recv_multi(zmq::socket_t &socket, zmq_frames &frames);

struct Event {
    int sample_num;
    int event_id;
    int event_channel;
    int ts;
    std::vector<uint8_t> *data;
};

class ZMQReader {
public:
    ZMQReader(const std::vector<int> &channel, std::string address = "127.0.0.1",
              int port = 5556);


    ~ZMQReader() { closeSocket(); }

    bool recv(zmq_frames &frames);

    // Connect socket
    bool connectSocket(zmq::context_t &context_);

    void closeSocket();

    /** Read data - format for passing data
     JSON
     {
      ...
      "type": "data",
      "content":
      {
        "n_channels": nChannels,
        "n_samples": nSamples,
        "n_real_samples": nRealSamples,
        "sample_rate": (integer) sampling rate of first channel,
        "timestamp": sample id (number)
      }
    **/
    void readData(std::string data);

    /** Read Events
      format for passing data
     JSON
     {
      ...
      "type":"event",
      "content":
      {
        "type": number,
        "sample_num": sampleNum (number),
        "event_id": id (number),
        "event_channel": channel (number),
        "timestamp": sample id (number)
      }
      ...
     }
     **/

    void readEvents(const zmq_frames &frames);

    /** Read/Check header - missed packets
     *
     * @param header_frame, string
     *
     * format:
     *
     * JSON
     * {
     * "message_no": number,
     * "type":"event",
     * "content":
     * {
     *   ... // specific to the type and parse in the read_type
     * }
     * "data_size": size (if size > 0 it's the size of binary data coming in in
     *the next frame (multi-part message)
     * }
     *
     **/
    std::string checkHeader(const std::string &header_frame);

private:
    json header;
    zmq::socket_t socket_;
    std::string address_;
    int port_;

    std::map<int, std::vector<float> *> samples;
    std::vector<uint64_t> *timestamps;
    std::vector<Event> *spikes;
    std::vector<Event> *events;

    uint count_packet_ = 0;
    uint count_missed_packet_ = 0;
    uint valid_packet_counter_ = 0;
    uint last_message_number = 0;
};


#endif // ZMQ_READER_H
