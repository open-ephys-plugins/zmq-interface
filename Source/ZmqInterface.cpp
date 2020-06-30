/*
 ------------------------------------------------------------------
 
 ZMQInterface
 Copyright (C) 2016 FP Battaglia
 Copyright (C) 2017-2019 Andras Szell
 
 based on
 Open Ephys GUI
 Copyright (C) 2013, 2015 Open Ephys
 
 ------------------------------------------------------------------
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 */
/* 
  ==============================================================================

    ZmqInterface.cpp
    Created: 19 Sep 2015 9:47:12pm
    Author:  Francesco Battaglia
    Updated by: Andras Szell

  ==============================================================================
*/

#include <zmq.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <errno.h>
#include "ZmqInterface.h"
#include "ZmqInterfaceEditor.h"

#define DEBUG_ZMQ
const int MAX_MESSAGE_LENGTH = 64000;

struct EventData {
    uint8 type;
    uint8 eventId;
    uint8 eventChannel;
    uint8 numBytes;
    int sampleNum;
    time_t eventTime;
    char application[256];
    char uuid[256];
    bool isEvent;
};


ZmqInterface::ZmqInterface(const String& processorName)
    : GenericProcessor(processorName), Thread("Zmq thread")
{
    context = 0;
    socket = 0;
    listenSocket = 0;
    controlSocket = 0;
    killSocket = 0;
    pipeInSocket = 0;
    pipeOutSocket = 0;
    
    flag = 0;
    messageNumber = 0;
    dataPort = 5556; //TODO make this editable
    listenPort = 5557;
    
    setProcessorType(PROCESSOR_TYPE_FILTER);

    createContext();
    threadRunning = false;
    openListenSocket();
    openKillSocket();
    
    // TODO mock implementation
}

ZmqInterface::~ZmqInterface()
{
    threadRunning = false;
    // send the kill signal to the thread
    
    zmq_msg_t messageEnvelope;
    zmq_msg_init_size(&messageEnvelope, strlen("STOP")+1);
    memcpy(zmq_msg_data(&messageEnvelope), "STOP", strlen("STOP")+1);
    zmq_msg_send(&messageEnvelope, killSocket, 0);
    zmq_msg_close(&messageEnvelope);
    
    stopThread(200); // this is probably overkill but won't hurt
    closeDataSocket();
    zmq_close(killSocket);
    zmq_close(pipeOutSocket);
    
    sleep(500);
    
    if (context)
    {
        zmq_ctx_destroy(context);
        context = 0;
    }
}

OwnedArray<ZmqApplication> *ZmqInterface::getApplicationList()
{
    return &applications;
}

int ZmqInterface::createContext()
{
    context = zmq_ctx_new();
    if (!context)
        return -1;
    return 0;
}

int ZmqInterface::createDataSocket()
{
    if (!socket)
    {
        socket = zmq_socket(context, ZMQ_PUB);
        if (!socket)
            return -1;
        String urlstring;
        urlstring = String("tcp://*:") + String(dataPort);
        std::cout << urlstring << std::endl;
        int rc = zmq_bind(socket, urlstring.toRawUTF8());
        if (rc)
        {
            std::cout << "couldn't open data socket" << std::endl;
            std::cout << zmq_strerror(zmq_errno()) << std::endl;
            jassert(false);
        }
    }
    return 0;
}

int ZmqInterface::closeDataSocket()
{
    if (socket)
    {
        std::cout << "close data socket" << std::endl;
        int rc = zmq_close(socket);
        jassert(rc==0);
        socket = 0;
    }
    return 0;
}

void ZmqInterface::openListenSocket()
{
    startThread();
}

int ZmqInterface::closeListenSocket()
{
    int rc = 0;
    if (listenSocket)
    {
        std::cout << "close listen socket" << std::endl;
        rc = zmq_close(listenSocket);
        listenSocket = 0;
    }
    
    return rc;
}

void ZmqInterface::openKillSocket()
{
    killSocket = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(killSocket, "inproc://zmqthreadcontrol");
}

void ZmqInterface::openPipeOutSocket()
{
    pipeOutSocket = zmq_socket(context, ZMQ_PAIR);
    zmq_bind(pipeOutSocket, "inproc://zmqthreadpipe");
}

