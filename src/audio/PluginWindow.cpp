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

#include "PluginWindow.h"
#include "PluginDebugWindow.h"

struct PluginWindow::ProgramAudioProcessorEditor final : AudioProcessorEditor
{
	explicit ProgramAudioProcessorEditor(AudioProcessor& p);

	void paint(Graphics& g) override;
	void resized() override;

private:
	class Model : public ListBoxModel
	{
	public:
		Model(Component& o, AudioProcessor& p);

		int getNumRows() override;
		void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override;
		void selectedRowsChanged(int row) override;

	private:
		Component& owner;
		AudioProcessor& proc;
	};

	Model model{*this, *getAudioProcessor()};
	ListBox listBox{"Programs", &model};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgramAudioProcessorEditor)
};

//==============================================================================
PluginWindow::PluginWindow(AudioProcessorGraph::Node* n,
                           Type t,
                           OwnedArray<PluginWindow>& windowList,
                           KeyListener* keyListener)
    : DocumentWindow(n->getProcessor()->getName() + getFormatSuffix(n->getProcessor()),
                     LookAndFeel::getDefaultLookAndFeel().findColour(backgroundColourId),
                     minimiseButton | closeButton),
      activeWindowList(windowList),
      node(n),
      type(t)
{
	setSize(400, 300);

	if (auto* ui = createProcessorEditor(*node->getProcessor(), type))
	{
		setContentOwned(ui, true);
		setResizable(ui->isResizable(), false);
	}

	setConstrainer(&constrainer);

	setTopLeftPosition(
	    node->properties.getWithDefault(getLastXProp(type), Random::getSystemRandom().nextInt(500)),
	    node->properties.getWithDefault(getLastYProp(type), Random::getSystemRandom().nextInt(500)));

	node->properties.set(getOpenProp(type), true);

	setVisible(true);

	addKeyListener(keyListener);
}

PluginWindow::~PluginWindow()
{
	clearContentComponent();
}

void PluginWindow::closeButtonPressed()
{
	node->properties.set(getOpenProp(type), false);
	activeWindowList.removeObject(this);
}

float PluginWindow::getDesktopScaleFactor() const
{
	return 1.0f;
}

String PluginWindow::getLastXProp(Type type)
{
	return "uiLastX_" + getTypeName(type);
}

String PluginWindow::getLastYProp(Type type)
{
	return "uiLastY_" + getTypeName(type);
}

String PluginWindow::getOpenProp(Type type)
{
	return "uiopen_" + getTypeName(type);
}

void PluginWindow::moved()
{
	node->properties.set(getLastXProp(type), getX());
	node->properties.set(getLastYProp(type), getY());
}

AudioProcessorEditor* PluginWindow::createProcessorEditor(AudioProcessor& processor,
														  PluginWindow::Type type)
{
	if (type == PluginWindow::Type::normal)
	{
		if (processor.hasEditor())
			if (auto* ui = processor.createEditorAndMakeActive())
				return ui;

		type = PluginWindow::Type::generic;
	}

	if (type == PluginWindow::Type::generic)
	{
		auto* result = new GenericAudioProcessorEditor(processor);
		result->setResizeLimits(200, 300, 1'000, 10'000);
		return result;
	}

	if (type == PluginWindow::Type::programs)
		return new ProgramAudioProcessorEditor(processor);

	// if (type == PluginWindow::Type::audioIO)
	// 	return new IOConfigurationWindow(processor);

	if (type == PluginWindow::Type::debug)
		return new PluginDebugWindow(processor);

	jassertfalse;
	return {};
}

String PluginWindow::getTypeName(Type type)
{
	switch (type)
	{
	case Type::normal:
		return "Normal";
	case Type::generic:
		return "Generic";
	case Type::programs:
		return "Programs";
	case Type::audioIO:
		return "IO";
	case Type::debug:
		return "Debug";
	case Type::numTypes:
	default:
		return {};
	}
}

//==============================================================================
PluginWindow::DecoratorConstrainer::DecoratorConstrainer(DocumentWindow& windowIn)
    : window(windowIn)
{
}

ComponentBoundsConstrainer* PluginWindow::DecoratorConstrainer::getWrappedConstrainer() const
{
	auto* editor = dynamic_cast<AudioProcessorEditor*>(window.getContentComponent());
	return editor != nullptr ? editor->getConstrainer() : nullptr;
}

BorderSize<int> PluginWindow::DecoratorConstrainer::getAdditionalBorder() const
{
	const auto nativeFrame = [&]() -> BorderSize<int>
	{
		if (auto* peer = window.getPeer())
			if (const auto frameSize = peer->getFrameSizeIfPresent())
				return *frameSize;

		return {};
	}();

	return nativeFrame.addedTo(window.getContentComponentBorder());
}

//==============================================================================
PluginWindow::ProgramAudioProcessorEditor::ProgramAudioProcessorEditor(AudioProcessor& p)
    : AudioProcessorEditor(p)
{
	setOpaque(true);

	addAndMakeVisible(listBox);
	listBox.updateContent();

	const auto rowHeight = listBox.getRowHeight();

	setSize(400, jlimit(rowHeight, 400, p.getNumPrograms() * rowHeight));
}

void PluginWindow::ProgramAudioProcessorEditor::paint(Graphics& g)
{
	g.fillAll(Colours::grey);
}

void PluginWindow::ProgramAudioProcessorEditor::resized()
{
	listBox.setBounds(getLocalBounds());
}

PluginWindow::ProgramAudioProcessorEditor::Model::Model(Component& o, AudioProcessor& p)
    : owner(o), proc(p)
{
}

int PluginWindow::ProgramAudioProcessorEditor::Model::getNumRows()
{
	return proc.getNumPrograms();
}

void PluginWindow::ProgramAudioProcessorEditor::Model::paintListBoxItem(int rowNumber,
                                                                        Graphics& g,
                                                                        int width,
                                                                        int height,
                                                                        bool rowIsSelected)
{
	const auto textColour = owner.findColour(ListBox::textColourId);

	if (rowIsSelected)
	{
		const auto defaultColour = owner.findColour(ListBox::backgroundColourId);
		const auto c =
		    rowIsSelected ? defaultColour.interpolatedWith(textColour, 0.5f) : defaultColour;

		g.fillAll(c);
	}

	g.setColour(textColour);
	g.drawText(proc.getProgramName(rowNumber), Rectangle<int>{width, height}.reduced(2),
	           Justification::left, true);
}

void PluginWindow::ProgramAudioProcessorEditor::Model::selectedRowsChanged(int row)
{
	if (0 <= row)
		proc.setCurrentProgram(row);
}

//==============================================================================
String getFormatSuffix(const AudioProcessor* plugin)
{
	const auto format = [plugin]()
	{
		if (auto* instance = dynamic_cast<const AudioPluginInstance*>(plugin))
			return instance->getPluginDescription().pluginFormatName;

		return String();
	}();

	return format.isNotEmpty() ? (" (" + format + ")") : format;
}
