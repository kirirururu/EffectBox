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

#include "CustomPluginScanner.h"

class Superprocess final : private ChildProcessCoordinator
{
public:
	Superprocess()
	{
		launchWorkerProcess(File::getSpecialLocation(File::currentExecutableFile), processUID, 0, 0);
	}

	enum class State
	{
		timeout,
		gotResult,
		connectionLost,
	};

	struct Response
	{
		State state;
		std::unique_ptr<XmlElement> xml;
	};

	Response getResponse()
	{
		std::unique_lock<std::mutex> lock{mutex};

		if (!condvar.wait_for(lock, std::chrono::milliseconds{50},
							  [&] { return gotResult || connectionLost; }))
			return {State::timeout, nullptr};

		const auto state = connectionLost ? State::connectionLost : State::gotResult;
		connectionLost = false;
		gotResult = false;

		return {state, std::move(pluginDescription)};
	}

	using ChildProcessCoordinator::sendMessageToWorker;

	void handleMessageFromWorker(const MemoryBlock& mb) override
	{
		const std::lock_guard<std::mutex> lock{mutex};
		pluginDescription = parseXML(mb.toString());
		gotResult = true;
		condvar.notify_one();
	}

	void handleConnectionLost() override
	{
		const std::lock_guard<std::mutex> lock{mutex};
		connectionLost = true;
		condvar.notify_one();
	}

private:
	std::mutex mutex;
	std::condition_variable condvar;

	std::unique_ptr<XmlElement> pluginDescription;
	bool connectionLost = false;
	bool gotResult = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Superprocess)
};

//==============================================================================
CustomPluginScanner::CustomPluginScanner()
{
	if (auto* file = getAppProperties().getUserSettings())
		file->addChangeListener(this);

	handleChange();
}

CustomPluginScanner::~CustomPluginScanner()
{
	if (auto* file = getAppProperties().getUserSettings())
		file->removeChangeListener(this);
}

bool CustomPluginScanner::findPluginTypesFor(AudioPluginFormat& format,
                                             OwnedArray<PluginDescription>& result,
                                             const String& fileOrIdentifier)
{
	if (scanInProcess)
	{
		superprocess = nullptr;
		format.findAllTypesForFile(result, fileOrIdentifier);
		return true;
	}

	if (addPluginDescriptions(format.getName(), fileOrIdentifier, result))
		return true;

	superprocess = nullptr;
	return false;
}

void CustomPluginScanner::scanFinished()
{
	superprocess = nullptr;
}

void CustomPluginScanner::changeListenerCallback(ChangeBroadcaster*)
{
	handleChange();
}

bool CustomPluginScanner::addPluginDescriptions(const String& formatName,
                                                const String& fileOrIdentifier,
                                                OwnedArray<PluginDescription>& result)
{
	if (superprocess == nullptr)
		superprocess = std::make_unique<Superprocess>();

	MemoryBlock block;
	MemoryOutputStream stream{block, true};
	stream.writeString(formatName);
	stream.writeString(fileOrIdentifier);

	if (!superprocess->sendMessageToWorker(block))
		return false;

	for (;;)
	{
		if (shouldExit())
			return true;

		const auto response = superprocess->getResponse();

		if (response.state == Superprocess::State::timeout)
			continue;

		if (response.xml != nullptr)
		{
			for (const auto* item : response.xml->getChildIterator())
			{
				auto desc = std::make_unique<PluginDescription>();

				if (desc->loadFromXml(*item))
					result.add(std::move(desc));
			}
		}

		return (response.state == Superprocess::State::gotResult);
	}
}

void CustomPluginScanner::handleChange()
{
	if (auto* file = getAppProperties().getUserSettings())
		scanInProcess = (file->getIntValue(scanModeKey) == 0);
}
