
#ifndef ZMQ_READER_H
#define ZMQ_READER_H

#include <deque>
#include <string>
#include <zmq.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
typedef std::deque<std::string> zmq_frames;

class ZMQReader {
public:
  ZMQReader(std::string address= "127.0.0.1", int port=5556): port_(port), address_(address){
    connectSocket();
    Process();
  }

  ~ZMQReader(){closeSocket();}

  // Connect + parse message = put in a thread
  void Process();

  // Connect socket
  void connectSocket();
  void closeSocket();

  // Read data
  bool readData(zmq_frames frames);
  // Read Params
  bool readParams(zmq_frames frames);
  // Read Events
  bool readEvents(zmq_frames frames);
  // Read Spikes
  bool readSpikes(zmq_frames frames);

  // Read/Check header - missed packets
  void checkHeader(json header);

private:
  int port_;
  std::string address_;
  zmq::socket_t socket_;

  std::map<int, std::vector<float> *> samples_;
  std::deque<uint64_t> timestamps_;

  int count_packet_;
  int count_missed_packet_;
  int valid_packet_counter_ = 0;
  int last_message_number;
};

class ZMQRegister{

  ZMQRegister(std::string application_name, std::string uuid, int port=5557): port_(port){
    connectSocket();
    Process();
  }
  void Process();
  // register the app in open-ephys

  // Connect socket
  void connectSocket();
  void closeSocket();

  // ping every 2secs
  void pingOpenEphys();

  // send event to openEphys

private:
  int port_;

};

#endif // ZMQ_READER_H
