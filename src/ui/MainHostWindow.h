#pragma once

#include "EngineClient.h"
#include "GraphEditorPanel.h"

#include <functional>
#include <vector>

using namespace juce;

//==============================================================================
namespace CommandIDs {
static const int open = 0x30000;
static const int save = 0x30001;
static const int saveAs = 0x30002;
static const int newFile = 0x30003;
static const int scanPlugins = 0x30004;
static const int showPluginListEditor = 0x30100;
static const int showAudioSettings = 0x30200;
static const int aboutBox = 0x30300;
static const int allWindowsForward = 0x30400;
static const int toggleDoublePrecision = 0x30500;
static const int autoScalePluginWindows = 0x30600;
static const int showGraphIO = 0x30700;
} // namespace CommandIDs

//==============================================================================
ApplicationCommandManager& getCommandManager();
ApplicationProperties& getAppProperties();

constexpr bool autoScaleOptionAvailable =
#if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
    true;
#else
    false;
#endif

//==============================================================================
class MainHostWindow final : public DocumentWindow,
                             public MenuBarModel,
                             public ApplicationCommandTarget,
                             public ChangeListener,
                             public FileDragAndDropTarget
{
public:
	//==============================================================================
	explicit MainHostWindow(EngineClient& client);
	~MainHostWindow() override;

	//==============================================================================
	void closeButtonPressed() override;
	void changeListenerCallback(ChangeBroadcaster*) override;

	bool isInterestedInFileDrag(const StringArray& files) override;
	void fileDragEnter(const StringArray& files, int x, int y) override;
	void fileDragMove(const StringArray& files, int x, int y) override;
	void fileDragExit(const StringArray& files) override;
	void filesDropped(const StringArray& files, int x, int y) override;

	void menuBarActivated(bool isActive) override;

	StringArray getMenuBarNames() override;
	PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName) override;
	void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;
	ApplicationCommandTarget* getNextCommandTarget() override;
	void getAllCommands(Array<CommandID>&) override;
	void getCommandInfo(CommandID, ApplicationCommandInfo&) override;
	bool perform(const InvocationInfo&) override;

	void tryToQuitApplication();

	/** Runs once after the client has fetched the initial engine state. */
	void handleEngineConnected();

	/** Refreshes the menus and the plug-in list window after the engine's
	    plug-in list has changed. */
	void pluginListChanged();

	/** Loads a graph file into the engine, prompting to save changes if needed. */
	void openGraphFile(const File& file);

	void createPlugin(const proto::PluginDescription&, Point<int> pos);

	void addPluginsToMenu(PopupMenu&);
	std::optional<proto::PluginDescription> getChosenType(int menuID) const;

	std::unique_ptr<GraphDocumentComponent> graphHolder;

private:
	//==============================================================================
	File currentGraphFile() const { return File{client.mirror.model().file()}; }
	bool graphIsDirty() const { return client.mirror.isDirty(); }

	void promptSaveChanges(std::function<void()> proceed);
	void saveToCurrentFile(std::function<void()> onDone);
	void saveGraphAs(std::function<void()> onDone);
	void addRecentFile(const File& file);

	void showGraphIOEditor();

	//==============================================================================
	static void updatePrecisionMenuItem(ApplicationCommandInfo& info, bool enabled);
	static void updateAutoScaleMenuItem(ApplicationCommandInfo& info, bool enabled);

	//==============================================================================
	EngineClient& client;

	std::vector<proto::PluginDescription> menuPlugins;

	bool doublePrecisionEnabled = false;
	bool autoScaleEnabled = false;

	class PluginListWindow;
	std::unique_ptr<PluginListWindow> pluginListWindow;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainHostWindow)
};
