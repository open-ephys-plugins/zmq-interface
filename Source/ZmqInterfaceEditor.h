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

    ZmqInterfaceEditor.h
    Created: 22 Sep 2015 9:42:44am
    Author:  Francesco Battaglia

  ==============================================================================
*/

#ifndef ZMQINTERFACEEDITOR_H_INCLUDED
#define ZMQINTERFACEEDITOR_H_INCLUDED

#include <EditorHeaders.h>
class ZmqInterface;

struct ZmqApplication;

class ZmqInterfaceEditor: public GenericEditor
{
public:

    /** Constructor */
    ZmqInterfaceEditor(GenericProcessor *parentNode);

    /** Destructor */
    virtual ~ZmqInterfaceEditor();

    /** Refresh list of connected apps */
    void refreshListAsync();

    /** Updates list of connected apps */
    void startAcquisition() override;

    /** Updates list of connected apps */
    void stopAcquisition() override;

private:
    class ZmqInterfaceEditorListBox;

    OwnedArray<ZmqApplication> *getApplicationList();
    ZmqInterface *ZmqProcessor;
    std::unique_ptr<ZmqInterfaceEditorListBox> listBox;
    std::unique_ptr<Label> listTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZmqInterfaceEditor)
    
};

#endif  // ZMQINTERFACEEDITOR_H_INCLUDED