void ZmqInterface::run()
{
    //OutputDebugStringW(L"ZMQ plugin starting.\n");
    std::cout << "ZMQ plugin starting." << std::endl;
    listenSocket = zmq_socket(context, ZMQ_REP);
    String urlstring;
    urlstring = String("tcp://*:") + String(listenPort);
    int rc = zmq_bind(listenSocket, urlstring.toRawUTF8()); // give the chance to change the port
    jassert(rc == 0);
    threadRunning = true;
    char* buffer = new char[MAX_MESSAGE_LENGTH];

    int size;

    controlSocket = zmq_socket(context, ZMQ_PAIR);
    zmq_bind(controlSocket, "inproc://zmqthreadcontrol");
    
    pipeInSocket = zmq_socket(context, ZMQ_PAIR);
    zmq_connect(pipeInSocket, "inproc://zmqthreadpipe");
    
    zmq_pollitem_t items [] = {
        { listenSocket, 0, ZMQ_POLLIN, 0 },
        { controlSocket, 0, ZMQ_POLLIN, 0 }
    };
    

    while (threadRunning && (!threadShouldExit()))
    {
        zmq_poll (items, 2, -1);
        if (items[0].revents & ZMQ_POLLIN)
        {
            size = zmq_recv(listenSocket, buffer, MAX_MESSAGE_LENGTH-1, 0);
            buffer[size] = 0;
            
            if (size < 0)
            {
                std::cout << "failed in receiving listen socket" << std::endl;
                std::cout << zmq_strerror(zmq_errno()) << std::endl;
                jassert(false);
            }
            var v;
#ifdef ZMQ_DEBUG
            std::cout << "in listening thread: " << String(buffer) << std::endl;
#endif
            Result rs = JSON::parse(String(buffer), v);
            bool ok = rs.wasOk();

            EventData ed;
            String app = v["application"];
            String appUuid = v["uuid"];
            strncpy(ed.application, app.toRawUTF8(),  255);
            strncpy(ed.uuid, appUuid.toRawUTF8(), 255);
            ed.eventTime = time(NULL);
            
            String evT = v["type"];
            
            if (evT == "event")
            {
                ed.isEvent = true;
                ed.eventChannel = (int)v["event"]["event_channel"];
                ed.eventId = (int)v["event"]["event_id"];
                ed.numBytes = 0; // TODO  allow for event data
                ed.sampleNum = (int)v["event"]["sample_num"];
                ed.type = (int)v["event"]["type"];
//                std::cout << "chan " << (int)ed.eventChannel << " id " << (int)ed.eventId << " sample n "
//                    << (int)ed.sampleNum << " type " << (int)ed.type <<
//                    std::endl;
            }
            else
            {
                ed.isEvent = false;
            }
            
            // TODO allow for event data
            zmq_msg_t message;
            zmq_msg_init_size(&message, sizeof(EventData));
            memcpy(zmq_msg_data(&message), &ed, sizeof(EventData));
            int size_m = zmq_msg_send(&message, pipeInSocket, 0);
            jassert(size_m);
            size += size_m;
            zmq_msg_close(&message);
            
            // send response
            String response;
            if (ok)
            {
                if (ed.isEvent)
                {
                    response = String("message correctly parsed");
                }
                else
                {
                    response = String("heartbeat received");
                }
            }
            else
            {
                response = String("JSON message could not be read");
            }
            zmq_send(listenSocket, response.getCharPointer(), response.length(), 0);
            if ((!threadRunning) || threadShouldExit())
                break; // we're exiting

        }
        else
            break;
        
    }
    closeListenSocket();
    
    zmq_close(pipeInSocket);
    zmq_close(controlSocket);
    delete[] buffer;
    threadRunning = false;
    return;
}

