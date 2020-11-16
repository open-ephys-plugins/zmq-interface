# ZMQ C++ Example

Minimal project to read data send by  OpenEphys ZMQ node. 

## Dependencies 

see requirement.txt - Install in anaconda or a pip environment 

Dependencies installed through CMake Fetch_Content 
- CPPzmq
- json lib


## Build 

- Activate the environment

```
    mkdir build
    cd build
    cmake ..
    make
    mkdir build
    ./OpenEphysZMQReader
```

## How to use it - Receive data

Turn On Open Ephys GUI with a ZMQ node. 

Input data to modify = 

```
  std::vector<int> channels= {1};
  ZMQReader reader(channels,  "127.0.0.1", 5556);
```
- Ip address =  127.0.0.1 (fixed by OpenEphys for the moment)
- Port address = 5556 (fixed by OpenEphys for the moment)
- channels number to record 

Output data = 

- reader.samples_ (std::map<int, std::vector<float>>) = map channel integer with a std::vector filled with the data send by Open-Ephys GUI
- reader.timestamps_ (std::vector<uint64_t>) = timestamps for each samples

You should then see this type of output in the example project : 

```
--- Connect the reader ---
--- Connect socket ---
--- Start reading data ---
{"content":{"n_channels":16,"n_real_samples":928,"n_samples":1024,"sample_rate":40000,"timestamp":20574688},"data_size":65536,"message_no":159892,"type":"data"}
Received first valid data packet (TS = 20574688).
Data size: 928
{"content":{"n_channels":16,"n_real_samples":928,"n_samples":1024,"sample_rate":40000,"timestamp":20575616},"data_size":65536,"message_no":159893,"type":"data"}
Data size: 1856
...
```

For each received message, n_real_samples are added to the samples_ buffer. 

#### Data message

A data message is send in 3 frames : 
- type
- header in JSON Format: 
    ```
        content:
            n_channels: 16
            n_real_samples: 928
            n_samples: 1024
            sample_rate: 40000
            timestamp: 20576544
        data_size: 65536
        message_no: 159894
        type: data
    ```
- data buffer 

*Example of the buffer architecture with 2 channels* : 

![image](buffer_example.png)

To read a specific channel : 
start index = n_samples * channel_number
stop index = n_samples * channel_number + n_real_samples
    
n_samples is fixed by the ZMQ Open-Ephys node while the n_real_samples is in function of the Open-Ephys source.


#### Event message
TBD

#### Spike message
TBD


## How to register the app in OpenEphys 

Send a heartbeat every 2secs with this JSON message format :

```
app: name_app
uiid: unique number
type: heartbeat
```

Then receive a confirmation message. 