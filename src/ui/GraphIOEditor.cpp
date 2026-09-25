#include <JuceHeader.h>

#include "GraphIOEditor.h"

//==============================================================================
class GraphIOEditor::IOListModel final : public ListBoxModel
{
public:
	IOListModel(GraphIOEditor& o, bool in) : owner(o), isInput(in)
	{
	}

	int getNumRows() override { return static_cast<int>(owner.endpoints(isInput).size()); }

	void paintListBoxItem(int row, Graphics& g, int width, int height, bool rowIsSelected) override
	{
		const auto& eps = owner.endpoints(isInput);

		if (!isPositiveAndBelow(row, eps.size()))
			return;

		g.fillAll(rowIsSelected ? Colour(0xff3a6ea5) : Colour(0xff23272e));
		g.setColour(rowIsSelected ? Colours::white : Colours::grey);
		g.setFont(FontOptions(13.0f, Font::plain));

		const auto& e = eps[static_cast<size_t>(row)];
		const auto text = e.name + "   (" + (e.numChannels == 2 ? "Stereo" : "Mono") + ")";
		g.drawText(text, 6, 0, width - 12, height, Justification::centredLeft);

		g.setColour(Colours::black.withAlpha(0.4f));
		g.fillRect(0, height - 1, width, 1);
	}

	void selectedRowsChanged(int lastRowSelected) override { owner.rowSelected(isInput, lastRowSelected); }

private:
	GraphIOEditor& owner;
	const bool isInput;

	JUCE_DECLARE_NON_COPYABLE(IOListModel)
};

//==============================================================================
static void selectSingleRow(ListBox& list, int row)
{
	if (row < 0)
		return;

	SparseSet<int> rows;
	rows.addRange(Range<int>{row, row + 1});
	list.setSelectedRows(rows, NotificationType::dontSendNotification);
}

//==============================================================================
GraphIOEditor::GraphIOEditor(PluginGraph& g) : graph(g)
{
	inputModel.reset(new IOListModel(*this, true));
	outputModel.reset(new IOListModel(*this, false));

	inputList.setModel(inputModel.get());
	inputList.setRowHeight(26);
	addAndMakeVisible(inputList);

	outputList.setModel(outputModel.get());
	outputList.setRowHeight(26);
	addAndMakeVisible(outputList);

	inputTitle.setText("Inputs", NotificationType::dontSendNotification);
	outputTitle.setText("Outputs", NotificationType::dontSendNotification);
	inputTitle.setFont(FontOptions(13.0f, Font::bold));
	outputTitle.setFont(FontOptions(13.0f, Font::bold));
	addAndMakeVisible(inputTitle);
	addAndMakeVisible(outputTitle);

	nameLabel.setText("Name", NotificationType::dontSendNotification);
	channelsLabel.setText("Channels", NotificationType::dontSendNotification);
	addAndMakeVisible(nameLabel);
	addAndMakeVisible(channelsLabel);

	nameEditor.setText("", NotificationType::dontSendNotification);
	nameEditor.addListener(this);
	addAndMakeVisible(nameEditor);

	channelsCombo.addItem("Mono", 1);
	channelsCombo.addItem("Stereo", 2);
	channelsCombo.setSelectedItemIndex(0, NotificationType::dontSendNotification);
	channelsCombo.addListener(this);
	addAndMakeVisible(channelsCombo);

	addInputButton.setButtonText("Add Input");
	addOutputButton.setButtonText("Add Output");
	removeButton.setButtonText("Remove");
	addInputButton.addListener(this);
	addOutputButton.addListener(this);
	removeButton.addListener(this);
	addAndMakeVisible(addInputButton);
	addAndMakeVisible(addOutputButton);
	addAndMakeVisible(removeButton);
	removeButton.setEnabled(false);

	setSize(540, 420);

	if (!graph.inputs.empty())
	{
		selectSingleRow(inputList, 0);
		rowSelected(true, 0);
	}
}

//==============================================================================
void GraphIOEditor::rowSelected(bool isInput, int index)
{
	currentIsInput = isInput;
	currentIndex = index;
	removeButton.setEnabled(index >= 0);
	updateEditingControls();
}

