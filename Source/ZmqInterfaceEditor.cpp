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

    ZmqInterfaceEditor.cpp
    Created: 22 Sep 2015 9:42:44am
    Author:  Francesco Battaglia

  ==============================================================================
*/




#include "ZmqInterfaceEditor.h"
#include "ZmqInterface.h"

class ZmqInterfaceEditor::ZmqInterfaceEditorListBox: public ListBox,
private ListBoxModel, public AsyncUpdater
{
public:
    ZmqInterfaceEditorListBox(const String noItemsText, ZmqInterfaceEditor *e):
    ListBox(String::empty, nullptr), noItemsMessage(noItemsText)
    {
        editor = e;
        setModel(this);
        
        backgroundGradient = ColourGradient(Colour(190, 190, 190), 0.0f, 0.0f,
                                            Colour(185, 185, 185), 0.0f, 120.0f, false);
        backgroundGradient.addColour(0.2f, Colour(155, 155, 155));
        
        backgroundColor = Colour(155, 155, 155);
        
        setColour(backgroundColourId, backgroundColor);
        
        refresh();

    }
    
    void handleAsyncUpdate()
    {
        refresh();
    }
    
    void refresh()
    {
        updateContent();
        repaint();
        
    }
    
    int getNumRows() override
    {
        return editor->getApplicationList()->size();
    }
    
    
    void paintListBoxItem (int row, Graphics& g, int width, int height, bool rowIsSelected) override
    {
        OwnedArray<ZmqApplication> *items = editor->getApplicationList();
        if (isPositiveAndBelow (row, items->size()))
        {
            g.fillAll(Colour(155, 155, 155));
            if (rowIsSelected)
                g.fillAll (findColour (TextEditor::highlightColourId)
                           .withMultipliedAlpha (0.3f));
            
            ZmqApplication *i = (*items)[row];
            const String item (i->name); // TODO change when we put a map
                
            const int x = getTickX();
            
            g.setFont (height * 0.6f);
            if (i->alive)
                g.setColour(Colours::green);
            else
                g.setColour(Colours::red);
            g.drawText (item, x, 0, width - x - 2, height, Justification::centredLeft, true);
        } // end of function
    }
    
    
    void listBoxItemClicked (int row, const MouseEvent& e) override
    {
        selectRow (row);
    }
    
    void paint (Graphics& g) override
    {
        ListBox::paint (g);
        g.setColour (Colours::grey);
        g.setGradientFill(backgroundGradient);
        if (editor->getApplicationList()->size() == 0)
        {
            g.setColour (Colours::grey);
            g.setFont (13.0f);
            g.drawText (noItemsMessage,
                        0, 0, getWidth(), getHeight() / 2,
                        Justification::centred, true);
        }
    }
    
    
    
    
private:
    const String noItemsMessage;
    ZmqInterfaceEditor *editor;
    /** Stores the editor's background color. */
    Colour backgroundColor;
    
    /** Stores the editor's background gradient. */
    ColourGradient backgroundGradient;
    
    int getTickX() const
    {
        return getRowHeight() + 5;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZmqInterfaceEditorListBox)
};

ZmqInterfaceEditor::ZmqInterfaceEditor(GenericProcessor *parentNode, bool useDefaultParameters): GenericEditor(parentNode, useDefaultParameters)
{
    ZmqProcessor = (ZmqInterface *)parentNode;
    listBox = new ZmqInterfaceEditorListBox(String("no app connected"), this);
    listBox->setBounds(2,25,130,105);
    addAndMakeVisible(listBox);
#if 0
    dataPortEditor = new TextEditor("dataport");
    addAndMakeVisible(dataPortEditor);
    dataPortEditor->setBounds(10, 110, 40, 16);
    dataPortEditor->setText(String(ZmqProcessor->getDataPort()));

	portButton = new TextButton("set_ports");
    addAndMakeVisible(portButton);
    portButton->setBounds(90, 110, 40, 16);
    portButton->setButtonText("set ports");
    portButton->addListener(this);
#endif
    setEnabledState(false);
}

ZmqInterfaceEditor::~ZmqInterfaceEditor()
{
    deleteAllChildren();
}

void ZmqInterfaceEditor::saveCustomParameters(XmlElement *xml)
{
    // todo: ports    
}

void ZmqInterfaceEditor::loadCustomParameters(XmlElement* xml)
{
    // todo: ports
}

void ZmqInterfaceEditor::refreshListAsync()
{
    listBox->triggerAsyncUpdate();
}

OwnedArray<ZmqApplication> *ZmqInterfaceEditor::getApplicationList()
{
    OwnedArray<ZmqApplication> *ar = ZmqProcessor->getApplicationList();
    return ar;
    
}

#if 0
void ZmqInterfaceEditor::buttonClicked(Button* button)
{
    if (button == portButton)
    {
        String dport = dataPortEditor->getText();
        int dportVal = dport.getIntValue();
        if ( (dportVal == 0) && !dport.containsOnly("0")) {
            // wrong integer input
            CoreServices::sendStatusMessage("Invalid data port value");
            dataPortEditor->setText(String(ZmqProcessor->getDataPort()));
        } else {
            ZmqProcessor->setPorts(dportVal, dportVal + 1);
            CoreServices::sendStatusMessage("ZMQ ports updated");
        }
    }
}
#endif

