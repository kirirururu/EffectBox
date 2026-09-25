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

#include <JuceHeader.h>

class SineWaveSynth final : public AudioProcessor
{
public:
	SineWaveSynth();
	static String getIdentifier();

	void prepareToPlay(double newSampleRate, int) override;
	void releaseResources() override;

	void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

	using AudioProcessor::processBlock;

	const String getName() const override;
	double getTailLengthSeconds() const override;
	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool hasEditor() const override;
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram(int) override;
	const String getProgramName(int) override;
	void changeProgramName(int, const String&) override;
	void getStateInformation(juce::MemoryBlock&) override;
	void setStateInformation(const void*, int) override;

private:
	struct SineWaveSound;
	struct SineWaveVoice;

	AudioProcessorEditor* createEditor() override;

	Synthesiser synth;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SineWaveSynth)
};
