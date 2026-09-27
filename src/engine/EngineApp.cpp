#include "CustomPluginScanner.h"
#include "EngineServer.h"
#include "InternalPlugins.h"

#include <memory>

namespace CommandIDs {
const int showAudioSettings = 0x30200;
} // namespace CommandIDs

extern const char* processUID;

//==============================================================================
enum class AutoScale
{
	scaled,
	unscaled,
	useDefault
};

constexpr bool autoScaleOptionAvailable =
#if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
    true;
#else
    false;
#endif

ApplicationCommandManager& getCommandManager();
ApplicationProperties& getAppProperties();
AutoScale getAutoScaleValueForPlugin(const String&);
void setAutoScaleValueForPlugin(const String&, AutoScale);
bool shouldAutoScalePlugin(const PluginDescription&);
void addPluginAutoScaleOptionsSubMenu(const AudioPluginInstance*, PopupMenu&);

//==============================================================================
class PluginScannerSubprocess final : private ChildProcessWorker, private AsyncUpdater
{
public:
	PluginScannerSubprocess() { addDefaultFormatsToManager(formatManager); }

	using ChildProcessWorker::initialiseFromCommandLine;

	void handleMessageFromCoordinator(const MemoryBlock& mb) override
	{
		if (mb.isEmpty())
			return;

		const std::lock_guard<std::mutex> lock(mutex);

		if (const auto results = doScan(mb); !results.isEmpty())
		{
			sendResults(results);
		}
		else
		{
			pendingBlocks.emplace(mb);
			triggerAsyncUpdate();
		}
	}

	void handleConnectionLost() override { JUCEApplicationBase::quit(); }

	void handleAsyncUpdate() override
	{
		for (;;)
		{
			const std::lock_guard<std::mutex> lock(mutex);

			if (pendingBlocks.empty())
				return;

			sendResults(doScan(pendingBlocks.front()));
			pendingBlocks.pop();
		}
	}

private:
	OwnedArray<PluginDescription> doScan(const MemoryBlock& block) const
	{
		MemoryInputStream stream{block, false};
		const auto formatName = stream.readString();
		const auto identifier = stream.readString();

		PluginDescription pd;
		pd.fileOrIdentifier = identifier;
		pd.uniqueId = pd.deprecatedUid = 0;

		const auto matchingFormat = [&]() -> AudioPluginFormat*
		{
			for (auto* format : formatManager.getFormats())
				if (format->getName() == formatName)
					return format;

			return nullptr;
		}();

		OwnedArray<PluginDescription> results;

		if (matchingFormat != nullptr &&
		    (MessageManager::getInstance()->isThisTheMessageThread() ||
		     matchingFormat->requiresUnblockedMessageThreadDuringCreation(pd)))
		{
			matchingFormat->findAllTypesForFile(results, identifier);
		}

		return results;
	}

	void sendResults(const OwnedArray<PluginDescription>& results)
	{
		XmlElement xml("LIST");

		for (const auto& desc : results)
			xml.addChildElement(desc->createXml().release());

		const auto str = xml.toString();
		sendMessageToCoordinator({str.toRawUTF8(), str.getNumBytesAsUTF8()});
	}

	std::mutex mutex;
	std::queue<MemoryBlock> pendingBlocks;
	AudioPluginFormatManager formatManager;
};

//==============================================================================
class EngineCommands final : public ApplicationCommandTarget
{
public:
	explicit EngineCommands(AudioDeviceManager& dm) : deviceManager(dm) { }

	ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }

	void getAllCommands(Array<CommandID>& commands) override
	{
		commands.add(CommandIDs::showAudioSettings);
	}

	void getCommandInfo(const CommandID commandID, ApplicationCommandInfo& result) override
	{
		if (commandID == CommandIDs::showAudioSettings)
		{
			result.setInfo("Change the Audio Device Settings", {}, "General", 0);
			result.addDefaultKeypress('a', ModifierKeys::commandModifier);
		}
	}

	bool perform(const InvocationInfo& info) override
	{
		if (info.commandID != CommandIDs::showAudioSettings)
			return false;

		auto* selector =
		    new AudioDeviceSelectorComponent(deviceManager, 0, 256, 0, 256, true, true, true, false);
		selector->setSize(500, 450);

		DialogWindow::LaunchOptions options;
		options.content.setOwned(selector);
		options.dialogTitle = "Audio Settings";
		options.dialogBackgroundColour =
		    LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
		options.escapeKeyTriggersCloseButton = true;
		options.useNativeTitleBar = false;
		options.resizable = false;

		options.create()->enterModalState(
		    true,
		    ModalCallbackFunction::create(
		        [this](int)
		        {
			        getAppProperties().getUserSettings()->setValue(
				    "audioDeviceState", deviceManager.createStateXml().get());
			        getAppProperties().getUserSettings()->saveIfNeeded();
		        }),
		    true);

		return true;
	}

