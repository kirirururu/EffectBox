#include "GraphIOEditor.h"
#include "MainHostWindow.h"

static constexpr int menuIDBase = 0x324503f4;

//==============================================================================
class MainHostWindow::PluginListWindow final : public DocumentWindow
{
public:
	PluginListWindow(MainHostWindow& mw, EngineClient& cl)
	    : DocumentWindow(
	          "Available Plugins",
	          LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
	          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
	      owner(mw),
	      client(cl)
	{
		list.setModel(&listModel);
		list.setRowHeight(26);

		setContentOwned(new PluginListContent(*this), true);

		setResizable(true, false);
		setResizeLimits(300, 400, 800, 1500);
		setTopLeftPosition(60, 60);

		restoreWindowStateFromString(
		    getAppProperties().getUserSettings()->getValue("listWindowPos"));
		setVisible(true);
	}

	~PluginListWindow() override
	{
		getAppProperties().getUserSettings()->setValue("listWindowPos", getWindowStateAsString());
	}

	void closeButtonPressed() override { owner.pluginListWindow = nullptr; }

	/** Redraws the list after the engine's plug-in list has changed. */
	void refresh() { list.updateContent(); }

private:
	struct PluginListContent final : public Component
	{
		explicit PluginListContent(PluginListWindow& w) : owner(w)
		{
			addAndMakeVisible(&w.list);
		}

		void resized() override
		{
			owner.list.setBounds(getLocalBounds());
		}

		PluginListWindow& owner;

		JUCE_DECLARE_NON_COPYABLE(PluginListContent)
	};

	class ListModel final : public ListBoxModel
	{
	public:
		explicit ListModel(EngineClient& c) : client(c) { }

		int getNumRows() override { return (int)client.plugins.size(); }

		void paintListBoxItem(int row, Graphics& g, int width, int height,
		                      bool rowIsSelected) override
		{
			g.fillAll(rowIsSelected ? Colour(0xff42A2C8) : Colours::transparentBlack);
			g.setColour(Colours::white);
			g.setFont(FontOptions(13.0f, Font::plain));

			if (isPositiveAndBelow(row, (int)client.plugins.size()))
			{
				const auto& p = client.plugins[(size_t)row];
				g.drawText(
				    String{p.name()} + "  (" + String{p.manufacturer()} + ")", 6,
				    0, width - 12, height, Justification::centredLeft);
			}
		}

	private:
		EngineClient& client;
	};

	MainHostWindow& owner;
	EngineClient& client;
	ListBox list;
	ListModel listModel{client};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListWindow)
};

//==============================================================================
MainHostWindow::MainHostWindow(EngineClient& c)
    : DocumentWindow(
          JUCEApplication::getInstance()->getApplicationName(),
          LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
          DocumentWindow::allButtons),
      client(c)
{
	setResizable(true, false);
	setResizeLimits(500, 400, 10000, 10000);
	centreWithSize(800, 600);

	graphHolder.reset(new GraphDocumentComponent(client));
	setContentNonOwned(graphHolder.get(), false);

	setUsingNativeTitleBar(true);

	restoreWindowStateFromString(getAppProperties().getUserSettings()->getValue("mainWindowPos"));

	setVisible(true);

	addKeyListener(getCommandManager().getKeyMappings());

	client.mirror.addChangeListener(this);

#if JUCE_MAC
	setMacMainMenu(this);
#else
	setMenuBar(this);
#endif

	getCommandManager().setFirstCommandTarget(this);
}

MainHostWindow::~MainHostWindow()
{
	pluginListWindow = nullptr;

	client.mirror.removeChangeListener(this);

	getAppProperties().getUserSettings()->setValue("mainWindowPos", getWindowStateAsString());
	clearContentComponent();

#if JUCE_MAC
	setMacMainMenu(nullptr);
#else
	setMenuBar(nullptr);
#endif

	graphHolder = nullptr;
}

void MainHostWindow::closeButtonPressed()
{
	tryToQuitApplication();
}

void MainHostWindow::tryToQuitApplication()
{
	ModalComponentManager::getInstance()->cancelAllModalComponents();

	client.closeAllPluginWindows();

	if (graphIsDirty())
	{
		promptSaveChanges([] { JUCEApplication::quit(); });
		return;
	}

	JUCEApplication::quit();
}

