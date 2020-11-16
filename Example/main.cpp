
#include "ZMQReader.h"

using namespace std;

int main(int argc, char** argv) {
  std::cout <<"--- Connect the reader ---" << std::endl;
  std::vector<int> channels= {1};
  ZMQReader reader(channels,  "127.0.0.1", 5556);
  std::cout <<"--- Connect socket ---" << std::endl;
  zmq::context_t context;

  if(reader.connectSocket(context)){
    std::cout <<"--- Start reading data ---" << std::endl;
    reader.Process();

    reader.closeSocket();
  }

  exit(0);
}