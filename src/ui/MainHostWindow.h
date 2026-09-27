#pragma once

#include "EngineClient.h"
#include "GraphEditorPanel.h"

#include <vector>

using namespace juce;

//==============================================================================
namespace CommandIDs {
static const int showPluginListEditor = 0x30100;
static const int aboutBox = 0x30300;
static const int allWindowsForward = 0x30400;
static const int showGraphIO = 0x30700;
} // namespace CommandIDs

//==============================================================================
ApplicationCommandManager& getCommandManager();
ApplicationProperties& getAppProperties();

//==============================================================================
class MainHostWindow final : public DocumentWindow,
                             public MenuBarModel,
                             public ApplicationCommandTarget,
                             public ChangeListener
{
public:
	//==============================================================================
	explicit MainHostWindow(EngineClient& client);
	~MainHostWindow() override;

	//==============================================================================
	void closeButtonPressed() override;
	void changeListenerCallback(ChangeBroadcaster*) override;

	void menuBarActivated(bool isActive) override;

	StringArray getMenuBarNames() override;
	PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName) override;
	void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;
	ApplicationCommandTarget* getNextCommandTarget() override;
	void getAllCommands(Array<CommandID>&) override;
	void getCommandInfo(CommandID, ApplicationCommandInfo&) override;
	bool perform(const InvocationInfo&) override;

	void tryToQuitApplication();

	void createPlugin(const proto::PluginDescription&, Point<int> pos);

	void addPluginsToMenu(PopupMenu&);
	std::optional<proto::PluginDescription> getChosenType(int menuID) const;

	std::unique_ptr<GraphDocumentComponent> graphHolder;

private:
	//==============================================================================
	void showGraphIOEditor();

	//==============================================================================
	EngineClient& client;

	std::vector<proto::PluginDescription> menuPlugins;

	class PluginListWindow;
	std::unique_ptr<PluginListWindow> pluginListWindow;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainHostWindow)
};
