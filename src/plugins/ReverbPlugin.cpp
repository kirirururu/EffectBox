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

#include "ReverbPlugin.h"

ReverbPlugin::ReverbPlugin()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", AudioChannelSet::stereo())
                         .withOutput("Output", AudioChannelSet::stereo()))
{
}

String ReverbPlugin::getIdentifier()
{
	return "Reverb";
}

void ReverbPlugin::prepareToPlay(double newSampleRate, int)
{
	reverb.setSampleRate(newSampleRate);
}

void ReverbPlugin::reset()
{
	reverb.reset();
}

void ReverbPlugin::releaseResources()
{
}

void ReverbPlugin::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
	auto numChannels = buffer.getNumChannels();

	if (numChannels == 1)
		reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
	else
		reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1),
		                     buffer.getNumSamples());

	for (int ch = 2; ch < numChannels; ++ch)
		buffer.clear(ch, 0, buffer.getNumSamples());
}

const String ReverbPlugin::getName() const
{
	return getIdentifier();
}

double ReverbPlugin::getTailLengthSeconds() const
{
	return 0.0;
}

bool ReverbPlugin::acceptsMidi() const
{
	return false;
}

bool ReverbPlugin::producesMidi() const
{
	return false;
}

bool ReverbPlugin::hasEditor() const
{
	return false;
}

int ReverbPlugin::getNumPrograms()
{
	return 1;
}

int ReverbPlugin::getCurrentProgram()
{
	return 0;
}

void ReverbPlugin::setCurrentProgram(int)
{
}

const String ReverbPlugin::getProgramName(int)
{
	return {};
}

void ReverbPlugin::changeProgramName(int, const String&)
{
}

void ReverbPlugin::getStateInformation(juce::MemoryBlock&)
{
}

void ReverbPlugin::setStateInformation(const void*, int)
{
}

AudioProcessorEditor* ReverbPlugin::createEditor()
{
	return nullptr;
}
