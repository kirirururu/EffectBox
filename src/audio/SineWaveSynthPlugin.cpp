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

#include "SineWaveSynthPlugin.h"

struct SineWaveSynth::SineWaveSound final : public SynthesiserSound
{
	SineWaveSound() = default;

	bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
	bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

struct SineWaveSynth::SineWaveVoice final : public SynthesiserVoice
{
	SineWaveVoice() = default;

	bool canPlaySound(SynthesiserSound* sound) override
	{
		return dynamic_cast<SineWaveSound*>(sound) != nullptr;
	}

	void startNote(int midiNoteNumber,
	               float velocity,
	               SynthesiserSound* /*sound*/,
	               int /*currentPitchWheelPosition*/) override
	{
		currentAngle = 0.0;
		level = velocity * 0.15;
		tailOff = 0.0;

		double cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
		double cyclesPerSample = cyclesPerSecond / getSampleRate();

		angleDelta = cyclesPerSample * 2.0 * MathConstants<double>::pi;
	}

	void stopNote(float /*velocity*/, bool allowTailOff) override
	{
		if (allowTailOff)
		{
			// start a tail-off by setting this flag. The render callback will pick up on
			// this and do a fade out, calling clearCurrentNote() when it's finished.

			if (approximatelyEqual(
			        tailOff, 0.0)) // we only need to begin a tail-off if it's not already doing
			                       // so - the stopNote method could be called more than once.
				tailOff = 1.0;
		}
		else
		{
			// we're being told to stop playing immediately, so reset everything..

			clearCurrentNote();
			angleDelta = 0.0;
		}
	}

	void pitchWheelMoved(int /*newValue*/) override
	{
		// not implemented for the purposes of this demo!
	}

	void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override
	{
		// not implemented for the purposes of this demo!
	}

	void renderNextBlock(AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
	{
		if (!approximatelyEqual(angleDelta, 0.0))
		{
			if (tailOff > 0)
			{
				while (--numSamples >= 0)
				{
					const auto currentSample =
					    static_cast<float>(sin(currentAngle) * level * tailOff);

					for (int i = outputBuffer.getNumChannels(); --i >= 0;)
						outputBuffer.addSample(i, startSample, currentSample);

					currentAngle += angleDelta;
					++startSample;

					tailOff *= 0.99;

					if (tailOff <= 0.005)
					{
						// tells the synth that this voice has stopped
						clearCurrentNote();

						angleDelta = 0.0;
						break;
					}
				}
			}
			else
			{
				while (--numSamples >= 0)
				{
					const auto currentSample = static_cast<float>(sin(currentAngle) * level);

					for (int i = outputBuffer.getNumChannels(); --i >= 0;)
						outputBuffer.addSample(i, startSample, currentSample);

					currentAngle += angleDelta;
					++startSample;
				}
			}
		}
	}

	using SynthesiserVoice::renderNextBlock;

private:
	double currentAngle = 0, angleDelta = 0, level = 0, tailOff = 0;
};

//==============================================================================

SineWaveSynth::SineWaveSynth()
    : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo()))
{
	const int numVoices = 8;

	// Add some voices...
	for (int i = numVoices; --i >= 0;)
		synth.addVoice(new SineWaveVoice());

	// ..and give the synth a sound to play
	synth.addSound(new SineWaveSound());
}

String SineWaveSynth::getIdentifier()
{
	return "Sine Wave Synth";
}

void SineWaveSynth::prepareToPlay(double newSampleRate, int)
{
	synth.setCurrentPlaybackSampleRate(newSampleRate);
}

void SineWaveSynth::releaseResources()
{
}

void SineWaveSynth::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
	const int numSamples = buffer.getNumSamples();

	buffer.clear();
	synth.renderNextBlock(buffer, midiMessages, 0, numSamples);
	buffer.applyGain(0.8f);
}

const String SineWaveSynth::getName() const
{
	return getIdentifier();
}

double SineWaveSynth::getTailLengthSeconds() const
{
	return 0.0;
}

bool SineWaveSynth::acceptsMidi() const
{
	return true;
}

bool SineWaveSynth::producesMidi() const
{
	return true;
}

bool SineWaveSynth::hasEditor() const
{
	return false;
}

int SineWaveSynth::getNumPrograms()
{
	return 1;
}

int SineWaveSynth::getCurrentProgram()
{
	return 0;
}

void SineWaveSynth::setCurrentProgram(int)
{
}

const String SineWaveSynth::getProgramName(int)
{
	return {};
}
void SineWaveSynth::changeProgramName(int, const String&)
{
}

void SineWaveSynth::getStateInformation(juce::MemoryBlock&)
{
}

void SineWaveSynth::setStateInformation(const void*, int)
{
}

AudioProcessorEditor* SineWaveSynth::createEditor()
{
	return nullptr;
}
