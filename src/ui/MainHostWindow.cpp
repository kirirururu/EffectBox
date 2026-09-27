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
	JUCEApplication::quit();
}

void MainHostWindow::changeListenerCallback(ChangeBroadcaster*)
{
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
		menu.addCommandItem(&getCommandManager(), StandardApplicationCommandIDs::quit);
	}
	else if (topLevelMenuIndex == 1)
	{
		PopupMenu pluginsMenu;
		addPluginsToMenu(pluginsMenu);
		menu.addSubMenu("Create Plug-in", pluginsMenu);
	}
	else if (topLevelMenuIndex == 2)
	{
		menu.addCommandItem(&getCommandManager(), CommandIDs::showPluginListEditor);
		menu.addSeparator();
		menu.addCommandItem(&getCommandManager(), CommandIDs::showGraphIO);
		menu.addSeparator();
		menu.addCommandItem(&getCommandManager(), CommandIDs::aboutBox);
	}
	else if (topLevelMenuIndex == 3)
	{
		menu.addCommandItem(&getCommandManager(), CommandIDs::allWindowsForward);
	}

	return menu;
}

void MainHostWindow::menuItemSelected(int /*menuItemID*/, int /*topLevelMenuIndex*/)
{
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
	    CommandIDs::showPluginListEditor,
	    CommandIDs::showGraphIO,
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
	case CommandIDs::showPluginListEditor:
		result.setInfo("Edit the List of Available Plug-ins...", {}, category, 0);
		result.addDefaultKeypress('p', ModifierKeys::commandModifier);
		break;

	case CommandIDs::showGraphIO:
		result.setInfo("Edit Graph Inputs/Outputs...",
		               "Adds, removes and edits the inputs and outputs of the graph", category, 0);
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
