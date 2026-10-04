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

ApplicationProperties& getAppProperties();

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
	grpc::Status NewGraph(grpc::ServerContext* context,
	                      const proto::NewGraphRequest* request,
	                      proto::NewGraphResponse* response) override;
	grpc::Status LoadGraph(grpc::ServerContext* context,
	                       const proto::LoadGraphRequest* request,
	                       proto::LoadGraphResponse* response) override;
	grpc::Status SaveGraph(grpc::ServerContext* context,
	                       const proto::SaveGraphRequest* request,
	                       proto::SaveGraphResponse* response) override;
	grpc::Status ClearGraph(grpc::ServerContext* context,
	                        const proto::ClearGraphRequest* request,
	                        proto::ClearGraphResponse* response) override;
	grpc::Status ShowAudioSettings(grpc::ServerContext* context,
	                               const proto::ShowAudioSettingsRequest* request,
	                               proto::ShowAudioSettingsResponse* response) override;
	grpc::Status SetDoublePrecision(grpc::ServerContext* context,
	                                const proto::SetDoublePrecisionRequest* request,
	                                proto::SetDoublePrecisionResponse* response) override;
	grpc::Status SetAutoScalePluginWindows(grpc::ServerContext* context,
	                                       const proto::SetAutoScalePluginWindowsRequest* request,
	                                       proto::SetAutoScalePluginWindowsResponse* response) override;
	grpc::Status GetSettings(grpc::ServerContext* context,
	                         const proto::GetSettingsRequest* request,
	                         proto::GetSettingsResponse* response) override;
	grpc::Status ScanPlugins(grpc::ServerContext* context,
	                         const proto::ScanPluginsRequest* request,
	                         proto::ScanPluginsResponse* response) override;
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

	/** Scans the plug-in directories of all registered formats on a
	    background thread; results are broadcast as PluginListChanged events. */
	class PluginScanThread final : public Thread
	{
	public:
		PluginScanThread(KnownPluginList& list, AudioPluginFormatManager& formats)
		    : Thread("PluginScanThread"),
		      list(list),
		      formats(formats)
		{
		}

		void run() override
		{
			for (auto* format : formats.getFormats())
			{
				auto path = format->getDefaultLocationsToSearch();

				if (auto* settings = getAppProperties().getUserSettings())
					path = PluginListComponent::getLastSearchPath(*settings, *format);

				PluginDirectoryScanner scanner(list, *format, path, true, File{}, true);
				String name;

				while (!threadShouldExit() && scanner.scanNextFile(false, name))
					;
			}

			// let the server release the thread on the message thread
			if (onFinished != nullptr)
				MessageManager::callAsync(onFinished);
		}

		/** Invoked (message thread) when the scan has finished. */
		std::function<void()> onFinished;

	private:
		KnownPluginList& list;
		AudioPluginFormatManager& formats;
	};

	void onMessageThread(std::function<void()> function);
	void sendEvent(proto::EngineEvent event);

	//==============================================================================
	PluginGraph& graph;
	AudioProcessorPlayer& player;
	KnownPluginList& pluginList;
	AudioPluginFormatManager& formatManager;

	std::unique_ptr<grpc::Server> server;
	std::unique_ptr<PluginScanThread> scanThread;

	std::mutex eventMutex;
	std::condition_variable eventCondition;
	std::vector<proto::EngineEvent> pendingEvents;
	bool clientConnected{false};

	JUCE_DECLARE_NON_COPYABLE(EngineServer)
};
