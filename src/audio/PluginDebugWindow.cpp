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

#include "PluginDebugWindow.h"

PluginDebugWindow::PluginDebugWindow(AudioProcessor& proc)
	: AudioProcessorEditor(proc), audioProc(proc)
{
	setSize(500, 200);
	addAndMakeVisible(list);

	for (auto* p : audioProc.getParameters())
		p->addListener(this);

	log.add("Parameter debug log started");
}

PluginDebugWindow::~PluginDebugWindow()
{
	for (auto* p : audioProc.getParameters())
		p->removeListener(this);
}

void PluginDebugWindow::parameterValueChanged(int parameterIndex, float newValue)
{
	auto* param = audioProc.getParameters()[parameterIndex];
	auto value = param->getCurrentValueAsText().quoted() + " (" + String(newValue, 4) + ")";

	appendToLog("parameter change", *param, value);
}

void PluginDebugWindow::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
	auto* param = audioProc.getParameters()[parameterIndex];
	appendToLog("gesture", *param, gestureIsStarting ? "start" : "end");
}

void PluginDebugWindow::resized()
{
	list.setBounds(getLocalBounds());
}

int PluginDebugWindow::getNumRows()
{
	return log.size();
}

void PluginDebugWindow::paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool)
{
	g.setColour(getLookAndFeel().findColour(TextEditor::textColourId));

	if (isPositiveAndBelow(rowNumber, log.size()))
		g.drawText(log[rowNumber], Rectangle<int>{0, 0, width, height}, Justification::left, true);
}

void PluginDebugWindow::handleAsyncUpdate()
{
	if (log.size() > logSizeTrimThreshold)
		log.removeRange(0, log.size() - maxLogSize);

	{
		ScopedLock lock(pendingLogLock);
		log.addArray(pendingLogEntries);
		pendingLogEntries.clear();
	}

	list.updateContent();
	list.scrollToEnsureRowIsOnscreen(log.size() - 1);
}

void PluginDebugWindow::appendToLog(StringRef action, AudioProcessorParameter& param, StringRef value)
{
	String entry(action + " " + param.getName(30).quoted() + " [" +
				 String(param.getParameterIndex()) + "]: " + value);

	{
		ScopedLock lock(pendingLogLock);
		pendingLogEntries.add(entry);
	}

	triggerAsyncUpdate();
}