private:
	AudioDeviceManager& deviceManager;

	JUCE_DECLARE_NON_COPYABLE(EngineCommands)
};

//==============================================================================
class EngineApp final : public JUCEApplication
{
public:
	void initialise(const String& commandLine) override
	{
		auto scannerSubprocess = std::make_unique<PluginScannerSubprocess>();

		if (scannerSubprocess->initialiseFromCommandLine(commandLine, processUID))
		{
			storedScannerSubprocess = std::move(scannerSubprocess);
			return;
		}

		PropertiesFile::Options options;
		options.applicationName = "EffectBox Engine";
		options.filenameSuffix = "settings";
		options.osxLibrarySubFolder = "Preferences";

		appProperties = std::make_unique<ApplicationProperties>();
		appProperties->setStorageParameters(options);

		addDefaultFormatsToManager(formatManager);
		formatManager.addFormat(std::make_unique<InternalPluginFormat>());

		for (auto* format : formatManager.getFormats())
			if (auto* settings = getAppProperties().getUserSettings())
				format->searchPathsForPlugins(
				    PluginListComponent::getLastSearchPath(*settings, *format), false, false);

		RuntimePermissions::request(
		    RuntimePermissions::recordAudio,
		    [this](bool granted)
		    {
			    auto savedState =
			        getAppProperties().getUserSettings()->getXmlValue("audioDeviceState");
			    deviceManager.initialise(granted ? 256 : 0, 256, savedState.get(), true);
		    });

		deviceManager.addChangeListener(&deviceWatcher);

		knownPluginList.setCustomScanner(std::make_unique<CustomPluginScanner>());

		graph.reset(new PluginGraph(formatManager, knownPluginList));
		graphPlayer = std::make_unique<AudioProcessorPlayer>(getAppProperties().getUserSettings()
			    ->getBoolValue("doublePrecisionProcessing", false));
		graphPlayer->setProcessor(&graph->graph);

		deviceManager.addAudioCallback(graphPlayer.get());
		deviceManager.addMidiInputDeviceCallback({}, &graphPlayer->getMidiMessageCollector());
		updateMidiOutput();

		server = std::make_unique<EngineServer>(*graph, *graphPlayer, knownPluginList, formatManager);

		graph->onPluginCreateFailed = [this](const PluginDescription& description, const String& error)
		{ server->sendPluginCreateFailed(description, error); };

		if (auto savedPluginList = getAppProperties().getUserSettings()->getXmlValue("pluginList"))
			knownPluginList.recreateFromXml(*savedPluginList);

		InternalPluginFormat internalFormat;
		for (const auto& t : internalFormat.getAllTypes())
			knownPluginList.addType(t);

		commandManager.registerAllCommandsForTarget(&engineCommands);

		// defer graph seeding so plugin instantiation happens in the normal
		// event loop, not during initialisation
		MessageManager::callAsync([this] { graph->newDocument(); });
	}

	void shutdown() override
	{
		deviceManager.removeAudioCallback(graphPlayer.get());
		deviceManager.removeMidiInputDeviceCallback({}, &graphPlayer->getMidiMessageCollector());
		deviceManager.removeChangeListener(&deviceWatcher);

		graph = nullptr;

		if (auto* settings = getAppProperties().getUserSettings())
		{
			settings->setValue("audioDeviceState", deviceManager.createStateXml().get());
			settings->saveIfNeeded();
		}

		appProperties = nullptr;
	}

	bool moreThanOneInstanceAllowed() override { return true; }

	const String getApplicationName() override { return "EffectBox Engine"; }
	const String getApplicationVersion() override { return "0.1.0"; }

	ApplicationCommandManager commandManager;
	std::unique_ptr<ApplicationProperties> appProperties;

private:
	void updateMidiOutput()
	{
		if (auto* output = deviceManager.getDefaultMidiOutput())
		{
			output->startBackgroundThread();
			graphPlayer->setMidiOutput(output);
		}
	}

	AudioDeviceManager deviceManager;
	AudioPluginFormatManager formatManager;
	KnownPluginList knownPluginList;
	std::unique_ptr<AudioProcessorPlayer> graphPlayer;

	std::unique_ptr<PluginGraph> graph;
	std::unique_ptr<EngineServer> server;
	std::unique_ptr<PluginScannerSubprocess> storedScannerSubprocess;

	class DeviceWatcher final : public ChangeListener
	{
	public:
		explicit DeviceWatcher(EngineApp& app) : owner(app) { }
		void changeListenerCallback(ChangeBroadcaster*) override { owner.updateMidiOutput(); }