void MainHostWindow::changeListenerCallback(ChangeBroadcaster*)
{
	auto title = JUCEApplication::getInstance()->getApplicationName();
	const auto f = currentGraphFile();

	if (f.existsAsFile())
		title = f.getFileName() + " - " + title;

	setName(title);
}

bool MainHostWindow::isInterestedInFileDrag(const StringArray& files)
{
	return files.size() == 1 && File{files[0]}.hasFileExtension(GRAPH_FILE_SUFFIX);
}

void MainHostWindow::fileDragEnter(const StringArray&, int, int)
{
}

void MainHostWindow::fileDragMove(const StringArray&, int, int)
{
}

void MainHostWindow::fileDragExit(const StringArray&)
{
}

void MainHostWindow::filesDropped(const StringArray& files, int, int)
{
	if (files.size() == 1)
		openGraphFile(File{files[0]});
}

void MainHostWindow::handleEngineConnected()
{
	const auto settings = client.getSettings();
	doublePrecisionEnabled = settings.doublePrecision;
	autoScaleEnabled = settings.autoScalePluginWindows;

	menuItemsChanged();
}

void MainHostWindow::pluginListChanged()
{
	menuItemsChanged();

	if (pluginListWindow != nullptr)
		pluginListWindow->refresh();
}

void MainHostWindow::openGraphFile(const File& file)
{
	const auto load = [this, file]
	{
		String error;

		if (client.loadGraph(file.getFullPathName(), error))
			addRecentFile(file);
		else
			AlertWindow::showMessageBoxAsync(
			    MessageBoxIconType::WarningIcon, TRANS("Failed to open file"), error, "OK");
	};

	if (graphIsDirty())
		promptSaveChanges(load);
	else
		load();
}

void MainHostWindow::promptSaveChanges(std::function<void()> proceed)
{
	const auto f = currentGraphFile();
	const auto documentTitle =
	    f.existsAsFile() ? f.getFileName() : TRANS("the current graph");

	auto* alert = new AlertWindow(
	    TRANS("Save changes?"),
	    TRANS("Do you want to save the changes you made to ") + documentTitle + "?",
	    MessageBoxIconType::QuestionIcon);

	alert->addButton(TRANS("Save"), 1, KeyPress{KeyPress::returnKey});
	alert->addButton(TRANS("Discard changes"), 2, KeyPress{});
	alert->addButton(TRANS("Cancel"), 0, KeyPress{KeyPress::escapeKey});

	alert->enterModalState(
	    true,
	    ModalCallbackFunction::create(
	        [this, proceed = std::move(proceed)](int result)
	        {
		        switch (result)
		        {
		        case 1:
			        if (currentGraphFile().existsAsFile())
				        saveToCurrentFile(std::move(proceed));
			        else
				        saveGraphAs(std::move(proceed));
			        break;

		        case 2:
			        proceed();
			        break;

		        default:
			        break;
		        }
	        }),
	    true);
}

void MainHostWindow::saveToCurrentFile(std::function<void()> onDone)
{
	const auto f = currentGraphFile();
	String error;

	if (client.saveGraph(f.getFullPathName(), error))
	{
		if (onDone != nullptr)
			onDone();
	}
	else
	{
		AlertWindow::showMessageBoxAsync(
		    MessageBoxIconType::WarningIcon, TRANS("Failed to save file"), error, "OK");
	}
}

void MainHostWindow::saveGraphAs(std::function<void()> onDone)
{
	auto* chooser = new FileChooser(TRANS("Save graph"), File{}, String{"*"} + GRAPH_FILE_SUFFIX);

	chooser->launchAsync(
	    FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
	    [this, onDone = std::move(onDone)](const FileChooser& c)
	    {
		    const auto result = c.getResult();

		    if (result == File())
		    {
			    // the user cancelled: abort the follow-up action as well
			    return;
		    }

		    String error;

		    if (client.saveGraph(result.getFullPathName(), error))
		    {
			    if (onDone != nullptr)
				    onDone();
		    }
		    else
		    {
			    AlertWindow::showMessageBoxAsync(
			        MessageBoxIconType::WarningIcon, TRANS("Failed to save file"), error, "OK");
		    }
	    });
}

