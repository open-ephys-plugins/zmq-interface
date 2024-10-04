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

#include <zmq.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <errno.h>
#include "ZmqInterface.h"
#include "ZmqInterfaceEditor.h"

#define DEBUG_ZMQ
const int MAX_MESSAGE_LENGTH = 64000;

struct EventData
{
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

ZmqInterface::ZmqInterface (const String& processorName)
    : GenericProcessor (processorName), Thread ("ZMQ thread")
{
    context = nullptr;
    socket = nullptr;
    listenSocket = nullptr;
    controlSocket = nullptr;
    killSocket = nullptr;
    pipeInSocket = nullptr;
    pipeOutSocket = nullptr;

    messageNumber = 0;
    dataPort = 5556;
    listenPort = dataPort + 1;
    selectedStream = 0;
    selectedStreamSourceNodeId = 0;
    selectedStreamName = "";
    selectedStreamSampleRate = 0.0f;

    createContext();

    openListenSocket();
    openDataSocket();
    openKillSocket();
    openPipeOutSocket();
}

ZmqInterface::~ZmqInterface()
{
    //LOGD("Sending stop message");
    //zmq_msg_t messageEnvelope;
    //zmq_msg_init_size(&messageEnvelope, strlen("STOP")+1);
    //memcpy(zmq_msg_data(&messageEnvelope), "STOP", strlen("STOP")+1);
    // zmq_msg_send(&messageEnvelope, killSocket, 0);
    // zmq_msg_close(&messageEnvelope);
    // LOGD("Sent stop message");

    closeDataSocket();
    closeListenSocket(); // stop the polling thread

    zmq_close (pipeOutSocket);
    zmq_close (killSocket);

    sleep (250);

    if (context)
    {
        zmq_ctx_destroy (context);
        context = 0;
    }
}

void ZmqInterface::registerParameters()
{
    addMaskChannelsParameter (Parameter::STREAM_SCOPE, "channels", "Channels", "The input channels data to send");
    addSelectedStreamParameter (Parameter::PROCESSOR_SCOPE, "stream", "Stream", "The selected stream to send data from", {}, 0, true, true);
    addIntParameter (Parameter::PROCESSOR_SCOPE, "data_port", "Data Port", "Port number to send data", dataPort, 1000, 65535, true);
}

AudioProcessorEditor* ZmqInterface::createEditor()
{
    editor = std::make_unique<ZmqInterfaceEditor> (this);
    return editor.get();
}

OwnedArray<ZmqApplication>* ZmqInterface::getApplicationList()
{
    return &applications;
}

int ZmqInterface::createContext()
{
    LOGD ("Creating ZMQ context");

    context = zmq_ctx_new();
    if (! context)
        return -1;
    return 0;
}

int ZmqInterface::openDataSocket()
{
    if (! socket)
    {
        LOGD ("Opening data socket");

        socket = zmq_socket (context, ZMQ_PUB);
        if (! socket)
            return -1;
        String urlstring;
        urlstring = String ("tcp://*:") + String (dataPort);
        LOGD ("[ZMQ data socket] ", urlstring);
        int rc = zmq_bind (socket, urlstring.toRawUTF8());
        if (rc)
        {
            LOGE ("Couldn't open data socket");
            LOGE (zmq_strerror (zmq_errno()));
            jassert (false);
        }
    }

    return 0;
}

int ZmqInterface::closeDataSocket()
{
    if (socket)
    {
        LOGD ("Closing data socket");

        int rc = zmq_close (socket);
        jassert (rc == 0);
        socket = nullptr;
    }
    return 0;
}

void ZmqInterface::openListenSocket()
{
    if (! listenSocket)
    {
        LOGD ("Opening listening socket");

        listenSocket = zmq_socket (context, ZMQ_REP);
        String urlstring;
        urlstring = String ("tcp://*:") + String (listenPort);
        LOGD ("[ZMQ listen socket] ", urlstring);
        int rc = zmq_bind (listenSocket, urlstring.toRawUTF8()); // give the chance to change the port

        if (rc != 0) // port is taken
        {
            dataPort += 2;
            listenPort = dataPort + 1;

            getParameter ("data_port")->setNextValue (dataPort);
            return;
        }

        startThread();
        LOGD ("Starting timer callbacks");
        startTimer (500);
    }
}

int ZmqInterface::closeListenSocket()
{
    int rc = 0;

    if (listenSocket)
    {
        LOGD ("Closing listening socket");

        stopTimer();

        stopThread (500);

        rc = zmq_close (listenSocket);
        listenSocket = nullptr;
    }

    return rc;
}

void ZmqInterface::openKillSocket()
{
    killSocket = zmq_socket (context, ZMQ_PAIR);
    zmq_connect (killSocket, "inproc://zmqthreadcontrol");
}

void ZmqInterface::openPipeOutSocket()
{
    LOGD ("Opening pipeout socket");

    pipeOutSocket = zmq_socket (context, ZMQ_PAIR);
    zmq_bind (pipeOutSocket, "inproc://zmqthreadpipe");
}

void ZmqInterface::timerCallback()
{
    receiveEvents();

    checkForApplications();
}

void ZmqInterface::run()
{
    LOGD ("Starting ZMQ thread");

    char* buffer = new char[MAX_MESSAGE_LENGTH];

    LOGD ("ZMQ: creating control socket");
    controlSocket = zmq_socket (context, ZMQ_PAIR);
    zmq_bind (controlSocket, "inproc://zmqthreadcontrol");

    LOGD ("ZMQ: creating pipe in socket");
    pipeInSocket = zmq_socket (context, ZMQ_PAIR);
    zmq_connect (pipeInSocket, "inproc://zmqthreadpipe");

    int size;

    zmq_pollitem_t items[] = {
        { listenSocket, 0, ZMQ_POLLIN, 0 },
        { controlSocket, 0, ZMQ_POLLIN, 0 }
    };

    while (! threadShouldExit())
    {
        zmq_poll (items, 2, 100);

        if (items[0].revents & ZMQ_POLLIN)
        {
            size = zmq_recv (listenSocket, buffer, MAX_MESSAGE_LENGTH - 1, 0);
            buffer[size] = 0;

            if (size < 0)
            {
                LOGE ("Failed in receiving listen socket");
                LOGE (zmq_strerror (zmq_errno()));
                jassert (false);
            }
            var v;
            Result rs = JSON::parse (String (buffer), v);
            bool ok = rs.wasOk();

            EventData ed;
            String app = v["application"];
            String appUuid = v["uuid"];
            strncpy (ed.application, app.toRawUTF8(), 255);
            strncpy (ed.uuid, appUuid.toRawUTF8(), 255);
            ed.eventTime = time (NULL);

            String evT = v["type"];

            if (evT == "event")
            {
                ed.isEvent = true;
                ed.eventChannel = (int) v["event"]["event_channel"];
                ed.eventId = (int) v["event"]["event_id"];
                ed.numBytes = 0; // TODO  allow for event data
                ed.sampleNum = (int) v["event"]["sample_num"];
                ed.type = (int) v["event"]["type"];
            }
            else
            {
                ed.isEvent = false;
            }

            // TODO allow for event data
            zmq_msg_t message;
            zmq_msg_init_size (&message, sizeof (EventData));
            memcpy (zmq_msg_data (&message), &ed, sizeof (EventData));
            int size_m = zmq_msg_send (&message, pipeInSocket, 0);
            jassert (size_m);
            size += size_m;
            zmq_msg_close (&message);

            // send response
            String response;
            if (ok)
            {
                if (ed.isEvent)
                {
                    response = String ("message correctly parsed");
                }
                else
                {
                    response = String ("heartbeat received");
                }
            }
            else
            {
                response = String ("JSON message could not be read");
            }

            zmq_send (listenSocket, response.getCharPointer(), response.length(), 0);
        }
    }
    
    



    delete[] buffer;

    zmq_close (pipeInSocket);
    zmq_close (controlSocket);
    pipeInSocket = nullptr;
    controlSocket = nullptr;

    return;
}

/* format of output packets (JSON)
 { 
  "message_num": number,
  "type": "data"|"event"|"spike"|"parameter",
  "content":
  (for data)
  {
    "stream" : stream name (string)
    "channel_num" : local channel index
    "num_samples": num of samples in this buffer
    "sample_num": index of first sample
    "sample_rate": sampling rate of this channel
  }
  (for event)
  {
    "stream" : stream name (string)
    "source_node" : processor ID that generated the event
    "type": specifies TTL vs. message,
    "sample_num": index of the event
  }
  (for spike)
  {
    "stream" : stream name (string)
    "source_node" : processor ID that generated the spike
    "electrode" : name of the spike channel
    "sample_num" : index of the peak sample
    "num_channels" : total number of channels in this spike
    "num_samples" : total number of samples in this spike
    "sorted_id" : sorted ID (default = 0)
    "threshold" : threshold values across all channels
  }
  (for parameter) // todo
  {
    "param_name1": param_value1,
    "param_name2": param_value2,
  }
  "data_size": size (if size > 0 it's the size of binary data coming in 
               the next frame (multi-part message))
 }
 
 and then a possible data packet
 */

bool ZmqInterface::startAcquisition()
{
    messageNumber = 0;

    return true;
}

bool ZmqInterface::stopAcquisition()
{
    LOGC ("ZMQ Interface -- total messages sent: ", messageNumber);

    return true;
}

int ZmqInterface::sendData (float* data,
                            int channelNum,
                            int nSamples,
                            int64 sampleNumber,
                            float sampleRate)
{
    messageNumber++;

    DynamicObject::Ptr obj = new DynamicObject();

    int mn = messageNumber;
    obj->setProperty ("message_num", mn);
    obj->setProperty ("type", "data");

    DynamicObject::Ptr c_obj = new DynamicObject();

    c_obj->setProperty ("stream", selectedStreamName);
    c_obj->setProperty ("channel_num", channelNum);
    c_obj->setProperty ("num_samples", nSamples);
    c_obj->setProperty ("sample_num", sampleNumber);
    c_obj->setProperty ("sample_rate", sampleRate);

    obj->setProperty ("content", var (c_obj));
    obj->setProperty ("data_size", (int) (nSamples * sizeof (float)));
    obj->setProperty ("timestamp", Time::currentTimeMillis());

    var json (obj);

    String s = JSON::toString (json);
    void* headerData = (void*) s.toRawUTF8();

    size_t headerSize = s.length();

    zmq_msg_t messageEnvelope;
    zmq_msg_init_size (&messageEnvelope, strlen ("DATA") + 1);
    memcpy (zmq_msg_data (&messageEnvelope), "DATA", strlen ("DATA") + 1);
    int size = zmq_msg_send (&messageEnvelope, socket, ZMQ_SNDMORE);
    jassert (size != -1);
    zmq_msg_close (&messageEnvelope);

    zmq_msg_t messageHeader;
    zmq_msg_init_size (&messageHeader, headerSize);
    memcpy (zmq_msg_data (&messageHeader), headerData, headerSize);
    size = zmq_msg_send (&messageHeader, socket, ZMQ_SNDMORE);
    jassert (size != -1);
    zmq_msg_close (&messageHeader);

    zmq_msg_t message;
    zmq_msg_init_size (&message, sizeof (float) * nSamples);
    memcpy (zmq_msg_data (&message), data, sizeof (float) * nSamples);
    int size_m = zmq_msg_send (&message, socket, 0);
    jassert (size_m);
    size += size_m;
    zmq_msg_close (&message);

    return size;
}

int ZmqInterface::sendSpikeEvent (const SpikePtr spike)
{
    messageNumber++;
    int size = 0;

    int bufferSize = spike->getChannelInfo()->getDataSize();
    if (bufferSize)
    {
        if (spike)
        {
            DynamicObject::Ptr obj = new DynamicObject();
            obj->setProperty ("message_num", messageNumber);
            obj->setProperty ("type", "spike");

            DynamicObject::Ptr c_obj = new DynamicObject();
            const SpikeChannel* channel = spike->getChannelInfo();
            int64 nChannels = channel->getNumChannels();

            c_obj->setProperty ("stream", selectedStreamName);
            c_obj->setProperty ("source_node", spike->getProcessorId());
            c_obj->setProperty ("electrode", channel->getName());
            c_obj->setProperty ("sample_num", spike->getSampleNumber());
            c_obj->setProperty ("num_channels", nChannels);
            c_obj->setProperty ("num_samples", (int64) channel->getTotalSamples());
            c_obj->setProperty ("sorted_id", spike->getSortedId());

            var t_var;
            for (int i = 0; i < nChannels; i++)
                t_var.append (spike->getThreshold (i));
            c_obj->setProperty ("threshold", t_var);

            obj->setProperty ("spike", var (c_obj));
            obj->setProperty ("timestamp", Time::currentTimeMillis());

            var json (obj);
            String s = JSON::toString (json);
            void* headerData = (void*) s.toRawUTF8();
            size_t headerSize = s.length();

            zmq_msg_t messageEnvelope;
            zmq_msg_init_size (&messageEnvelope, strlen ("EVENT") + 1);
            memcpy (zmq_msg_data (&messageEnvelope), "EVENT", strlen ("EVENT") + 1);
            size = zmq_msg_send (&messageEnvelope, socket, ZMQ_SNDMORE);
            jassert (size != -1);
            zmq_msg_close (&messageEnvelope);

            zmq_msg_t messageHeader;
            zmq_msg_init_size (&messageHeader, headerSize);
            memcpy (zmq_msg_data (&messageHeader), headerData, headerSize);
            size = zmq_msg_send (&messageHeader, socket, ZMQ_SNDMORE);
            jassert (size != -1);
            zmq_msg_close (&messageHeader);
            zmq_msg_t message;
            zmq_msg_init_size (&message, channel->getDataSize());
            // getdatapointer???
            memcpy (zmq_msg_data (&message), spike->getDataPointer(), channel->getDataSize());
            size = zmq_msg_send (&message, socket, 0);
        }
    }
    return 0;
}

int ZmqInterface::sendEvent (uint8 type,
                             int64 sampleNum,
                             int sourceNodeId,
                             size_t numBytes,
                             const uint8* eventData)
{
    int size;

    messageNumber++;

    DynamicObject::Ptr obj = new DynamicObject();

    obj->setProperty ("message_num", messageNumber);
    obj->setProperty ("type", "event");

    DynamicObject::Ptr c_obj = new DynamicObject();
    c_obj->setProperty ("stream", selectedStreamName);
    c_obj->setProperty ("source_node", sourceNodeId);
    c_obj->setProperty ("type", type);
    c_obj->setProperty ("sample_num", sampleNum);

    obj->setProperty ("content", var (c_obj));
    obj->setProperty ("data_size", (int) numBytes);
    obj->setProperty ("timestamp", Time::currentTimeMillis());

    var json (obj);
    String s = JSON::toString (json);
    void* headerData = (void*) s.toRawUTF8();
    size_t headerSize = s.length();
    
    
  

  
    zmq_msg_t messageEnvelope;
    zmq_msg_init_size (&messageEnvelope, strlen ("EVENT") + 1);
    memcpy (zmq_msg_data (&messageEnvelope), "EVENT", strlen ("EVENT") + 1);
    size = zmq_msg_send (&messageEnvelope, socket, ZMQ_SNDMORE);
    jassert (size != -1);
    zmq_msg_close (&messageEnvelope);

    zmq_msg_t messageHeader;
    zmq_msg_init_size (&messageHeader, headerSize);
    memcpy (zmq_msg_data (&messageHeader), headerData, headerSize);
    if (numBytes == 0)
    {
        size = zmq_msg_send (&messageHeader, socket, 0);
        jassert (size != -1);
        zmq_msg_close (&messageHeader);
    }
    else
    {
        size = zmq_msg_send (&messageHeader, socket, ZMQ_SNDMORE);
        jassert (size != -1);
        zmq_msg_close (&messageHeader);
        zmq_msg_t message;
        zmq_msg_init_size (&message, numBytes);
        memcpy (zmq_msg_data (&message), eventData, numBytes);
        int size_m = zmq_msg_send (&message, socket, 0);
        jassert (size_m);
        size += size_m;
        zmq_msg_close (&message);
    }
    return size;
}

void ZmqInterface::handleTTLEvent (TTLEventPtr event)
{
    if (event->getEventType() == EventChannel::TTL && event->getStreamId() == selectedStream)
    {
        const uint8* dataptr = reinterpret_cast<const uint8*> (event->getRawDataPointer());
        uint8 numBytes = event->getChannelInfo()->getDataSize();

        sendEvent (event->getEventType(),
                   event->getSampleNumber(),
                   event->getProcessorId(),
                   numBytes,
                   dataptr);
    }
}

void ZmqInterface::handleSpike (SpikePtr spike)
{
    if (spike->getStreamId() == selectedStream)
        sendSpikeEvent (spike);
}

int ZmqInterface::receiveEvents()
{
    EventData ed;
    while (true)
    {
        int size = zmq_recv (pipeOutSocket, &ed, sizeof (ed), ZMQ_DONTWAIT);
        if (size == -1)
        {
            if (zmq_errno() == EAGAIN)
            {
                break;
            }
            else
            {
                LOGE ("Pipe out error: ", zmq_strerror (zmq_errno()));
            }
        }

        int appNo = -1;
        for (int i = 0; i < applications.size(); i++)
        {
            ZmqApplication* app = applications[i];
            if (app->Uuid == String (ed.uuid))
            {
                bool refreshEd = false;
                appNo = i;
                app->lastSeen = ed.eventTime;
                if (! app->alive)
                    refreshEd = true;
                app->alive = true;
                ZmqInterfaceEditor* zed = dynamic_cast<ZmqInterfaceEditor*> (getEditor());
                zed->refreshListAsync();

                break;
            }
        }

        if (appNo == -1)
        {
            ZmqApplication* app = new ZmqApplication;
            app->name = String (ed.application);
            app->Uuid = String (ed.uuid);
            app->lastSeen = ed.eventTime;
            app->alive = true;
            applications.add (app);
            LOGC ("Adding new zmq client application ", app->name, " ", app->Uuid);
            ZmqInterfaceEditor* zed = dynamic_cast<ZmqInterfaceEditor*> (getEditor());
            zed->refreshListAsync();
        }

        if (ed.isEvent)
        {
            LOGD ("ZMQ event received");
        }
    }

    return 0;
}

void ZmqInterface::checkForApplications()
{
    time_t timeNow = time (NULL);

    for (int i = 0; i < applications.size(); i++)
    {
        ZmqApplication* app = applications[i];

        if ((timeNow - app->lastSeen) > 5 && app->alive)
        {
            app->alive = false;
            LOGC ("App ", app->name, " no longer alive");
            ZmqInterfaceEditor* zed = dynamic_cast<ZmqInterfaceEditor*> (getEditor());

            if (zed != nullptr)
                zed->refreshListAsync();
        }
    }
}

void ZmqInterface::process (AudioBuffer<float>& buffer)
{
    checkForEvents (true); // see if we got any TTL events or spikes

    for (auto stream : dataStreams)
    {
        if ((*stream)["enable_stream"]
            && stream->getStreamId() == selectedStream)
        {
            // Send the sample number of the first sample in the buffer block
            int64 sampleNum = getFirstSampleNumberForBlock (stream->getStreamId());
            int numSamples = getNumSamplesInBlock (selectedStream);

            if (numSamples == 0)
                continue;

            for (auto chan : selectedChannels)
            {
                int globalChanIndex = stream->getContinuousChannels().getUnchecked (chan)->getGlobalIndex();
                sendData (buffer.getWritePointer (globalChanIndex), chan, numSamples, sampleNum, selectedStreamSampleRate);
            }
        }
    }
}

void ZmqInterface::updateSettings()
{
}

void ZmqInterface::parameterValueChanged (Parameter* param)
{
    if (param->getName().equalsIgnoreCase ("channels"))
    {
        if (param->getStreamId() == selectedStream)
            selectedChannels = static_cast<MaskChannelsParameter*> (param)->getArrayValue();
    }
    else if (param->getName().equalsIgnoreCase ("stream"))
    {
        String streamKey = param->getValueAsString();

        selectedStream = getDataStream (streamKey)->getStreamId();
        selectedStreamName = getDataStream (streamKey)->getName();
        selectedStreamSourceNodeId = getDataStream (streamKey)->getSourceNodeId();
        selectedStreamSampleRate = getDataStream (streamKey)->getSampleRate();
    }
    else if (param->getName().equalsIgnoreCase ("data_port"))
    {
        int newDataPort = static_cast<IntParameter*> (param)->getIntValue();

        if (dataPort != newDataPort)
        {
            // close previous sockets and assign new ports

            closeListenSocket();
            closeDataSocket();
            dataPort = newDataPort;
            listenPort = dataPort + 1;
            openListenSocket();
            openDataSocket();
        }
    }
}