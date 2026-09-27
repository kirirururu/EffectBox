#pragma once

#include "PluginGraph.h"

#include "EffectBoxRPC.grpc.pb.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>

using namespace juce;

/**
    Listens on a UNIX-domain socket and serves the graph editor protocol to a
    single connected client (the GUI application).
*/
class EngineServer final : public proto::Engine::Service, public ChangeListener
{
public:
	EngineServer(PluginGraph& graph,
	             AudioProcessorPlayer& player,
	             KnownPluginList& pluginList,
	             AudioPluginFormatManager& formatManager);
	~EngineServer() noexcept override;

	//==============================================================================
	void changeListenerCallback(ChangeBroadcaster*) override;

	/** Sends an event about a failed plugin instantiation (message thread). */
	void sendPluginCreateFailed(const PluginDescription& description, const String& error);

	//==============================================================================
	grpc::Status GetPluginList(grpc::ServerContext* context,
	                           const proto::GetPluginListRequest* request,
	                           proto::GetPluginListResponse* response) override;
	grpc::Status GetGraphSnapshot(grpc::ServerContext* context,
	                              const proto::GetGraphSnapshotRequest* request,
	                              proto::GetGraphSnapshotResponse* response) override;
	grpc::Status AddPlugin(grpc::ServerContext* context,
	                       const proto::AddPluginRequest* request,
	                       proto::AddPluginResponse* response) override;
	grpc::Status RemoveNode(grpc::ServerContext* context,
	                        const proto::RemoveNodeRequest* request,
	                        proto::RemoveNodeResponse* response) override;
	grpc::Status DisconnectNode(grpc::ServerContext* context,
	                          const proto::DisconnectNodeRequest* request,
	                          proto::DisconnectNodeResponse* response) override;
	grpc::Status AddConnection(grpc::ServerContext* context,
	                          const proto::AddConnectionRequest* request,
	                          proto::AddConnectionResponse* response) override;
	grpc::Status RemoveConnection(grpc::ServerContext* context,
	                             const proto::RemoveConnectionRequest* request,
	                             proto::RemoveConnectionResponse* response) override;
	grpc::Status SetNodePosition(grpc::ServerContext* context,
	                             const proto::SetNodePositionRequest* request,
	                             proto::SetNodePositionResponse* response) override;
	grpc::Status AddIOEndpoint(grpc::ServerContext* context,
	                           const proto::AddIOEndpointRequest* request,
	                           proto::AddIOEndpointResponse* response) override;
	grpc::Status RemoveIOEndpoint(grpc::ServerContext* context,
	                              const proto::RemoveIOEndpointRequest* request,
	                              proto::RemoveIOEndpointResponse* response) override;
	grpc::Status SetIOEndpointName(grpc::ServerContext* context,
	                               const proto::SetIOEndpointNameRequest* request,
	                               proto::SetIOEndpointNameResponse* response) override;
	grpc::Status SetIOEndpointChannels(grpc::ServerContext* context,
	                                   const proto::SetIOEndpointChannelsRequest* request,
	                                   proto::SetIOEndpointChannelsResponse* response) override;
	grpc::Status OpenPluginWindow(grpc::ServerContext* context,
	                              const proto::OpenPluginWindowRequest* request,
	                              proto::OpenPluginWindowResponse* response) override;
	grpc::Status CloseAllPluginWindows(grpc::ServerContext* context,
	                                   const proto::CloseAllPluginWindowsRequest* request,
	                                   proto::CloseAllPluginWindowsResponse* response) override;
	grpc::Status ToggleBypass(grpc::ServerContext* context,
	                          const proto::ToggleBypassRequest* request,
	                          proto::ToggleBypassResponse* response) override;
	grpc::Status SavePluginState(grpc::ServerContext* context,
	                             const proto::SavePluginStateRequest* request,
	                             proto::SavePluginStateResponse* response) override;
	grpc::Status LoadPluginState(grpc::ServerContext* context,
	                             const proto::LoadPluginStateRequest* request,
	                             proto::LoadPluginStateResponse* response) override;
	grpc::Status GetEvents(grpc::ServerContext* context,
	                       const proto::GetEventsRequest* request,
	                       grpc::ServerWriter<proto::EngineEvent>* writer) override;

private:
	/** Runs the function on the JUCE message thread and returns its result. */
	template <typename F>
	auto runOnMessageThread(F&& function)
	{
		using ResultType = std::invoke_result_t<std::decay_t<F>>;

		if constexpr (std::is_void_v<ResultType>)
		{
			auto promise = std::make_shared<std::promise<void>>();
			auto future = promise->get_future();

			MessageManager::callAsync(
			    [promise, function = std::forward<F>(function)]() mutable
			    {
				    try
				    {
					    function();
					    promise->set_value();
				    }
				    catch (...)
				    {
					    promise->set_exception(std::current_exception());
				    }
			    });

			return future;
		}
		else
		{
			auto promise = std::make_shared<std::promise<ResultType>>();
			auto future = promise->get_future();

			MessageManager::callAsync(
			    [promise, function = std::forward<F>(function)]() mutable
			    {
				    try
				    {
					    promise->set_value(function());
				    }
				    catch (...)
				    {
					    promise->set_exception(std::current_exception());
				    }
			    });

			return future;
		}
	}

	void onMessageThread(std::function<void()> function);
	void sendEvent(proto::EngineEvent event);

	//==============================================================================
	PluginGraph& graph;
	AudioProcessorPlayer& player;
	KnownPluginList& pluginList;
	AudioPluginFormatManager& formatManager;

	std::unique_ptr<grpc::Server> server;

	std::mutex eventMutex;
	std::condition_variable eventCondition;
	std::vector<proto::EngineEvent> pendingEvents;
	bool clientConnected{false};

	JUCE_DECLARE_NON_COPYABLE(EngineServer)
};