void MainHostWindow::addRecentFile(const File& file)
{
	RecentlyOpenedFilesList recentFiles;
	recentFiles.restoreFromString(
	    getAppProperties().getUserSettings()->getValue("recentFilterGraphFiles"));

	recentFiles.addFile(file);

	getAppProperties().getUserSettings()->setValue(
	    "recentFilterGraphFiles", recentFiles.toString());
}

void MainHostWindow::menuBarActivated(bool isActivated)
{
	if (isActivated && graphHolder != nullptr)
		Component::unfocusAllComponents();
}

StringArray MainHostWindow::getMenuBarNames()
{
	StringArray names;
	names.add("File");
	names.add("Plugins");
	names.add("Options");
	names.add("Windows");
	return names;
}

PopupMenu MainHostWindow::getMenuForIndex(int topLevelMenuIndex, const String& /*menuName*/)
{
	PopupMenu menu;

	if (topLevelMenuIndex == 0)
	{
		// "File" menu
		menu.addCommandItem(&getCommandManager(), CommandIDs::newFile);
		menu.addCommandItem(&getCommandManager(), CommandIDs::open);

		RecentlyOpenedFilesList recentFiles;
		recentFiles.restoreFromString(
		    getAppProperties().getUserSettings()->getValue("recentFilterGraphFiles"));

		PopupMenu recentFilesMenu;
		recentFiles.createPopupMenuItems(recentFilesMenu, 100, true, true);
		menu.addSubMenu("Open recent file", recentFilesMenu);

		menu.addCommandItem(&getCommandManager(), CommandIDs::save);
		menu.addCommandItem(&getCommandManager(), CommandIDs::saveAs);
		menu.addSeparator();
		menu.addCommandItem(&getCommandManager(), StandardApplicationCommandIDs::quit);
	}
	else if (topLevelMenuIndex == 1)
	{
		// "Plugins" menu
		PopupMenu pluginsMenu;
		addPluginsToMenu(pluginsMenu);
		menu.addSubMenu("Create Plug-in", pluginsMenu);
		menu.addSeparator();
		menu.addItem(250, "Delete All Plug-ins");
		menu.addCommandItem(&getCommandManager(), CommandIDs::scanPlugins);
	}
	else if (topLevelMenuIndex == 2)
	{
		// "Options" menu
		menu.addCommandItem(&getCommandManager(), CommandIDs::showPluginListEditor);
		menu.addSeparator();
		menu.addCommandItem(&getCommandManager(), CommandIDs::showGraphIO);
		menu.addCommandItem(&getCommandManager(), CommandIDs::showAudioSettings);
		menu.addCommandItem(&getCommandManager(), CommandIDs::toggleDoublePrecision);

		if (autoScaleOptionAvailable)
			menu.addCommandItem(&getCommandManager(), CommandIDs::autoScalePluginWindows);

		menu.addSeparator();
		menu.addCommandItem(&getCommandManager(), CommandIDs::aboutBox);
	}
	else if (topLevelMenuIndex == 3)
	{
		menu.addCommandItem(&getCommandManager(), CommandIDs::allWindowsForward);
	}

	return menu;
}

void MainHostWindow::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
	if (menuItemID == 250)
	{
		client.clearGraph();
	}
	else if (menuItemID >= 100 && menuItemID < 200)
	{
		RecentlyOpenedFilesList recentFiles;
		recentFiles.restoreFromString(
		    getAppProperties().getUserSettings()->getValue("recentFilterGraphFiles"));

		const auto file = recentFiles.getFile(menuItemID - 100);

		if (file.existsAsFile())
			openGraphFile(file);
	}
	else if (const auto chosen = getChosenType(menuItemID))
	{
		createPlugin(
		    *chosen,
		    {proportionOfWidth(0.3f + Random::getSystemRandom().nextFloat() * 0.6f),
		     proportionOfHeight(0.3f + Random::getSystemRandom().nextFloat() * 0.6f)});
	}
}