/* format for passing data
 JSON
 { 
  "message_no": number,
  "type": "data"|"event"|"parameter",
  "content":
  (for data)
  {
    "n_channels": nChannels,
    "n_samples": nSamples,
    "n_real_samples": nRealSamples,
    "sample_rate": (integer) sampling rate of first channel,
    "timestamp": sample id (number)
  }
  (for event)
  {
    "type": number,
    "sample_num": sampleNum (number),
    "event_id": id (number),
    "event_channel": channel (number),
    "timestamp": sample id (number)
  }
  (for parameter) // todo
  {
    "param_name1": param_value1,
    "param_name2": param_value2,
  }
  "data_size": size (if size > 0 it's the size of binary data coming in in the next frame (multi-part message)
 }
 
 and then a possible data packet
 */


int ZmqInterface::sendData(float *data, int nChannels, int nSamples, int nRealSamples, 
                           int64 timestamp, int sampleRate)
{
    
    messageNumber++;
    

    DynamicObject::Ptr obj = new DynamicObject();
    
    int mn = messageNumber;
    obj->setProperty("message_no", mn);
    obj->setProperty("type", "data");
    
    DynamicObject::Ptr c_obj = new DynamicObject();
    
    c_obj->setProperty("n_channels", nChannels);
    c_obj->setProperty("n_samples", nSamples);
    c_obj->setProperty("n_real_samples", nRealSamples);
    c_obj->setProperty("timestamp", timestamp);
    c_obj->setProperty("sample_rate", sampleRate);

    obj->setProperty("content", var(c_obj));
    obj->setProperty("data_size", (int)(nChannels * nSamples * sizeof(float)));
    
    var json(obj);
    
    String s = JSON::toString(json);
    void *headerData = (void *)s.toRawUTF8();
    

    size_t headerSize = s.length();
    
    zmq_msg_t messageEnvelope;
    zmq_msg_init_size(&messageEnvelope, strlen("DATA")+1);
    memcpy(zmq_msg_data(&messageEnvelope), "DATA", strlen("DATA")+1);
    int size = zmq_msg_send(&messageEnvelope, socket, ZMQ_SNDMORE);
    jassert(size != -1);
    zmq_msg_close(&messageEnvelope);
    
    zmq_msg_t messageHeader;
    zmq_msg_init_size(&messageHeader, headerSize);
    memcpy(zmq_msg_data(&messageHeader), headerData, headerSize);
    size = zmq_msg_send(&messageHeader, socket, ZMQ_SNDMORE);
    jassert(size != -1);
    zmq_msg_close(&messageHeader);
    
    zmq_msg_t message;
    zmq_msg_init_size(&message, sizeof(float)*nSamples*nChannels);
    memcpy(zmq_msg_data(&message), data, sizeof(float)*nSamples*nChannels);
    int size_m = zmq_msg_send(&message, socket, 0);
    jassert(size_m);
    size += size_m;
    zmq_msg_close(&message);
 
    return size;
}

