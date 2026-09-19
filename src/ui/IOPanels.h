#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A port of an input/output endpoint in a graph editor side panel.

    It is drawn like a pin of a plugin node (with the stub pointing into the
    panel) and is the point a plugin connection will be attached to later.
*/
class IOPortComponent final : public Component, public SettableTooltipClient
{
public:
	explicit IOPortComponent(bool isInput);

	void paint(Graphics&) override;

	/** true if this port belongs to a system input (left panel), false for a
	    system output (right panel).
	*/
	bool isInput = false;

	static constexpr int portSize = 16;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOPortComponent)
};

//==============================================================================
/**
    A row of an IO side panel: the name of an input/output endpoint.
*/
class IOEndpointComponent final : public Component
{
public:
	explicit IOEndpointComponent(String name);

	void paint(Graphics&) override;
	void resized() override;

	String getEndpointName() const { return endpointName; }

private:
	const String endpointName;
	Rectangle<int> textArea;
	Font font = FontOptions{13.0f, Font::bold};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOEndpointComponent)
};

//==============================================================================
/**
    A side panel of the graph editor that lists the system inputs (placed to the
    left of the graph) or the system outputs (placed to the right of the graph).

    The component is wider than the visible panel by portOverhang pixels on the
    inner side. That strip is transparent, and since child components cannot be
    painted outside of their parent's bounds, it lets the ports, which are
    centred on the panel's inner edge, protrude into the graph area without
    being clipped.
*/
class IOPanelComponent final : public Component
{
public:
	IOPanelComponent(String title, bool isInput, const StringArray& endpointNames, int visibleWidth);

	void paint(Graphics&) override;
	void resized() override;

	/** The width of the transparent strip that overhangs the graph area. */
	static constexpr int portOverhang = IOPortComponent::portSize / 2;

	int getNumEndpoints() const { return endpoints.size(); }

	IOEndpointComponent* getEndpoint(int index) const
	{
		return isPositiveAndBelow(index, endpoints.size()) ? endpoints[index] : nullptr;
	}

	IOPortComponent* getPort(int index) const
	{
		return isPositiveAndBelow(index, ports.size()) ? ports[index] : nullptr;
	}

	bool isInputPanel() const { return isInput; }

private:
	Rectangle<int> getVisibleArea() const
	{
		return isInput ? Rectangle<int>(0, 0, visibleWidth, getHeight())
		               : Rectangle<int>(portOverhang, 0, visibleWidth, getHeight());
	}

	OwnedArray<IOEndpointComponent> endpoints;
	OwnedArray<IOPortComponent> ports;
	Label titleLabel;
	const String panelTitle;
	const bool isInput;
	const int visibleWidth;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOPanelComponent)
};