	private:
		EngineApp& owner;
	} deviceWatcher{*this};

	EngineCommands engineCommands{deviceManager};
};

//==============================================================================
ApplicationCommandManager& getCommandManager()
{
	return static_cast<EngineApp&>(*JUCEApplication::getInstance()).commandManager;
}

ApplicationProperties& getAppProperties()
{
	return *static_cast<EngineApp&>(*JUCEApplication::getInstance()).appProperties;
}

//==============================================================================
static AutoScale autoScaleFromString(StringRef str)
{
	if (str.isEmpty())
		return AutoScale::useDefault;
	if (str == CharPointer_ASCII{"0"})
		return AutoScale::scaled;
	if (str == CharPointer_ASCII{"1"})
		return AutoScale::unscaled;

	jassertfalse;
	return AutoScale::useDefault;
}

static const char* autoScaleToString(AutoScale autoScale)
{
	if (autoScale == AutoScale::scaled)
		return "0";
	if (autoScale == AutoScale::unscaled)
		return "1";

	return {};
}

AutoScale getAutoScaleValueForPlugin(const String& identifier)
{
	if (identifier.isNotEmpty())
	{
		auto plugins = StringArray::fromLines(
		    getAppProperties().getUserSettings()->getValue("autoScalePlugins"));
		plugins.removeEmptyStrings();

		for (auto& plugin : plugins)
		{
			auto fromIdentifier = plugin.fromFirstOccurrenceOf(identifier, false, false);

			if (fromIdentifier.isNotEmpty())
				return autoScaleFromString(
				    fromIdentifier.fromFirstOccurrenceOf(":", false, false));
		}
	}

	return AutoScale::useDefault;
}

void setAutoScaleValueForPlugin(const String& identifier, AutoScale s)
{
	auto plugins =
	    StringArray::fromLines(getAppProperties().getUserSettings()->getValue("autoScalePlugins"));
	plugins.removeEmptyStrings();

	const auto index = [identifier, plugins]
	{
		const auto it = std::ranges::find_if(plugins, [&](const String& str)
		                                     { return str.startsWith(identifier); });

		return static_cast<int>(std::distance(plugins.begin(), it));
	}();

	if (s == AutoScale::useDefault && index != plugins.size())
	{
		plugins.remove(index);
	}
	else
	{
		auto str = identifier + ":" + autoScaleToString(s);

		if (index != plugins.size())
			plugins.getReference(index) = str;
		else
			plugins.add(str);
	}

	getAppProperties().getUserSettings()->setValue("autoScalePlugins",
	                                               plugins.joinIntoString("\n"));
}

static bool isAutoScaleAvailableForPlugin(const PluginDescription& description)
{
	return autoScaleOptionAvailable
	       && (description.pluginFormatName.containsIgnoreCase("VST")
	           || description.pluginFormatName.containsIgnoreCase("LV2"));
}

bool shouldAutoScalePlugin(const PluginDescription& description)
{
	if (!isAutoScaleAvailableForPlugin(description))
		return false;

	const auto scaleValue = getAutoScaleValueForPlugin(description.fileOrIdentifier);

	return (scaleValue == AutoScale::scaled
	        || (scaleValue == AutoScale::useDefault
	            && getAppProperties().getUserSettings()->getBoolValue("autoScalePluginWindows")));
}

void addPluginAutoScaleOptionsSubMenu(const AudioPluginInstance* pluginInstance,
                                      PopupMenu& menu)
{
	if (pluginInstance == nullptr)
		return;

	auto description = pluginInstance->getPluginDescription();

	if (!isAutoScaleAvailableForPlugin(description))
		return;

	auto identifier = description.fileOrIdentifier;

	PopupMenu autoScaleMenu;

	autoScaleMenu.addItem(
	    "Default", true, getAutoScaleValueForPlugin(identifier) == AutoScale::useDefault,
	    [identifier] { setAutoScaleValueForPlugin(identifier, AutoScale::useDefault); });

	autoScaleMenu.addItem("Enabled", true,
	                      getAutoScaleValueForPlugin(identifier) == AutoScale::scaled,
	                      [identifier] { setAutoScaleValueForPlugin(identifier, AutoScale::scaled); });

	autoScaleMenu.addItem("Disabled", true,
	                      getAutoScaleValueForPlugin(identifier) == AutoScale::unscaled,
	                      [identifier]
	                      { setAutoScaleValueForPlugin(identifier, AutoScale::unscaled); });

	menu.addSubMenu("Auto-scale window", autoScaleMenu);
}

START_JUCE_APPLICATION(EngineApp)