void MainHostWindow::createPlugin(const proto::PluginDescription& plugin, Point<int> pos)
{
	if (graphHolder != nullptr)
		graphHolder->createNewPlugin(plugin, pos);
}

void MainHostWindow::addPluginsToMenu(PopupMenu& m)
{
	menuPlugins.clear();

	for (const auto& plugin : client.plugins)
	{
		menuPlugins.push_back(plugin);
		const auto menuID = static_cast<int>(menuPlugins.size()) - 1 + menuIDBase;
		auto name = String{plugin.name()};

		if (!plugin.manufacturer().empty())
			name << " (" << String{plugin.manufacturer()} << ')';

		m.addItem(menuID, name, true, false);
	}
}

std::optional<proto::PluginDescription> MainHostWindow::getChosenType(const int menuID) const
{
	const auto index = menuID - menuIDBase;

	if (isPositiveAndBelow(index, static_cast<int>(menuPlugins.size())))
		return menuPlugins[static_cast<size_t>(index)];

	return {};
}

//==============================================================================
ApplicationCommandTarget* MainHostWindow::getNextCommandTarget()
{
	return findFirstTargetParentComponent();
}

void MainHostWindow::getAllCommands(Array<CommandID>& commands)
{
	const CommandID ids[] = {
	    CommandIDs::newFile,
	    CommandIDs::open,
	    CommandIDs::save,
	    CommandIDs::saveAs,
	    CommandIDs::scanPlugins,
	    CommandIDs::showPluginListEditor,
	    CommandIDs::showGraphIO,
	    CommandIDs::showAudioSettings,
	    CommandIDs::toggleDoublePrecision,
	    CommandIDs::autoScalePluginWindows,
	    CommandIDs::aboutBox,
	    CommandIDs::allWindowsForward,
	};

	commands.addArray(ids, numElementsInArray(ids));
}

void MainHostWindow::getCommandInfo(const CommandID commandID, ApplicationCommandInfo& result)
{
	const String category("General");

	switch (commandID)
	{
	case CommandIDs::newFile:
		result.setInfo("New", "Creates a new filter graph file", category, 0);
		result.defaultKeypresses.add(KeyPress('n', ModifierKeys::commandModifier, 0));
		break;

	case CommandIDs::open:
		result.setInfo("Open...", "Opens a filter graph file", category, 0);
		result.defaultKeypresses.add(KeyPress('o', ModifierKeys::commandModifier, 0));
		break;

	case CommandIDs::save:
		result.setInfo("Save", "Saves the current graph to a file", category, 0);
		result.defaultKeypresses.add(KeyPress('s', ModifierKeys::commandModifier, 0));
		break;

	case CommandIDs::saveAs:
		result.setInfo("Save As...", "Saves a copy of the current graph to a file", category, 0);
		result.defaultKeypresses.add(
		    KeyPress('s', ModifierKeys::shiftModifier | ModifierKeys::commandModifier, 0));
		break;

	case CommandIDs::scanPlugins:
		result.setInfo("Scan For New Plug-ins...",
		               "Searches the plug-in directories and updates the list", category, 0);
		break;

	case CommandIDs::showPluginListEditor:
		result.setInfo("Edit the List of Available Plug-ins...", {}, category, 0);
		result.addDefaultKeypress('p', ModifierKeys::commandModifier);
		break;

	case CommandIDs::showGraphIO:
		result.setInfo("Edit Graph Inputs/Outputs...",
		               "Adds, removes and edits the inputs and outputs of the graph", category, 0);
		break;

	case CommandIDs::showAudioSettings:
		result.setInfo("Change the Audio Device Settings", {}, category, 0);
		result.addDefaultKeypress('a', ModifierKeys::commandModifier);
		break;

	case CommandIDs::toggleDoublePrecision:
		updatePrecisionMenuItem(result, doublePrecisionEnabled);
		break;

	case CommandIDs::autoScalePluginWindows:
		updateAutoScaleMenuItem(result, autoScaleEnabled);
		break;

	case CommandIDs::aboutBox:
		result.setInfo("About...", {}, category, 0);
		break;

	case CommandIDs::allWindowsForward:
		result.setInfo("All Windows Forward", "Bring all plug-in windows forward", category, 0);
		result.addDefaultKeypress('w', ModifierKeys::commandModifier);
		break;

	default:
		break;
	}
}

