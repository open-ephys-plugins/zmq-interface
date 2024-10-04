/*
 ------------------------------------------------------------------
 
 ZMQInterface
 Copyright (C) 2016 FP Battaglia
 
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

#ifndef ZMQINTERFACE_H_INCLUDED
#define ZMQINTERFACE_H_INCLUDED

#include <ProcessorHeaders.h>

#include <queue>

struct ZmqApplication {
    String name;
    String Uuid;
    time_t lastSeen;
    bool alive;
};

class ZmqInterface    : public GenericProcessor, public Thread, public Timer
{
public:
    /** The class constructor, used to initialize any members. */
    ZmqInterface(const String &processorName = "ZMQ Interface");
    
    /** The class destructor, used to deallocate memory */
    ~ZmqInterface();

    /** Registers the parameters of the processor */
    void registerParameters() override;

    /** Creates the custom editor*/
    AudioProcessorEditor* createEditor();
    
    /** Streams incoming data over a ZMQ socket */
    void process(AudioBuffer<float>& continuousBuffer);

    /** Called whenever the settings of upstream plugins have changed */
    void updateSettings() override;

    /** Called when a parameter is updated*/
    void parameterValueChanged(Parameter* param) override;

    /** Returns a list of connected applications */
    OwnedArray<ZmqApplication> *getApplicationList();

    uint16 selectedStream;
    String selectedStreamName;
    int selectedStreamSourceNodeId;
    float selectedStreamSampleRate;

private:

    /** Runs the ZMQ polling thread*/
    void run();

    /** Creates the ZMQ context */
    int createContext();

    /** Opens the listening socket */
    void openListenSocket();

    /** Opens the kill socket */
    void openKillSocket();

    /** Opens the pipe out socket */
    void openPipeOutSocket();

    /** Closes the listening socket */
    int closeListenSocket();

    /** Opens the data socket */
    int openDataSocket();

    /** Closes the data socket */
    int closeDataSocket();

    /** Called at the start of acquisition */
    bool startAcquisition();
    
    /** Called at the end of acquisition */
    bool stopAcquisition();

    /** Called whenever a new TTL event arrives */
    void handleTTLEvent(TTLEventPtr event) override;

    /** Called whenever a new spike arrives */
    void handleSpike (SpikePtr spike) override;

    /** Sends continuous data for one channel over the ZMQ socket */
    int sendData(float *data, int channelNum, int nSamples, int64 sampleNumber, float sampleRate);

    /** Sends an event over the ZMQ socket */
    int sendEvent( uint8 type,
                  int64 sampleNum,
                  int sourceNodeId,
                  size_t numBytes,
                  const uint8* eventData);

    /** Sends a spike over the ZMQ socket */
    int sendSpikeEvent(const SpikePtr spike);
    
    /** Currently only supports events related to keeping track of connected applications */
    int receiveEvents();

    /** Checks for connected applications */
    void checkForApplications();

    /** Calls checkForApplications() */
    void timerCallback();
    
    void *context;
    void *socket;
    void *listenSocket;
    void *controlSocket;
    void *killSocket;
    void *pipeInSocket;
    void *pipeOutSocket;
    
    OwnedArray<ZmqApplication> applications;
    
    int messageNumber;
    int dataPort;
    int listenPort;

    Array<int> selectedChannels;
    std::map<uint16, String> streamNamesMap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZmqInterface);
    
};


#endif  // ZMQINTERFACE_H_INCLUDED
