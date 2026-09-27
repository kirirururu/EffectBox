#include "EngineServer.h"

#include "Common.h"

#include <grpcpp/server_builder.h>

static proto::PluginDescription toProto(const PluginDescription& plugin)
{
	proto::PluginDescription result;
	result.set_identifier_string(plugin.createIdentifierString().toStdString());
	result.set_name(plugin.pluginFormatName.toStdString());
	result.set_descriptive_name(plugin.descriptiveName.toStdString());
	result.set_format_name(plugin.pluginFormatName.toStdString());
	result.set_category(plugin.category.toStdString());
	result.set_manufacturer(plugin.manufacturerName.toStdString());
	result.set_version(plugin.version.toStdString());
	result.set_file(plugin.fileOrIdentifier.toStdString());
	result.set_last_file_mod_time(plugin.lastFileModTime.toMilliseconds());
	result.set_last_info_update_time(plugin.lastInfoUpdateTime.toMilliseconds());
	result.set_unique_id(plugin.uniqueId);
	result.set_is_instrument(plugin.isInstrument);
	result.set_num_input_channels(static_cast<uint32_t>(plugin.numInputChannels));
	result.set_num_output_channels(static_cast<uint32_t>(plugin.numOutputChannels));
	result.set_has_shared_container(plugin.hasSharedContainer);
	return result;
}

static proto::BusInfo toProto(const AudioProcessor::Bus& bus)
{
	proto::BusInfo result;
	const auto& layout = bus.getCurrentLayout();
	result.set_name(bus.getName().toStdString());
	result.set_layout(
	    layout.isDisabled() ? std::string{"disabled"}
	                        : layout.getSpeakerArrangementAsString().toStdString()
	);
	return result;
}

static proto::ProcessorInfo toProto(AudioProcessor* processor)
{
	proto::ProcessorInfo result;
	if (processor)
	{
		result.set_num_ins(processor->getTotalNumInputChannels());
		result.set_num_outs(processor->getTotalNumOutputChannels());
		result.set_accepts_midi(processor->acceptsMidi());
		result.set_produces_midi(processor->producesMidi());

		for (int i = 0; i < processor->getBusCount(true); ++i)
			if (const auto* bus = processor->getBus(true, i))
				*result.add_input_buses() = toProto(*bus);

		for (int i = 0; i < processor->getBusCount(false); ++i)
			if (const auto* bus = processor->getBus(false, i))
				*result.add_output_buses() = toProto(*bus);
	}
	return result;
}

static proto::GraphSnapshot toProto(const PluginGraph& graph)
{
	proto::GraphSnapshot result;

	for (const auto* node : graph.graph.getNodes())
	{
		auto& n = *result.add_nodes();
		n.set_uid(node->nodeID.uid);
		n.set_x(node->properties["x"]);
		n.set_y(node->properties["y"]);

		if (const auto* plugin = dynamic_cast<AudioPluginInstance*>(node->getProcessor()))
			*n.mutable_plugin() = toProto(plugin->getPluginDescription());

		*n.mutable_processor() = toProto(node->getProcessor());
		n.set_bypassed(node->isBypassed());
	}

	for (const auto& connection : graph.graph.getConnections())
	{
		auto& c = *result.add_connections();
		c.set_source_node(connection.source.nodeID.uid);
		c.set_source_channel(connection.source.channelIndex);
		c.set_destination_node(connection.destination.nodeID.uid);
		c.set_destination_channel(connection.destination.channelIndex);
	}

	for (const auto& input : graph.inputs)
	{
		auto& in = *result.add_inputs();
		in.set_id(input.id.toString().toStdString());
		in.set_name(input.name.toStdString());
		in.set_num_channels(input.numChannels);
	}

	for (const auto& output : graph.outputs)
	{
		auto& out = *result.add_outputs();
		out.set_id(output.id.toString().toStdString());
		out.set_name(output.name.toStdString());
		out.set_num_channels(output.numChannels);
	}

	return result;
}

static AudioProcessorGraph::NodeID toNodeID(uint32_t uid)
{
	return AudioProcessorGraph::NodeID{uid};
}

//==============================================================================
EngineServer::EngineServer(PluginGraph& g,
                           AudioProcessorPlayer& p,
                           KnownPluginList& kpl,
                           AudioPluginFormatManager& fm)
    : graph(g),
      player(p),
      pluginList(kpl),
      formatManager(fm)
{
	(void)player;
	(void)formatManager;
	graph.addChangeListener(this);
	graph.graph.addChangeListener(this);

	grpc::ServerBuilder builder;
	builder.AddListeningPort("unix:" + getSocketPath(), grpc::InsecureServerCredentials());
	builder.RegisterService(this);
	// two poller threads: one serves the event stream, the other handles unary calls
	builder.SetSyncServerOption(grpc::ServerBuilder::NUM_CQS, 1);
	builder.SetSyncServerOption(grpc::ServerBuilder::MIN_POLLERS, 2);
	builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS, 2);
	server = builder.BuildAndStart();
}