bool MainHostWindow::perform(const InvocationInfo& info)
{
	switch (info.commandID)
	{
	case CommandIDs::newFile:
	{
		const auto create = [this] { client.newGraph(); };

		if (graphIsDirty())
			promptSaveChanges(create);
		else
			create();
		break;
	}

	case CommandIDs::open:
	{
		auto* chooser =
		    new FileChooser(TRANS("Open graph"), File{}, String{"*"} + GRAPH_FILE_SUFFIX);

		chooser->launchAsync(
		    FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
		    [this](const FileChooser& c)
		    {
			    const auto result = c.getResult();

			    if (result.existsAsFile())
				    openGraphFile(result);
		    });
		break;
	}

	case CommandIDs::save:
		if (currentGraphFile().existsAsFile())
			saveToCurrentFile({});
		else
			saveGraphAs({});
		break;

	case CommandIDs::saveAs:
		saveGraphAs({});
		break;

	case CommandIDs::scanPlugins:
	{
		switch (client.scanPlugins())
		{
		case 0:
			AlertWindow::showMessageBoxAsync(
			    MessageBoxIconType::WarningIcon,
			    TRANS("Failed to start the plug-in scan"),
			    TRANS("Couldn't reach the audio engine."),
			    "OK");
			break;

		case 2:
			AlertWindow::showMessageBoxAsync(
			    MessageBoxIconType::InfoIcon,
			    TRANS("Plug-in scan already in progress"),
			    TRANS("The plug-in list will update when the scan finishes."),
			    "OK");
			break;

		default:
			break;
		}

		break;
	}

	case CommandIDs::showAudioSettings:
		client.showAudioSettings();
		break;

	case CommandIDs::toggleDoublePrecision:
	{
		const auto enabled = !doublePrecisionEnabled;
		doublePrecisionEnabled = enabled;

		client.setDoublePrecision(enabled);
		menuItemsChanged();
		break;
	}

	case CommandIDs::autoScalePluginWindows:
	{
		const auto enabled = !autoScaleEnabled;
		autoScaleEnabled = enabled;

		client.setAutoScalePluginWindows(enabled);
		menuItemsChanged();
		break;
	}

	case CommandIDs::showPluginListEditor:
		if (pluginListWindow == nullptr)
			pluginListWindow.reset(new PluginListWindow(*this, client));

		pluginListWindow->toFront(true);
		break;

	case CommandIDs::showGraphIO:
		showGraphIOEditor();
		break;

	case CommandIDs::aboutBox:
		break;

	case CommandIDs::allWindowsForward:
	{
		auto& desktop = Desktop::getInstance();

		for (int i = 0; i < desktop.getNumComponents(); ++i)
			desktop.getComponent(i)->toBehind(this);

		break;
	}

	default:
		return false;
	}

	return true;
}

void MainHostWindow::showGraphIOEditor()
{
	if (graphHolder == nullptr)
		return;

	auto* editor = new GraphIOEditor(client.mirror, client);
	editor->setSize(540, 420);

	DialogWindow::LaunchOptions o;
	o.content.setOwned(editor);
	o.dialogTitle = "Graph Inputs/Outputs";
	o.componentToCentreAround = this;
	o.dialogBackgroundColour = getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
	o.escapeKeyTriggersCloseButton = true;
	o.useNativeTitleBar = false;
	o.resizable = false;

	auto* w = o.create();
	w->enterModalState(true, ModalCallbackFunction::create([](int) { }), true);
}

void MainHostWindow::updatePrecisionMenuItem(ApplicationCommandInfo& info, bool enabled)
{
	info.setInfo("Double Floating-Point Precision Rendering", {}, "General", 0);
	info.setTicked(enabled);
}

void MainHostWindow::updateAutoScaleMenuItem(ApplicationCommandInfo& info, bool enabled)
{
	info.setInfo("Auto-Scale Plug-in Windows", {}, "General", 0);
	info.setTicked(enabled);
}
