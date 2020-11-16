
#include "ZMQReader.h"
#include <cstdlib>
#include <unistd.h>

void ZMQReader::Process() {
  bool context = true;
  while (context) {
    zmq_frames msg;
    recv_multi(socket_, msg);

    if (msg.size() > 2) {
      std::string type = checkHeader(msg[1]);

      if (type == "data") {
        readData(msg[2]);
        std::cout << "Data size: " << samples_[1]->size() << std::endl;
      } else if (type == "event") {
        readEvents(msg);
        std::cout << "Read events" << std::endl;
      } else if (type == "spike") {
        readSpikes(msg);
        std::cout << "Read spikes" << std::endl;
      }
    } else {
      sleep(1);
    }
  }
}

bool ZMQReader::connectSocket(zmq::context_t& context_) {

  try{
    socket_ = zmq::socket_t(context_, ZMQ_SUB);
    //std::string source_id_;
    socket_.connect("tcp://" + address_ + ":" + std::to_string(port_));
    std::string source_id_;
    socket_.setsockopt(ZMQ_SUBSCRIBE, source_id_.c_str(), source_id_.length());
  } catch (...) {
    std::cout << "Error when creating the socket communication" << std::endl;
  }


  return socket_.connected();
}
void ZMQReader::closeSocket() {
  std::cout << "Statistics:\n  " << count_packet_ << " packets received and "
            << count_missed_packet_ << " packet missed."
            << std::endl; // TODO Add statistics on the data read
  socket_.close();
}

// Read/Check header - missed packets
std::string ZMQReader::checkHeader(const std::string& header_frame) {
  header = json::parse(header_frame);

  std::cout << header << std::endl;
  valid_packet_counter_++;

  if (valid_packet_counter_ == 1) { // first packet
    std::cout << "Received first valid data packet"
              << " (TS = " << header["content"]["timestamp"] << ")."
              << std::endl;
  } else if (last_message_number + 1 !=
             header["message_no"].get<int>()) { // Missed packet
    int nbr_missed_packet =
        header["message_no"].get<int>() - last_message_number + 1;
    std::cout << ". Packet message lost: " << nbr_missed_packet << std::endl;
    count_missed_packet_ = count_missed_packet_ + nbr_missed_packet;
  }
  last_message_number = header["message_no"].get<int>();

  return header["type"];
}

// Read data
bool ZMQReader::readData(std::string data) {

  if (header["type"] != "data") {
    return false;
  }

  // Data
  int n_samples = header["content"]["n_samples"].get<int>();
  int n_real_samples = header["content"]["n_real_samples"].get<int>();
  int sample_rate = header["content"]["sample_rate"];
  int init_ts = header["content"]["timestamp"].get<int>();

  // copy data onto buffers for each configured channel group
  for (auto &it : samples_) {
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

  timestamps_->insert(timestamps_->end(), ts.begin(), ts.end());
  return true;
}

// Read Events
bool ZMQReader::readEvents(const zmq_frames& frames) {
  if (header["type"] != "event") {
    return false;
  }
  return true;
}
// Read Spikes
bool ZMQReader::readSpikes(const zmq_frames& frames) {
  if (header["type"] != "spike") {
    return false;
  }

  return true;
}

bool recv_multi(zmq::socket_t &socket, zmq_frames &frames) {
  zmq::message_t message;

  int64_t rcvmore = 0;
  size_t type_size = sizeof(int64_t);

  do {
    if (!zmq_recvmsg(socket, reinterpret_cast<zmq_msg_t *>(&message),
                     ZMQ_NOBLOCK)) {
      return false;
    }

    frames.push_back(
        std::string(static_cast<char *>(message.data()), message.size()));
    socket.getsockopt(ZMQ_RCVMORE, &rcvmore, &type_size);
  } while (rcvmore != 0);

  return true;
}