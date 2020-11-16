
#ifndef ZMQ_READER_H
#define ZMQ_READER_H


#include <iostream>
#include <nlohmann/json.hpp>
#include <deque>
#include <string>
#include <zmq.hpp>

typedef std::deque<std::string> zmq_frames;
using json = nlohmann::json;

bool recv_multi(zmq::socket_t &socket, zmq_frames &frames);

class ZMQReader {
public:

  ZMQReader(const std::vector<int> channel, std::string address="127.0.0.1", int port= 5556): port_(port), address_(address){
    count_missed_packet_ = 0;
    count_packet_ = 0;
    for (auto &chan : channel) {
      samples_[chan] = new std::vector<float>();
    }
    timestamps_ = new std::vector<uint64_t>();

  }

  ~ZMQReader(){closeSocket();}

  // Connect + parse message = put in a thread
  void Process();

  // Connect socket
  bool connectSocket(zmq::context_t& context_);
  void closeSocket();

  // Read data
  bool readData(std::string data);
  // Read Params
  bool readParams(zmq_frames frames);
  // Read Events
  bool readEvents(const zmq_frames& frames);
  // Read Spikes
  bool readSpikes(const zmq_frames& frames);

  // Read/Check header - missed packets
  std::string checkHeader(const std::string& header_frame);

  zmq::socket_t socket_;
  std::string address_= "127.0.0.1";
  int port_=5556;
private:
  json header;
  std::map<int, std::vector<float> *> samples_;
  std::vector<uint64_t>* timestamps_;

  int count_packet_;
  int count_missed_packet_;
  int valid_packet_counter_ = 0;
  int last_message_number;
};

class ZMQRegister{

  ZMQRegister(std::string application_name, std::string uuid, int port=5557): port_(port){
  }
  void Process();
  // register the app in open-ephys

  // Connect socket
  void connectSocket(zmq::context_t context);
  void closeSocket();

  // ping every 2secs
  void pingOpenEphys();

  // send event to openEphys

private:
  int port_;

};


#endif // ZMQ_READER_H
