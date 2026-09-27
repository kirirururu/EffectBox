#pragma once

#include "EngineClient.h"

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

//==============================================================================
/**
    A modal content component for adding, removing and editing the input and
    output endpoints of a plugin graph.

    It shows two lists (system inputs and system outputs) and an editing area
    for the currently selected endpoint: its name and its channel count
    (mono or stereo). Every change is written straight back into the
    referenced graph, which marks the document as changed and notifies its
    change listeners (so the graph editor side panels stay in sync).
*/
class GraphIOEditor final : public Component,
                            private TextEditor::Listener,
                            private ComboBox::Listener,
                            private Button::Listener
{
public:
	//==============================================================================
	GraphIOEditor(GraphMirror& mirror, EngineClient& client);

	//==============================================================================
	void resized() override;

	/** Called by the list models when a row is selected. */
	void rowSelected(bool isInput, int index);

	/** Appends a new endpoint to the given list and selects it. */
	void addEndpoint(bool isInput);

	/** Removes the currently selected endpoint. */
	void removeSelected();

	void textEditorTextChanged(TextEditor&) override;
	void comboBoxChanged(ComboBox*) override;
	void buttonClicked(Button*) override;

private:
	//==============================================================================
	class IOListModel;

	void updateEditingControls();

	const ::google::protobuf::RepeatedPtrField<proto::IOEndpoint>& endpoints(bool isInput) const
	{
		return isInput ? mirror.model().inputs() : mirror.model().outputs();
	}

	GraphMirror& mirror;
	EngineClient& client;

	ListBox inputList, outputList;
	std::unique_ptr<IOListModel> inputModel, outputModel;

	TextEditor nameEditor;
	ComboBox channelsCombo;
	TextButton addInputButton, addOutputButton, removeButton;
	Label inputTitle, outputTitle, nameLabel, channelsLabel;

	bool currentIsInput = true;
	int currentIndex = -1;
	bool updatingUI = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphIOEditor)
};
