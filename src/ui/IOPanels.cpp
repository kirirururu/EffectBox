#include <JuceHeader.h>

#include "IOPanels.h"

//==============================================================================
IOPortComponent::IOPortComponent(bool in) : isInput(in)
{
	setSize(portSize, portSize);
}

void IOPortComponent::paint(Graphics& g)
{
	auto w = static_cast<float>(getWidth());
	auto h = static_cast<float>(getHeight());

	Path p;
	p.addEllipse(w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);

	// The stub points into the panel the port is attached to, the same way a pin
	// of a plugin node points into the node: for the input panel (left of the
	// graph) the stub is on the left side, for the output panel (right) on the
	// right side.
	p.addRectangle(isInput ? 0.0f : 0.5f * w, h * 0.4f, w * 0.5f, h * 0.2f);

	g.setColour(Colours::green);
	g.fillPath(p);
}

//==============================================================================
IOEndpointComponent::IOEndpointComponent(String name) : endpointName(std::move(name))
{
	setSize(120, 36);
}

void IOEndpointComponent::paint(Graphics& g)
{
	g.setColour(findColour(TextEditor::textColourId));
	g.setFont(font);
	g.drawFittedText(endpointName, textArea, Justification::centred, 2);

	g.setColour(Colours::black.withAlpha(0.4f));
	g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void IOEndpointComponent::resized()
{
	textArea = getLocalBounds().reduced(4, 0);
}

//==============================================================================
IOPanelComponent::IOPanelComponent(String title, bool in, const StringArray& names, int visibleWidthIn)
    : panelTitle(std::move(title)), isInput(in), visibleWidth(visibleWidthIn)
{
	titleLabel.setText(panelTitle, NotificationType::dontSendNotification);
	titleLabel.setJustificationType(Justification::centred);
	addAndMakeVisible(titleLabel);

	for (const auto& name : names)
	{
		addAndMakeVisible(endpoints.add(new IOEndpointComponent(name)));

		auto* port = ports.add(new IOPortComponent(in));
		port->setTooltip(name + (in ? " (input)" : " (output)"));
		addAndMakeVisible(port);
	}

	setSize(visibleWidth + portOverhang, 100);
}

void IOPanelComponent::paint(Graphics& g)
{
	// Only the visible part of the panel is painted; the strip that overhangs
	// the graph area stays transparent so that the ports can protrude through
	// it.
	const auto visibleArea = getVisibleArea();

	g.setColour(getLookAndFeel().findColour(ResizableWindow::backgroundColourId).darker());
	g.fillRect(visibleArea);

	g.setColour(Colours::black.withAlpha(0.4f));
	g.fillRect(isInput ? (visibleWidth - 1) : portOverhang, 0, 1, getHeight());
}

void IOPanelComponent::resized()
{
	auto visibleArea = getVisibleArea();

	titleLabel.setFont(FontOptions(14.0f, Font::bold));
	titleLabel.setBounds(visibleArea.removeFromTop(32));

	if (endpoints.size() > 0)
	{
		const auto itemHeight = jmin(36, visibleArea.getHeight() / endpoints.size());

		for (int i = 0; i < endpoints.size(); ++i)
		{
			const auto itemArea = visibleArea.removeFromTop(itemHeight);
			const int edgeX = isInput ? visibleWidth : portOverhang;

			endpoints[i]->setBounds(itemArea);

			ports[i]->setBounds(edgeX - IOPortComponent::portSize / 2,
			                    itemArea.getY() +
			                        (itemArea.getHeight() - IOPortComponent::portSize) / 2,
			                    IOPortComponent::portSize, IOPortComponent::portSize);
		}
	}
}
