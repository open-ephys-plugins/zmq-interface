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
/*
  ==============================================================================

    ZmqInterface.h
    Created: 19 Sep 2015 9:47:12pm
    Author:  Francesco Battaglia

  ==============================================================================
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


//=============================================================================
/*
 */
class ZmqInterface    : public GenericProcessor, public Thread
{
public:
    /** The class constructor, used to initialize any members. */
    ZmqInterface(const String &processorName = "Zmq Interface");
    
    /** The class destructor, used to deallocate memory */
    ~ZmqInterface();
    
    /** Determines whether the processor is treated as a source. */
    virtual bool isSource()
    {
        return false;
    }
    
    /** Determines whether the processor is treated as a sink. */
    virtual bool isSink()
    {
        return false;
    }
    
    /** Defines the functionality of the processor.
     
     The process method is called every time a new data buffer is available.
     
     Processors can either use this method to add new data, manipulate existing
     data, or send data to an external target (such as a display or other hardware).
     
     Continuous signals arrive in the "buffer" variable, event data (such as TTLs
     and spikes) is contained in the "events" variable, and "nSamples" holds the
     number of continous samples in the current buffer (which may differ from the
     size of the buffer).
     */
    //virtual void process(AudioSampleBuffer& buffer, MidiBuffer& events);
    virtual void process(AudioSampleBuffer& continuousBuffer);
    
    /** Any variables used by the "process" function _must_ be modified only through
     this method while data acquisition is active. If they are modified in any
     other way, the application will crash.  */
    void setParameter(int parameterIndex, float newValue);
    
    AudioProcessorEditor* createEditor();
    
    bool hasEditor() const
    {
        return true;
    }
    
    void updateSettings();
    
    bool isReady();
    
    void resetConnections();
    void run();

    OwnedArray<ZmqApplication> *getApplicationList();

    // TODO void saveCustomParametersToXml(XmlElement* parentElement);
    // TODO void loadCustomParametersFromXml();

    bool threadRunning ;
    // TODO void setNewListeningPort(int port);
#if 0
    void setPorts(uint32_t newDataPort, uint32_t newListenPort);
    uint32_t getDataPort () const { return dataPort; }
    uint32_t getListenPort () const { return listenPort; }
#endif

private:
    int createContext();
    void openListenSocket();
    void openKillSocket();
    void openPipeOutSocket();
    int closeListenSocket();
    int createDataSocket();
    int closeDataSocket();

    void handleEvent(const EventChannel* eventInfo, const MidiMessage& event, int samplePosition);
    void handleSpike(const SpikeChannel* spikeInfo, const MidiMessage& event, int samplePosition);
    int sendData(float *data, int nChannels, int nSamples, int nRealSamples, 
                 int64 timestamp, int sampleRate);
    int sendEvent( uint8 type,
                  int sampleNum,
                  uint8 eventId,
                  uint8 eventChannel,
                  uint8 numBytes,
                  const uint8* eventData,
                  int64 timestamp);
    int sendSpikeEvent(const SpikeChannel* spikeInfo, const MidiMessage &event);
    
    int receiveEvents(MidiBuffer &events);
    void checkForApplications();
    
    template<typename T> int sendParam(String name, T value);

    
    void *context;
    void *socket;
    void *listenSocket;
    void *controlSocket;
    void *killSocket;
    void *pipeInSocket;
    void *pipeOutSocket;
    
    
    OwnedArray<ZmqApplication> applications;
    
    int flag;
    int messageNumber;
    int dataPort; //TODO make this editable
    int listenPort;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZmqInterface);
    
};






#endif  // ZMQINTERFACE_H_INCLUDED