void GraphIOEditor::updateEditingControls()
{
	const auto& eps = endpoints(currentIsInput);

	if (!isPositiveAndBelow(currentIndex, eps.size()))
	{
		nameEditor.setText("", NotificationType::dontSendNotification);
		channelsCombo.setSelectedItemIndex(-1, NotificationType::dontSendNotification);
		return;
	}

	const auto& e = eps[static_cast<size_t>(currentIndex)];

	updatingUI = true;
	nameEditor.setText(e.name, NotificationType::dontSendNotification);
	channelsCombo.setSelectedItemIndex(e.numChannels == 2 ? 1 : 0,
	                                   NotificationType::dontSendNotification);
	updatingUI = false;
}

void GraphIOEditor::addEndpoint(bool isInput)
{
	const int count = static_cast<int>(endpoints(isInput).size());
	const String name = (isInput ? "Input " : "Output ") + String(count + 1);

	graph.addIOEndpoint(name, 1, isInput);

	auto* lb = isInput ? &inputList : &outputList;
	lb->updateContent();
	selectSingleRow(*lb, count);
	rowSelected(isInput, count);

	nameEditor.grabKeyboardFocus();
	nameEditor.selectAll();
}

void GraphIOEditor::removeSelected()
{
	const auto& eps = endpoints(currentIsInput);

	if (!isPositiveAndBelow(currentIndex, eps.size()))
		return;

	const Uuid id = eps[static_cast<size_t>(currentIndex)].id;
	graph.removeIOEndpoint(id, currentIsInput);

	auto* lb = currentIsInput ? &inputList : &outputList;
	lb->updateContent();

	const int newSize = static_cast<int>(endpoints(currentIsInput).size());
	const int newIndex = jmin(currentIndex, newSize - 1);

	if (newSize == 0)
		rowSelected(currentIsInput, -1);
	else
	{
		selectSingleRow(*lb, newIndex);
		rowSelected(currentIsInput, newIndex);
	}
}

//==============================================================================
void GraphIOEditor::textEditorTextChanged(TextEditor& e)
{
	if (&e != &nameEditor || updatingUI)
		return;

	const auto& eps = endpoints(currentIsInput);

	if (!isPositiveAndBelow(currentIndex, eps.size()))
		return;

	const Uuid id = eps[static_cast<size_t>(currentIndex)].id;
	graph.setIOEndpointName(id, nameEditor.getText(), currentIsInput);
	(currentIsInput ? inputList : outputList).repaint();
}

void GraphIOEditor::comboBoxChanged(ComboBox* c)
{
	if (c != &channelsCombo || updatingUI)
		return;

	const auto& eps = endpoints(currentIsInput);

	if (!isPositiveAndBelow(currentIndex, eps.size()))
		return;

	const int numChannels = (channelsCombo.getSelectedItemIndex() == 1) ? 2 : 1;
	const Uuid id = eps[static_cast<size_t>(currentIndex)].id;
	graph.setIOEndpointChannels(id, numChannels, currentIsInput);
	(currentIsInput ? inputList : outputList).repaint();
}

void GraphIOEditor::buttonClicked(Button* b)
{
	if (b == &addInputButton)
		addEndpoint(true);
	else if (b == &addOutputButton)
		addEndpoint(false);
	else if (b == &removeButton)
		removeSelected();
}

//==============================================================================
void GraphIOEditor::resized()
{
	auto r = getLocalBounds().reduced(12);

	auto topRow = r.removeFromTop(30);
	addInputButton.setBounds(topRow.removeFromLeft(120));
	addOutputButton.setBounds(topRow.removeFromRight(120));

	auto bottomRow = r.removeFromBottom(40);
	nameLabel.setBounds(bottomRow.removeFromLeft(44).reduced(0, 4));
	nameEditor.setBounds(bottomRow.removeFromLeft(200).reduced(4, 2));
	channelsLabel.setBounds(bottomRow.removeFromLeft(70).reduced(0, 4));
	channelsCombo.setBounds(bottomRow.removeFromLeft(90).reduced(4, 2));
	removeButton.setBounds(bottomRow.removeFromRight(90).reduced(0, 2));

	auto left = r.removeFromLeft(r.getWidth() / 2 - 6);
	inputTitle.setBounds(left.removeFromTop(18));
	inputList.setBounds(left);

	auto right = r;
	outputTitle.setBounds(right.removeFromTop(18));
	outputList.setBounds(right);
}
