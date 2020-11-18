
#ifndef ZMQ_READER_H
#define ZMQ_READER_H

#include <deque>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <zmq.hpp>


using json = nlohmann::json;

const int MIN_FRAME_NBR = 2;

struct Event {
    uint sample_num;
    uint event_id;
    uint event_channel;
    uint ts;
    std::vector<uint8_t> *data;
};

class ZMQReader {
public:
    /** Initiate the class and create the data structure based on the target channel **/
    ZMQReader(std::string address = "127.0.0.1", int port = 5556) :
            port(port),
            address(address) {};


    ~ZMQReader() { closeSocket(); }

    /** Wait to receive multiframes messages **/
    bool recv(std::deque<std::string> &frames);

    /** Connect the socket as a Subscriber to the communication **/
    bool connectSocket(zmq::context_t &context_);

    void closeSocket();

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

    /** Read data - format for passing data
     *
     * @param, data, string
     *
     * specific header format:
     *
     * JSON
     * {
     * ...
     * "type": "data",
     * "content":
     * {
     *   "n_channels": nChannels,
     *   "n_samples": nSamples,
     *   "n_real_samples": nRealSamples,
     *   "sample_rate": (integer) sampling rate of first channel,
     *   "timestamp": sample id (number)
     * }
    **/
    void readData(std::string &data);

    /** Read Events
     *
     * @param data, string
     *
     * specific header format:
     *
     * JSON
     * {
     * ...
     * "type":"event",
     * "content":
     * {
     *   "type": number,
     *   "sample_num": sampleNum (number),
     *   "event_id": id (number),
     *   "event_channel": channel (number),
     *   "timestamp": sample id (number)
     * }
     * ...
     * }
     **/

    void readEvents(const std::string &data);

    /** get from the buffer n_sample from the specified channel **/
    bool get_channel_data(uint channel, uint n_sample, std::vector<float> &data);

    /** get the n last events **/
    bool get_events(uint n_events, std::vector<Event> &event);

    /** get the n last spikes **/
    bool get_spikes(uint n_spikes, std::vector<Event> &spikes);

private:
    json header;
    zmq::socket_t socket;
    std::string address;
    int port;

    std::map<int, std::vector<float> *> samples;
    std::vector<uint64_t> *timestamps;
    std::vector<Event> *spikes;
    std::vector<Event> *events;

    uint count_packet = 0;
    uint count_missed_packet = 0;
    uint valid_packet_counter = 0;
    uint last_message_number = 0;
};


#endif // ZMQ_READER_H