EngineServer::~EngineServer() noexcept
{
	graph.removeChangeListener(this);
	graph.graph.removeChangeListener(this);

	if (server != nullptr)
		server->Shutdown();
}

void EngineServer::changeListenerCallback(ChangeBroadcaster*)
{
	proto::EngineEvent event;
	*event.mutable_plugin_graph_changed()->mutable_snapshot() = toProto(graph);
	sendEvent(std::move(event));
}

void EngineServer::sendPluginCreateFailed(const PluginDescription& description, const String& error)
{
	proto::EngineEvent event;
	auto* failed = event.mutable_plugin_create_failed();
	failed->set_plugin(description.descriptiveName.toStdString());
	failed->set_format_name(description.pluginFormatName.toStdString());
	failed->set_error(error.toStdString());
	sendEvent(std::move(event));
}

//==============================================================================
void EngineServer::onMessageThread(std::function<void()> function)
{
	MessageManager::callAsync(std::move(function));
}

void EngineServer::sendEvent(proto::EngineEvent event)
{
	const std::lock_guard lock(eventMutex);

	if (!clientConnected)
	{
		// without a client don't let events pile up: a newer snapshot replaces
		// the pending one of the same type
		for (auto& pending : pendingEvents)
			if (pending.event_case() == event.event_case())
			{
				pending = std::move(event);
				return;
			}
	}

	pendingEvents.push_back(std::move(event));
	eventCondition.notify_one();
}

grpc::Status EngineServer::GetPluginList(grpc::ServerContext*,
                                         const proto::GetPluginListRequest*,
                                         proto::GetPluginListResponse* response)
{
	runOnMessageThread(
	    [this, response]
	    {
		    for (const auto& plugin : pluginList.getTypes())
				*response->add_plugins() = toProto(plugin);
	    })
	    .get();

	return grpc::Status::OK;
}

grpc::Status EngineServer::GetGraphSnapshot(grpc::ServerContext*,
                                            const proto::GetGraphSnapshotRequest*,
                                            proto::GetGraphSnapshotResponse* response)
{
	runOnMessageThread(
	    [this, response]
	    {
		    *response->mutable_snapshot() = toProto(graph);
	    })
	    .get();

	return grpc::Status::OK;
}

grpc::Status EngineServer::AddPlugin(grpc::ServerContext*,
                                     const proto::AddPluginRequest* request,
                                     proto::AddPluginResponse* response)
{
	onMessageThread(
	    [this, request]
	    {
		    if (pluginList.getTypeForIdentifierString(request->identifier_string()) != nullptr)
			    graph.addPlugin(request->identifier_string(), {request->x(), request->y()});
	    });

	response->set_added(true);
	return grpc::Status::OK;
}

grpc::Status EngineServer::RemoveNode(grpc::ServerContext*,
                                      const proto::RemoveNodeRequest* request,
                                      proto::RemoveNodeResponse* response)
{
	onMessageThread(
	    [this, request]
	    {
		    graph.graph.removeNode(
		        toNodeID(static_cast<std::uint32_t>(request->node_id())));
	    });

	response->set_removed(true);
	return grpc::Status::OK;
}

grpc::Status EngineServer::DisconnectNode(grpc::ServerContext*,
                                          const proto::DisconnectNodeRequest* request,
                                          proto::DisconnectNodeResponse* response)
{
	onMessageThread(
	    [this, request]
	    {
		    graph.graph.disconnectNode(
		        toNodeID(static_cast<std::uint32_t>(request->node_id())));
	    });

	response->set_disconnected(true);
	return grpc::Status::OK;
}

grpc::Status EngineServer::AddConnection(grpc::ServerContext*,
                                         const proto::AddConnectionRequest* request,
                                         proto::AddConnectionResponse* response)
{
	onMessageThread(
	    [this, request]
	    {
		    const auto& c = request->connection();
		    graph.graph.addConnection(
		        {{toNodeID(c.source_node()), c.source_channel()},
		         {toNodeID(c.destination_node()), c.destination_channel()}});
	    });

	response->set_added(true);
	return grpc::Status::OK;
}

grpc::Status EngineServer::RemoveConnection(grpc::ServerContext*,
                                            const proto::RemoveConnectionRequest* request,
                                            proto::RemoveConnectionResponse* response)
{
	onMessageThread(
	    [this, request]
	    {
		    const auto& c = request->connection();
		    graph.graph.removeConnection(
		        {{toNodeID(c.source_node()), c.source_channel()},
		         {toNodeID(c.destination_node()), c.destination_channel()}});
	    });

	response->set_removed(true);
	return grpc::Status::OK;
}