// todo
int ZmqInterface::sendSpikeEvent(const SpikeChannel* spikeInfo, const MidiMessage& event)
{
    messageNumber++;
    int size = 0;

    const uint8_t* dataptr = event.getRawData();
    int bufferSize = event.getRawDataSize();
    if (bufferSize)
    {
        SpikeEventPtr spike = SpikeEvent::deserializeFromMessage(event, spikeInfo);

        if (spike)
        {
            DynamicObject::Ptr obj = new DynamicObject();
            obj->setProperty("message_no", messageNumber);
            obj->setProperty("type", "spike");
            DynamicObject::Ptr c_obj = new DynamicObject();
            c_obj->setProperty("timestamp", (int64)spike->getTimestamp());
            //c_obj->setProperty("timestamp_software",
            //                 (int64)spike.timestamp_software);
            const SpikeChannel* channel = spike->getChannelInfo();
            uint32_t nChannels = channel->getNumChannels();
            uint32_t nSamples = channel->getTotalSamples();
            c_obj->setProperty("n_channels", (int64)nChannels);
            c_obj->setProperty("n_samples", (int64)nSamples);
    
            // todo
            c_obj->setProperty("electrode_id", getSpikeChannelIndex(spike));
#if 0
            c_obj->setProperty("channel", spike.channel);
            c_obj->setProperty("source", spike.source);
            var c_var(spike.color[0]);
            c_var.append(spike.color[1]);
            c_var.append(spike.color[2]);
            c_obj->setProperty("color", c_var);
            var p_var(spike.pcProj[0]);
            p_var.append(spike.pcProj[1]);
            c_obj->setProperty("pc_proj", p_var);
            var g_var(spike.gain[0]);
            for (int i = 1; i < spike.nChannels; i++)
                g_var.append(spike.gain[i]);
            c_obj->setProperty("gain", g_var);
            var t_var = var(spike.threshold[0]);
            for (int i = 1; i < spike.nChannels; i++)
                t_var.append(spike.threshold[i]);
            c_obj->setProperty("threshold", t_var);
#endif
            obj->setProperty("spike", var(c_obj));
            var json (obj);
            String s = JSON::toString(json);
            void *headerData = (void *)s.toRawUTF8();
            size_t headerSize = s.length();
            
            
            zmq_msg_t messageEnvelope;
            zmq_msg_init_size(&messageEnvelope, strlen("EVENT")+1);
            memcpy(zmq_msg_data(&messageEnvelope), "EVENT", strlen("EVENT")+1);
            size = zmq_msg_send(&messageEnvelope, socket, ZMQ_SNDMORE);
            jassert(size != -1);
            zmq_msg_close(&messageEnvelope);

            zmq_msg_t messageHeader;
            zmq_msg_init_size(&messageHeader, headerSize);
            memcpy(zmq_msg_data(&messageHeader), headerData, headerSize);
            size = zmq_msg_send(&messageHeader, socket, ZMQ_SNDMORE);
            jassert(size != -1);
            zmq_msg_close(&messageHeader);
            zmq_msg_t message;
            zmq_msg_init_size(&message, nChannels * nSamples);
            // getdatapointer???
            memcpy(zmq_msg_data(&message), spike->getDataPointer(), nChannels * nSamples);
            size = zmq_msg_send(&message, socket, 0);
            
        }
    }
    return 0;
}

int ZmqInterface::sendEvent( uint8 type,
                             int sampleNum,
                             uint8 eventId,
                             uint8 eventChannel,
                             uint8 numBytes,
                             const uint8* eventData,
                             int64 timestamp)
{
    int size;
    
    messageNumber++;
    
    DynamicObject::Ptr obj = new DynamicObject();
    
    obj->setProperty("message_no", messageNumber);
    obj->setProperty("type", "event");
    
    DynamicObject::Ptr c_obj = new DynamicObject();
    c_obj->setProperty("type", type);
    c_obj->setProperty("sample_num", sampleNum);
    c_obj->setProperty("event_id", eventId);
    c_obj->setProperty("event_channel", eventChannel);
    c_obj->setProperty("timestamp", timestamp);

    obj->setProperty("content", var(c_obj));
    obj->setProperty("data_size", numBytes);
    
    var json (obj);
    String s = JSON::toString(json);
    void *headerData = (void *)s.toRawUTF8();
    size_t headerSize = s.length();
    
    
    zmq_msg_t messageEnvelope;
    zmq_msg_init_size(&messageEnvelope, strlen("EVENT")+1);
    memcpy(zmq_msg_data(&messageEnvelope), "EVENT", strlen("EVENT")+1);
    size = zmq_msg_send(&messageEnvelope, socket, ZMQ_SNDMORE);
    jassert(size != -1);
    zmq_msg_close(&messageEnvelope);
    
    zmq_msg_t messageHeader;
    zmq_msg_init_size(&messageHeader, headerSize);
    memcpy(zmq_msg_data(&messageHeader), headerData, headerSize);
    if (numBytes == 0)
    {
        size = zmq_msg_send(&messageHeader, socket, 0);
        jassert(size != -1);
        zmq_msg_close(&messageHeader);
    }
    else
    {
        size = zmq_msg_send(&messageHeader, socket, ZMQ_SNDMORE);
        jassert(size != -1);
        zmq_msg_close(&messageHeader);
        zmq_msg_t message;
        zmq_msg_init_size(&message, numBytes);
        memcpy(zmq_msg_data(&message), eventData, numBytes);
        int size_m = zmq_msg_send(&message, socket, 0);
        jassert(size_m);
        size += size_m;
        zmq_msg_close(&message);
    }
    return size;
}

