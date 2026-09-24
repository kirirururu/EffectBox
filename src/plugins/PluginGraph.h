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

#include "ui/PluginWindow.h"

//==============================================================================
/** A type that encapsulates a PluginDescription and some preferences regarding
    how plugins of that description should be instantiated.
*/
struct PluginDescriptionAndPreference
{
	enum class UseARA
	{
		no,
		yes
	};

	PluginDescriptionAndPreference() = default;

	explicit PluginDescriptionAndPreference(PluginDescription pd)
	    : pluginDescription(std::move(pd)),
	      useARA(pluginDescription.hasARAExtension ? PluginDescriptionAndPreference::UseARA::yes
	                                               : PluginDescriptionAndPreference::UseARA::no)
	{
	}

	PluginDescriptionAndPreference(PluginDescription pd, UseARA ara)
	    : pluginDescription(std::move(pd)), useARA(ara)
	{
	}

	PluginDescription pluginDescription;
	UseARA useARA = UseARA::no;
};

//==============================================================================
/**
    A single input or output endpoint of a plugin graph.

    Each endpoint has a unique identifier (generated automatically when the
    endpoint is created), a display name, and a channel count of either 1
    (mono) or 2 (stereo).
*/
struct GraphIOEndpoint
{
	GraphIOEndpoint() = default;

	Uuid id;
	String name;
	int numChannels = 1;
};

//==============================================================================
/**
    A collection of plugins and some connections between them.
*/
class PluginGraph final : public FileBasedDocument, public AudioProcessorListener, private ChangeListener
{
public:
	//==============================================================================
	PluginGraph(AudioPluginFormatManager&, KnownPluginList&);
	~PluginGraph() override;

	//==============================================================================
	using NodeID = AudioProcessorGraph::NodeID;

	void addPlugin(const PluginDescriptionAndPreference&, Point<double>);

	AudioProcessorGraph::Node::Ptr getNodeForName(const String& name) const;

	void setNodePosition(NodeID, Point<double>);
	Point<double> getNodePosition(NodeID) const;

	//==============================================================================
	/** Adds a new input or output endpoint with a freshly generated UUID.

	    @param numChannels  Either 1 (mono) or 2 (stereo); any other value
	                       is treated as mono.
	*/
	Uuid addIOEndpoint(const String& name, int numChannels, bool isInput);
	void removeIOEndpoint(const Uuid& id, bool isInput);
	void setIOEndpointName(const Uuid& id, const String& name, bool isInput);
	void setIOEndpointChannels(const Uuid& id, int numChannels, bool isInput);
	GraphIOEndpoint* findIOEndpoint(const Uuid& id, bool isInput);
	int indexOfIOEndpoint(const Uuid& id, bool isInput);

	//==============================================================================
	void clear();

	PluginWindow* getOrCreateWindowFor(AudioProcessorGraph::Node*, PluginWindow::Type);
	// void closeCurrentlyOpenWindowsFor (AudioProcessorGraph::NodeID);
	bool closeAnyOpenPluginWindows();

	//==============================================================================
	void audioProcessorParameterChanged(AudioProcessor*, int, float) override { }
	void audioProcessorChanged(AudioProcessor*, const ChangeDetails&) override { changed(); }

	//==============================================================================
	std::unique_ptr<XmlElement> createXml() const;
	void restoreFromXml(const XmlElement&);

	static const char* getFilenameSuffix() { return ".filtergraph"; }
	static const char* getFilenameWildcard() { return "*.filtergraph"; }

	//==============================================================================
	void newDocument();

	static File getDefaultGraphDocumentOnMobile();

	void changeListenerCallback(ChangeBroadcaster*) override;

	//==============================================================================
	AudioProcessorGraph graph;

	//==============================================================================
	/** The system inputs and outputs of the graph, in display order. */
	std::vector<GraphIOEndpoint> inputs;
	std::vector<GraphIOEndpoint> outputs;

protected:
	String getDocumentTitle() override;
	Result loadDocument(const File& file) override;
	Result saveDocument(const File& file) override;
	File getLastDocumentOpened() override;
	void setLastDocumentOpened(const File& file) override;

private:
	//==============================================================================
	AudioPluginFormatManager& formatManager;
	KnownPluginList& knownPlugins;
	OwnedArray<PluginWindow> activePluginWindows;
	ScopedMessageBox messageBox;

	NodeID lastUID;
	NodeID getNextUID() noexcept;

	void createNodeFromXml(const XmlElement&);
	void addPluginCallback(std::unique_ptr<AudioPluginInstance>,
	                       const String& error,
	                       Point<double>,
	                       PluginDescriptionAndPreference::UseARA useARA);

	//==============================================================================
	std::vector<GraphIOEndpoint>& ioVector(bool isInput) { return isInput ? inputs : outputs; }
	const std::vector<GraphIOEndpoint>& ioVector(bool isInput) const
	{
		return isInput ? inputs : outputs;
	}
	void seedDefaultIO();

	static constexpr int defaultNumIOEndpoints = 2;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGraph)
};
