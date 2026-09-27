#include "MainHostWindow.h"

#include <memory>

//==============================================================================
class GuiApp final : public JUCEApplication
{
public:
	void initialise(const String& /*commandLine*/) override
	{
		PropertiesFile::Options options;
		options.applicationName = "EffectBox UI";
		options.filenameSuffix = "settings";
		options.osxLibrarySubFolder = "Preferences";

		appProperties = std::make_unique<ApplicationProperties>();
		appProperties->setStorageParameters(options);

		engineClient = std::make_unique<EngineClient>();

		// TODO: application crashes when showing error message
		engineClient->onPluginCreateFailed = [](const proto::PluginDescription& plugin, const String& error)
		{
			AlertWindow::showMessageBoxAsync(
			    MessageBoxIconType::WarningIcon,
			    TRANS("Couldn't create plugin") + " " + String{plugin.name()},
			    error,
			    "OK");
		};
		engineClient->onEngineLost = []()
		{
			AlertWindow::showMessageBoxAsync(
			    MessageBoxIconType::WarningIcon,
			    TRANS("The audio engine has stopped"),
			    TRANS("The audio engine has stopped"),
			    "OK");
		};

		if (!engineClient->start())
		{
			AlertWindow::showMessageBoxAsync(
				MessageBoxIconType::WarningIcon,
				TRANS("Couldn't start the audio engine"),
			    TRANS("Close the application and start it again."),
			    TRANS("Exit"));
			quit();
			return;
		}

		mainWindow = std::make_unique<MainHostWindow>(*engineClient);

		commandManager.registerAllCommandsForTarget(this);
		commandManager.registerAllCommandsForTarget(mainWindow.get());

		mainWindow->menuItemsChanged();
	}

	void shutdown() override
	{
		mainWindow = nullptr;
		engineClient = nullptr;
		appProperties = nullptr;
	}

	void systemRequestedQuit() override
	{
		if (mainWindow != nullptr)
			mainWindow->tryToQuitApplication();
		else
			JUCEApplicationBase::quit();
	}

	const String getApplicationName() override { return "Juce Plug-In Host"; }
	const String getApplicationVersion() override { return "0.1.0"; }
	bool moreThanOneInstanceAllowed() override { return true; }

	ApplicationCommandManager commandManager;
	std::unique_ptr<ApplicationProperties> appProperties;

private:
	std::unique_ptr<MainHostWindow> mainWindow;
	std::unique_ptr<EngineClient> engineClient;
};

static GuiApp& getApp()
{
	return *dynamic_cast<GuiApp*>(JUCEApplication::getInstance());
}

ApplicationProperties& getAppProperties()
{
	return *getApp().appProperties;
}

ApplicationCommandManager& getCommandManager()
{
	return getApp().commandManager;
}

// This kicks the whole thing off..
START_JUCE_APPLICATION(GuiApp)