template<typename T> int ZmqInterface::sendParam(String name, T value)
{
    int size;
    
    messageNumber++;
    
//    MemoryOutputStream jsonHeader;
//    jsonHeader << "{ \"messageNo\": " << messageNumber << "," << newLine;
//    jsonHeader << "  \"type\": \"param\"," << newLine;
//    jsonHeader << " \"content\": " << newLine;
//    jsonHeader << " { \"" << name << "\": " << value  << newLine;
//    jsonHeader << "}," << newLine;
//    jsonHeader << " \"dataSize\": " << 0 << newLine;
//    jsonHeader << "}";
    
    
    DynamicObject::Ptr obj = new DynamicObject();
    
    obj->setProperty("message_no", messageNumber);
    obj->setProperty("type", "event");
    DynamicObject::Ptr c_obj = new DynamicObject();
    c_obj->setProperty(name, value);
    
    obj->setProperty("content", var(c_obj));
    obj->setProperty("data_size", 0);
    
    var json (obj);
    String s = JSON::toString(json);
    void *headerData = (void *)s.toRawUTF8();
    size_t headerSize = s.length();
    
    zmq_msg_t messageEnvelope;
    zmq_msg_init_size(&messageEnvelope, strlen("PARAM")+1);
    memcpy(zmq_msg_data(&messageEnvelope), "PARAM", strlen("PARAM")+1);
    size = zmq_msg_send(&messageEnvelope, socket, ZMQ_SNDMORE);
    jassert(size != -1);
    zmq_msg_close(&messageEnvelope);
    
    
    zmq_msg_t messageHeader;
    zmq_msg_init_size(&messageHeader, headerSize);
    memcpy(zmq_msg_data(&messageHeader), headerData, headerSize);
    size = zmq_msg_send(&messageHeader, socket, 0);
    jassert(size != -1);
    zmq_msg_close(&messageHeader);
    
    return size;
}


AudioProcessorEditor* ZmqInterface::createEditor()
{
    
    //        std::cout << "in PythonEditor::createEditor()" << std::endl;
    editor = new ZmqInterfaceEditor(this, true);
    return editor;
}

bool ZmqInterface::isReady()
{
    return true;
}

void ZmqInterface::setParameter(int parameterIndex, float newValue)
{
    editor->updateParameterButtons(parameterIndex);
    
    //Parameter& p =  parameters.getReference(parameterIndex);
    //p.setValue(newValue, 0);
    
    //threshold = newValue;
    
    //std::cout << float(p[0]) << std::endl;
    editor->updateParameterButtons(parameterIndex);
}

void ZmqInterface::resetConnections()
{
    nextAvailableChannel = 0;
    
    wasConnected = false;

    return;
}

void ZmqInterface::handleEvent(const EventChannel* eventInfo, const MidiMessage& event, int samplePosition)
{
    //std::cout << "Event handling, type: " << Event::getEventType(event) << "\n" << std::flush;

    if (Event::getEventType(event) == EventChannel::TTL)
    {
        TTLEventPtr ttl = TTLEvent::deserializeFromMessage(event, eventInfo);

        const uint8* dataptr = event.getRawData();
        int size = event.getRawDataSize();
        //std::cout << "event data size " << size << std::endl;
        uint8 numBytes;
        if (size > 6)
            numBytes = size - 6;
        else
            numBytes = 0;


        //int eventNodeId = *(dataptr+1);
        const int eventId = ttl->getState() ? 1 : 0;
        const int eventChannel = ttl->getChannel();
        //const int eventTime = samplePosition;
        //std::cout << "ZMQ TTL timestamp " << event.getTimeStamp() << " samplepos " << samplePosition << std::endl << std::flush;
        //std::cout << "TTL TS " << *(uint64_t*)(dataptr + 8) << " vs " << ttl->getTimestamp() << " vs " << event.getTimeStamp() << std::endl;

        sendEvent(Event::getEventType(event),
            samplePosition,
            eventId,
            eventChannel,
            numBytes,
            dataptr + 6,
            ttl->getTimestamp()); // there should be a better way... getTimeStamp()??
    }
}

