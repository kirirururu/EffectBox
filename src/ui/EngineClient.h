#pragma once

#include "Common.h"
#include "EffectBoxRPC.grpc.pb.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace juce;

//==============================================================================
/** Local mirror of the engine's graph topology.

    Updated by snapshot events and by local edits (node dragging) that are sent
    to the engine; broadcasts a change message after each snapshot so the
    editor components can resynchronise.
*/
class GraphMirror final : public ChangeBroadcaster
{
public:
	void applySnapshot(proto::GraphSnapshot snapshot);

	/** Updates a node position locally without notifying listeners. */
	void setNodePositionLocal(std::uint32_t uid, double x, double y);

	const proto::GraphSnapshot& model() const { return data; }

	const proto::Node* findNode(std::uint32_t uid) const;
	bool isConnected(const proto::Connection& connection) const;

	/** A local approximation of the engine's connection validation, used for
	    live feedback while dragging a connector. */
	bool canConnect(const proto::Connection& connection) const;

private:
	proto::GraphSnapshot data;
};

//==============================================================================
/** Talks to the engine process: spawns it, keeps the socket, and dispatches
    events onto the message thread.
*/
class EngineClient final : private Thread
{
public:
	EngineClient();
	~EngineClient() override;

	/** Spawns the engine next to the running executable and connects to it. */
	bool start();
	void stop();

	bool isConnected() const noexcept { return connected; }

	//==============================================================================
	// Graph topology
	void addPlugin(const proto::PluginDescription& plugin, double x, double y);
	void removeNode(std::uint32_t uid);
	void disconnectNode(std::uint32_t uid);
	void addConnection(const proto::Connection& connection);
	void removeConnection(const proto::Connection& connection);
	void setNodePosition(std::uint32_t uid, double x, double y);

	//==============================================================================
	// IO endpoints
	void addIOEndpoint(const String& name, int numChannels, bool isInput);
	void removeIOEndpoint(const Uuid& id, bool isInput);
	void setIOEndpointName(const Uuid& id, const String& name, bool isInput);
	void setIOEndpointChannels(const Uuid& id, int numChannels, bool isInput);

	//==============================================================================
	// Plugin windows
	void openPluginWindow(std::uint32_t uid, int windowType);
	void closeAllPluginWindows();

	//==============================================================================
	// Misc
	void toggleBypass(std::uint32_t uid);
	bool savePluginState(std::uint32_t uid, String& outBase64);
	void loadPluginState(std::uint32_t uid, const String& base64);

	//==============================================================================
	GraphMirror mirror;
	std::vector<proto::PluginDescription> plugins;

	std::function<void(const proto::PluginDescription&, const String&)> onPluginCreateFailed;
	std::function<void()> onEngineLost;

private:
	//==============================================================================
	void run() override;
	void pumpEvents();
	void handleEvent(proto::EngineEvent event);
	void notifyEngineLost();

	//==============================================================================
	ChildProcess engineProcess;

	std::shared_ptr<grpc::Channel> channel;
	std::unique_ptr<proto::Engine::Stub> stub;
	std::unique_ptr<grpc::ClientContext> eventsContext;

	std::atomic<bool> connected{false};

	// flipped off in the destructor so message-thread callbacks queued during
	// shutdown don't touch a destroyed client
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);

	JUCE_DECLARE_NON_COPYABLE(EngineClient)
};