grpc::Status EngineServer::SetNodePosition(grpc::ServerContext*,
                                           const proto::SetNodePositionRequest* request,
                                           proto::SetNodePositionResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    graph.setNodePosition(toNodeID(request->node_id()), {request->x(), request->y()});
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::AddIOEndpoint(grpc::ServerContext*,
                                         const proto::AddIOEndpointRequest* request,
                                         proto::AddIOEndpointResponse* response)
{
	const auto id = runOnMessageThread(
	                   [this, request]
	                   {
	                   return graph.addIOEndpoint(request->name(), request->channels(),
	                                               request->is_input());
	                   })
	                   .get();

	response->set_endpoint_id(id.toString().toStdString());
	return grpc::Status::OK;
}

grpc::Status EngineServer::RemoveIOEndpoint(grpc::ServerContext*,
                                            const proto::RemoveIOEndpointRequest* request,
                                            proto::RemoveIOEndpointResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    graph.removeIOEndpoint(Uuid{request->endpoint_id()}, request->is_input());
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::SetIOEndpointName(grpc::ServerContext*,
                                             const proto::SetIOEndpointNameRequest* request,
                                             proto::SetIOEndpointNameResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    graph.setIOEndpointName(Uuid{request->endpoint_id()}, request->name(), request->is_input());
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::SetIOEndpointChannels(grpc::ServerContext*,
                                                 const proto::SetIOEndpointChannelsRequest* request,
                                                 proto::SetIOEndpointChannelsResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    graph.setIOEndpointChannels(Uuid{request->endpoint_id()}, request->channels(),
		                                request->is_input());
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::OpenPluginWindow(grpc::ServerContext*,
                                            const proto::OpenPluginWindowRequest* request,
                                            proto::OpenPluginWindowResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    if (auto* node = graph.graph.getNodeForId(toNodeID(request->node_id())))
			    graph.getOrCreateWindowFor(node, static_cast<PluginWindow::Type>(request->type()));
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::CloseAllPluginWindows(grpc::ServerContext*,
                                                 const proto::CloseAllPluginWindowsRequest*,
                                                 proto::CloseAllPluginWindowsResponse*)
{
	onMessageThread([this] { graph.closeAnyOpenPluginWindows(); });
	return grpc::Status::OK;
}

grpc::Status EngineServer::ToggleBypass(grpc::ServerContext*,
                                        const proto::ToggleBypassRequest* request,
                                        proto::ToggleBypassResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    if (auto* node = graph.graph.getNodeForId(toNodeID(request->node_id())))
			    node->setBypassed(!node->isBypassed());
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::SavePluginState(grpc::ServerContext*,
                                           const proto::SavePluginStateRequest* request,
                                           proto::SavePluginStateResponse* response)
{
	auto state = runOnMessageThread(
	    [this, request] -> String
	    {
		    if (auto* node = graph.graph.getNodeForId(toNodeID(request->node_id())))
			    if (auto* processor = node->getProcessor())
			    {
				    MemoryBlock block;
				    processor->getStateInformation(block);
				    return block.toBase64Encoding();
			    }

		    return {};
	    });

	if (state.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
		return grpc::Status(grpc::StatusCode::INTERNAL, "Timed out");

	response->set_data_base64(state.get().toStdString());
	return grpc::Status::OK;
}

grpc::Status EngineServer::LoadPluginState(grpc::ServerContext*,
                                           const proto::LoadPluginStateRequest* request,
                                           proto::LoadPluginStateResponse*)
{
	onMessageThread(
	    [this, request]
	    {
		    if (auto* node = graph.graph.getNodeForId(toNodeID(request->node_id())))
			    if (auto* processor = node->getProcessor())
			    {
				    MemoryBlock block;
				    block.fromBase64Encoding(request->data_base64());

				    if (block.getSize() > 0)
					    processor->setStateInformation(block.getData(),
					                                  static_cast<int>(block.getSize()));
			    }
	    });

	return grpc::Status::OK;
}

grpc::Status EngineServer::GetEvents(grpc::ServerContext* context,
                                     const proto::GetEventsRequest*,
                                     grpc::ServerWriter<proto::EngineEvent>* writer)
{
	{
		const std::lock_guard lock(eventMutex);

		if (clientConnected)
			return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "A client is already connected");

		clientConnected = true;
	}

	{
		auto initial = runOnMessageThread(
		    [this]
		    {
			    std::vector<proto::EngineEvent> events;

			    proto::EngineEvent graphEvent;
			    *graphEvent.mutable_plugin_graph_changed()->mutable_snapshot() = toProto(graph);
			    events.push_back(std::move(graphEvent));

			    proto::EngineEvent listEvent;
			    for (const auto& plugin : pluginList.getTypes())
				    *listEvent.mutable_plugin_list_changed()->add_plugins() = toProto(plugin);
			    events.push_back(std::move(listEvent));

			    return events;
		    });

		for (const auto& event : initial.get())
			if (!writer->Write(event))
				break;
	}

	while (!context->IsCancelled())
	{
		std::vector<proto::EngineEvent> events;

		{
			std::unique_lock lock(eventMutex);
			eventCondition.wait_for(lock, std::chrono::milliseconds(100),
			                        [this] { return !pendingEvents.empty(); });
			events.swap(pendingEvents);
		}

		for (const auto& event : events)
			if (!writer->Write(event))
				break;
	}

	{
		const std::lock_guard lock(eventMutex);
		clientConnected = false;
		pendingEvents.clear();
	}

	return grpc::Status::OK;
}
