
#include "ZMQReader.h"


void ZMQReader::connectSocket(){
  try {
    zmq::context_t context_ = zmq::context_t(1);
    socket_ = zmq::socket_t(context_, ZMQ_SUB);
    std::string source_id_ = "";
    socket_.setsockopt(ZMQ_SUBSCRIBE, source_id_.c_str(),
                            source_id_.length());
    socket_.connect("tcp://" + address_ + ":" +
                         std::to_string(port_));
  } catch (...) {
    std::cout << "Error when connecting the socket to the address "
              << "tcp://" << address_ << ":" << std::to_string(port_) << std::endl;
  }
}
void ZMQReader::closeSocket(){
  std::cout << "Statistics:  " << std::endl; //TODO Add statistics on the data read
  socket_.close();
}

// Read/Check header - missed packets
void ZMQReader::checkHeader(json header){

  valid_packet_counter_++;

  if (valid_packet_counter_ == 1) { // first packet
    std::cout << "Received first valid data packet"
              << " (TS = " << header["content"]["timestamp"] << ")." << std::endl;
  } else if (last_message_number + 1 !=
             header["message_no"].get<int>()) { // Missed packet
    int nbr_missed_packet = header["message_no"].get<int>() - last_message_number+1;
    std::cout << ". Packet message lost: " << nbr_missed_packet << std::endl;
    count_missed_packet_= count_missed_packet_ + nbr_missed_packet;
  }
  last_message_number = header["message_no"].get<int>();
}

// Read data
bool ZMQReader::readData(zmq_frames frames){
  json header = json::parse(frames[1]);

  // Header:
  // {"content":{"n_channels":16,"n_real_samples":928,"n_samples":1024,"sample_rate":40000,"timestamp":142912},
  // "data_size":65536,"message_no":197705,"type":"data"}

  if(header["type"] != "data"){
    return false;
  }

  checkHeader(header);
  // Data
  int n_samples = header["content"]["n_samples"].get<int>();
  int n_real_samples = header["content"]["n_real_samples"].get<int>();
  int sample_rate = header["content"]["sample_rate"];
  int init_ts = header["content"]["timestamp"].get<int>();

    // copy data onto buffers for each configured channel group
    for (auto &it : samples_) {
      auto start_index = frames[2].begin() + n_samples* it.first;

      it.second->insert(it.second->end(), start_index,
                        start_index + n_real_samples);
    }

  auto ts_increase = [&init_ts, &sample_rate, idx = 0]() mutable {
    ++idx;
    // return (init_ts + idx) * static_cast<uint64_t>(1000000 / sample_rate_); // microseconds timestamps
    return init_ts + idx; // OpenEphys timestamp index
  };

  std::vector<uint64_t> ts(n_real_samples);
  ts.reserve(n_real_samples);
  std::generate_n(ts.begin(), n_real_samples, ts_increase);

  timestamps_.insert(timestamps_.end(), ts.begin(), ts.end());
  return true;
}
// Read Params
bool ZMQReader::readParams(zmq_frames frames){
  json header = json::parse(frames[1]);

  // Header:
  // {"content":{"n_channels":16,"n_real_samples":928,"n_samples":1024,"sample_rate":40000,"timestamp":142912},
  // "data_size":65536,"message_no":197705,"type":"data"}

  if(header["type"] != "data"){
    return false;
  }
}
// Read Events
bool ZMQReader::readEvents(zmq_frames frames){
  json header = json::parse(frames[1]);
  if(header["type"] != "event"){
    return false;
  }


}
// Read Spikes
bool ZMQReader::readSpikes(zmq_frames frames){
  json header = json::parse(frames[1]);
  if(header["type"] != "spike"){
    return false;
  }


}

