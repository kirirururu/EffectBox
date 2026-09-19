/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "IOPanels.h"
#include "plugins/PluginGraph.h"

class MainHostWindow;

//==============================================================================
/**
    A panel that displays and edits a PluginGraph.
*/
class GraphEditorPanel final : public Component, public ChangeListener, private Timer
{
public:
	//==============================================================================
	GraphEditorPanel(PluginGraph& graph);
	~GraphEditorPanel() override;

	void createNewPlugin(const PluginDescriptionAndPreference&, Point<int> position) const;

	void paint(Graphics&) override;
	void resized() override;

	void mouseDown(const MouseEvent&) override;
	void mouseUp(const MouseEvent&) override;
	void mouseDrag(const MouseEvent&) override;

	void changeListenerCallback(ChangeBroadcaster*) override;

	//==============================================================================
	void updateComponents();

	//==============================================================================
	void showPopupMenu(Point<int> position);

	//==============================================================================
	void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source,
	                        AudioProcessorGraph::NodeAndChannel dest,
	                        const MouseEvent&);
	void dragConnector(const MouseEvent&);
	void endDraggingConnector(const MouseEvent&);

	//==============================================================================
	void timerCallback() override;

	//==============================================================================
	PluginGraph& graph;

private:
	struct PluginComponent;
	struct ConnectorComponent;
	struct PinComponent;

	OwnedArray<PluginComponent> nodes;
	OwnedArray<ConnectorComponent> connectors;
	std::unique_ptr<ConnectorComponent> draggingConnector;
	std::unique_ptr<PopupMenu> menu;

	PluginComponent* getComponentForPlugin(AudioProcessorGraph::NodeID) const;
	ConnectorComponent* getComponentForConnection(const AudioProcessorGraph::Connection&) const;
	PinComponent* findPinAt(Point<float>) const;

	//==============================================================================
	Point<int> originalTouchPos;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphEditorPanel)
};


//==============================================================================
/**
    A panel that embeds a GraphEditorPanel with a midi keyboard at the bottom.

    It also manages the graph itself, and plays it.
*/
class GraphDocumentComponent final : public Component,
                                     public DragAndDropTarget,
                                     public DragAndDropContainer,
                                     private ChangeListener
{
public:
	GraphDocumentComponent(AudioPluginFormatManager& formatManager,
	                       AudioDeviceManager& deviceManager,
	                       KnownPluginList& pluginList);

	~GraphDocumentComponent() override;

	//==============================================================================
	void createNewPlugin(const PluginDescriptionAndPreference&, Point<int> position) const;
	void setDoublePrecision(bool doublePrecision);
	bool closeAnyOpenPluginWindows() const;

	//==============================================================================
	std::unique_ptr<PluginGraph> graph;

	void resized() override;
	void releaseGraph();

	//==============================================================================
	bool isInterestedInDragSource(const SourceDetails&) override;
	void itemDropped(const SourceDetails&) override;

	//==============================================================================
	std::unique_ptr<GraphEditorPanel> graphPanel;

	//==============================================================================
	void showSidePanel(bool isSettingsPanel);
	void hideLastSidePanel();

	//==============================================================================
	void changeListenerCallback(ChangeBroadcaster*) override;

	BurgerMenuComponent burgerMenu;

private:
	//==============================================================================
	AudioDeviceManager& deviceManager;
	KnownPluginList& pluginList;

	AudioProcessorPlayer graphPlayer;
	MidiOutput* midiOutput = nullptr;

	std::unique_ptr<IOPanelComponent> inputPanel;
	std::unique_ptr<IOPanelComponent> outputPanel;

	static constexpr int ioPanelWidth = 140;
	static constexpr int defaultNumEndpoints = 2;

	struct TooltipBar;
	std::unique_ptr<TooltipBar> statusBar;

	class TitleBarComponent;
	std::unique_ptr<TitleBarComponent> titleBarComponent;

	//==============================================================================
	struct PluginListBoxModel;
	std::unique_ptr<PluginListBoxModel> pluginListBoxModel;

	ListBox pluginListBox;

	SidePanel mobileSettingsSidePanel{"Settings", 300, true};
	SidePanel pluginListSidePanel{"Plugins", 250, false};
	SidePanel* lastOpenedSidePanel = nullptr;

	//==============================================================================
	void init();
	void checkAvailableWidth();
	void updateMidiOutput();

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphDocumentComponent)
};