void ZmqInterface::handleSpike(const SpikeChannel* spikeInfo, const MidiMessage& event, int samplePosition)
{
    std::cout << "spike" << std::endl;
    sendSpikeEvent(spikeInfo, event);
}

int ZmqInterface::receiveEvents(MidiBuffer& events)
{
    
    EventData ed;
    while (true)
    {
        int size = zmq_recv(pipeOutSocket, &ed, sizeof(ed), ZMQ_DONTWAIT);
        if (size == -1)
        {
            if (zmq_errno() == EAGAIN)
            {
                break;
            }
            else
            {
                std::cout << "pipe out error: " << zmq_strerror(zmq_errno()) << std::endl;
            }
        }
//        std::cout << "adding events" << std::endl;
//        std::cout << "chan " << (int)ed.eventChannel << " id " << (int)ed.eventId << " sample n "
//        << (int)ed.sampleNum << " type " << (int)ed.type <<
//        std::endl;
        
        int appNo = -1;
        for (int i = 0; i < applications.size(); i++)
        {
            ZmqApplication *app = applications[i];
            if (app->Uuid == String(ed.uuid))
            {
                bool refreshEd = false;
                appNo = i;
                app->lastSeen = ed.eventTime;
                if (!app->alive)
                    refreshEd = true;
                app->alive = true;
                ZmqInterfaceEditor *zed =    dynamic_cast<ZmqInterfaceEditor *> (getEditor());
                zed->refreshListAsync();

                break;
            }
        }

        if (appNo == -1)
        {
            ZmqApplication *app = new ZmqApplication;
            app->name = String(ed.application);
            app->Uuid = String(ed.uuid);
            app->lastSeen = ed.eventTime;
            app->alive = true;
            applications.add(app);
            std::cout << "adding new application " << app->name << " " << app->Uuid << std::endl;
            std::cout << " now there are " << applications.size() << " apps" << std::endl;
            ZmqInterfaceEditor *zed =    dynamic_cast<ZmqInterfaceEditor *> (getEditor());
            zed->refreshListAsync();
//            MessageManagerLock mmlock;
//            editor->repaint();
    
        }

        if (ed.isEvent) {
            std::cout << "ZMQ event received\n";
        }
#if 0
        if (ed.isEvent) {
            addEvent(events, ed.type, ed.sampleNum, ed.eventId, ed.eventChannel, ed.numBytes, NULL, false);
        }

        //void addEvent(int channelIndex, const Event* event, int sampleNum);
        //void addEvent(const EventChannel* channel, const Event* event, int sampleNum);
        // TODO allow for event data
#endif
    }

    return 0;
}

void ZmqInterface::checkForApplications()
{
    time_t timeNow = time(NULL);
    for (int i = 0; i < applications.size(); i++)
    {
        ZmqApplication *app = applications[i];
        if ((timeNow - app->lastSeen) > 10 && app->alive)
        {
            app->alive = false;
            std::cout << "app " << app->name << " not alive" << std::endl;
            ZmqInterfaceEditor *zed =    dynamic_cast<ZmqInterfaceEditor *> (getEditor());
            zed->refreshListAsync();
        }
    }
}

void ZmqInterface::process(AudioSampleBuffer& buffer)
{
    if (!socket)
        createDataSocket();
    
    if (!pipeOutSocket)
        openPipeOutSocket();

    checkForEvents(true); // see if we got any TTL events

    // current timestamp is at the end of the buffer; we want to send the timestamp of the first sample instead
    uint64_t firstTs = getTimestamp(0) - getNumSamples(0);

    float sampleRate;

    // Simplified sample rate detection (could check channel type or report
    // sampling rate of all channels separately in case they differ)
    if (dataChannelArray.size() > 0) 
    {
        sampleRate = dataChannelArray[0]->getSampleRate();
    }
    else 
    {   // this should never run - if no data channels, then no data...
        sampleRate = CoreServices::getGlobalSampleRate();
    }

    sendData(*(buffer.getArrayOfWritePointers()), buffer.getNumChannels(), 
        buffer.getNumSamples(), getNumSamples(0), firstTs, (int)sampleRate);

#if 0
    receiveEvents(events);
#endif
    checkForApplications();
}

void ZmqInterface::updateSettings()
{
    
}


