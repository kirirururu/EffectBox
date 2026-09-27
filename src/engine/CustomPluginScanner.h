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

// #include "MainHostWindow.h"

#include <juce_audio_processors/juce_audio_processors.h>

using namespace juce;

class Superprocess;

constexpr const char* scanModeKey = "pluginScanMode";

class CustomPluginScanner final : public KnownPluginList::CustomScanner, private ChangeListener
{
public:
	CustomPluginScanner();

	~CustomPluginScanner() override;

	bool findPluginTypesFor(AudioPluginFormat& format,
	                        OwnedArray<PluginDescription>& result,
	                        const String& fileOrIdentifier) override;

	void scanFinished() override;

	void changeListenerCallback(ChangeBroadcaster*) override;

private:
	/*  Scans for a plugin with format 'formatName' and ID 'fileOrIdentifier' using a subprocess,
	    and adds discovered plugin descriptions to 'result'.

	    Returns true on success.

	    Failure indicates that the subprocess is unrecoverable and should be terminated.
	*/
	bool addPluginDescriptions(const String& formatName,
	                           const String& fileOrIdentifier,
	                           OwnedArray<PluginDescription>& result);

	void handleChange();

	std::unique_ptr<Superprocess> superprocess;

	std::atomic<bool> scanInProcess{true};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomPluginScanner)
};
