#include "EngineClient.h"

#include <grpcpp/create_channel.h>

//==============================================================================
void GraphMirror::applySnapshot(proto::GraphSnapshot snapshot)
{
	data = std::move(snapshot);
	sendChangeMessage();
}

void GraphMirror::setNodePositionLocal(std::uint32_t uid, double x, double y)
{
	for (auto& node : *data.mutable_nodes())
		if (node.uid() == uid)
		{
			node.set_x(x);
			node.set_y(y);
			return;
		}
}

const proto::Node* GraphMirror::findNode(std::uint32_t uid) const
{
	for (const auto& node : data.nodes())
		if (node.uid() == uid)
			return &node;

	return nullptr;
}

bool GraphMirror::isConnected(const proto::Connection& connection) const
{
	for (const auto& c : data.connections())
	{
		if (c.source_node() == connection.source_node() &&
			c.source_channel() == connection.source_channel() &&
			c.destination_node() == connection.destination_node() &&
			c.destination_channel() == connection.destination_channel())
			return true;
	}

	return false;
}

bool GraphMirror::canConnect(const proto::Connection& connection) const
{
	if (isConnected(connection))
		return false;

	static constexpr int midiChannelIndex = -1;

	const auto source = findNode(connection.source_node());
	const auto destination = findNode(connection.destination_node());

	if (source == nullptr || destination == nullptr)
		return false;

	const auto sourceValid =
	    connection.source_channel() == midiChannelIndex
	        ? source->processor().produces_midi()
	        : (connection.source_channel() >= 0 && connection.source_channel() < source->processor().num_outs());

	const auto destinationValid =
	    connection.destination_channel() == midiChannelIndex
	        ? destination->processor().accepts_midi()
	        : (connection.destination_channel() >= 0
	          && connection.destination_channel() < destination->processor().num_ins());

	return sourceValid && destinationValid;
}

//==============================================================================
EngineClient::EngineClient()
    : Thread("EngineClient"),
      channel(grpc::CreateChannel("unix:" + getSocketPath(), grpc::InsecureChannelCredentials()))
{
	stub = proto::Engine::NewStub(channel);
}

EngineClient::~EngineClient()
{
	stop();
}

bool EngineClient::start()
{
	auto enginePath =
	    File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("EffectBoxEngine");

	if (!enginePath.existsAsFile())
		return false;

	if (!engineProcess.start(StringArray{enginePath.getFullPathName()}, ChildProcess::wantStdOut))
		return false;

	startThread();
	return true;
}

void EngineClient::stop()
{
	connected = false;

	// cancel the event stream so the pump thread unblocks from Read(); this
	// works whether the engine is our child process or an external one
	if (eventsContext != nullptr)
		eventsContext->TryCancel();

	// kill the engine we started so its socket goes away
	if (engineProcess.isRunning())
		engineProcess.kill();

	stopThread(2000);
	*alive = false;
}

void EngineClient::run()
{
	// the socket file appears once the engine's gRPC server is up
	auto socketPath = getSocketPath();

	while (!threadShouldExit() && !File{String{socketPath}}.existsAsFile())
		Thread::sleep(50);

	if (threadShouldExit())
		return;

	pumpEvents();
}

void EngineClient::pumpEvents()
{
	eventsContext = std::make_unique<grpc::ClientContext>();
	proto::GetEventsRequest request;
	auto reader = stub->GetEvents(eventsContext.get(), request);

	proto::EngineEvent event;

	while (!threadShouldExit() && reader->Read(&event))
	{
		connected = true;

		const auto copy = std::move(event);
		MessageManager::callAsync(
		    [alive = alive, this, copy]
		    {
			    if (*alive)
				    handleEvent(copy);
		    });
	}

	if (threadShouldExit())
		return; // shutting down, the stream was cancelled by us

	// the stream ended on its own: the engine is gone
	notifyEngineLost();
}

void EngineClient::handleEvent(proto::EngineEvent event)
{
	switch (event.event_case())
	{
	case proto::EngineEvent::kPluginGraphChanged:
		mirror.applySnapshot(event.plugin_graph_changed().snapshot());
		break;

	case proto::EngineEvent::kPluginListChanged:
		plugins.assign(event.plugin_list_changed().plugins().begin(),
		               event.plugin_list_changed().plugins().end());
		break;

	case proto::EngineEvent::kPluginCreateFailed:
	{
		const auto& failed = event.plugin_create_failed();

		proto::PluginDescription info;
		info.set_name(failed.plugin());
		info.set_format_name(failed.format_name());

		if (onPluginCreateFailed != nullptr)
			onPluginCreateFailed(info, String{failed.error()});

		break;
	}

	default:
		break;
	}
}

void EngineClient::notifyEngineLost()
{
	connected = false;

	if (onEngineLost != nullptr)
	{
		auto callback = std::move(onEngineLost);
		MessageManager::callAsync(
		    [alive = alive, callback = std::move(callback)]
		    {
			    if (*alive)
				    callback();
		    });
	}
}

//==============================================================================
void EngineClient::addPlugin(const proto::PluginDescription& plugin, double x, double y)
{
	grpc::ClientContext context;

	proto::AddPluginRequest request;
	request.set_identifier_string(plugin.identifier_string());
	request.set_x(x);
	request.set_y(y);

	proto::AddPluginResponse response;
	stub->AddPlugin(&context, request, &response);
}

void EngineClient::removeNode(std::uint32_t uid)
{
	grpc::ClientContext context;

	proto::RemoveNodeRequest request;
	request.set_node_id(static_cast<int32_t>(uid));

	proto::RemoveNodeResponse response;
	stub->RemoveNode(&context, request, &response);
}

void EngineClient::disconnectNode(std::uint32_t uid)
{
	grpc::ClientContext context;

	proto::DisconnectNodeRequest request;
	request.set_node_id(static_cast<int32_t>(uid));

	proto::DisconnectNodeResponse response;
	stub->DisconnectNode(&context, request, &response);
}

void EngineClient::addConnection(const proto::Connection& connection)
{
	grpc::ClientContext context;

	proto::AddConnectionRequest request;
	*request.mutable_connection() = connection;

	proto::AddConnectionResponse response;
	stub->AddConnection(&context, request, &response);
}

void EngineClient::removeConnection(const proto::Connection& connection)
{
	grpc::ClientContext context;

	proto::RemoveConnectionRequest request;
	*request.mutable_connection() = connection;

	proto::RemoveConnectionResponse response;
	stub->RemoveConnection(&context, request, &response);
}

void EngineClient::setNodePosition(std::uint32_t uid, double x, double y)
{
	grpc::ClientContext context;

	proto::SetNodePositionRequest request;
	request.set_node_id(uid);
	request.set_x(x);
	request.set_y(y);

	proto::SetNodePositionResponse response;
	stub->SetNodePosition(&context, request, &response);
}

void EngineClient::addIOEndpoint(const String& name, int numChannels, bool isInput)
{
	grpc::ClientContext context;

	proto::AddIOEndpointRequest request;
	request.set_name(name.toStdString());
	request.set_channels(numChannels);
	request.set_is_input(isInput);

	proto::AddIOEndpointResponse response;
	stub->AddIOEndpoint(&context, request, &response);
}

void EngineClient::removeIOEndpoint(const Uuid& id, bool isInput)
{
	grpc::ClientContext context;

	proto::RemoveIOEndpointRequest request;
	request.set_endpoint_id(id.toString().toStdString());
	request.set_is_input(isInput);

	proto::RemoveIOEndpointResponse response;
	stub->RemoveIOEndpoint(&context, request, &response);
}

void EngineClient::setIOEndpointName(const Uuid& id, const String& name, bool isInput)
{
	grpc::ClientContext context;

	proto::SetIOEndpointNameRequest request;
	request.set_endpoint_id(id.toString().toStdString());
	request.set_name(name.toStdString());
	request.set_is_input(isInput);

	proto::SetIOEndpointNameResponse response;
	stub->SetIOEndpointName(&context, request, &response);
}

void EngineClient::setIOEndpointChannels(const Uuid& id, int numChannels, bool isInput)
{
	grpc::ClientContext context;

	proto::SetIOEndpointChannelsRequest request;
	request.set_endpoint_id(id.toString().toStdString());
	request.set_channels(numChannels);
	request.set_is_input(isInput);

	proto::SetIOEndpointChannelsResponse response;
	stub->SetIOEndpointChannels(&context, request, &response);
}

void EngineClient::openPluginWindow(std::uint32_t uid, int windowType)
{
	grpc::ClientContext context;

	proto::OpenPluginWindowRequest request;
	request.set_node_id(uid);
	request.set_type(static_cast<proto::PluginWindowType>(windowType));

	proto::OpenPluginWindowResponse response;
	stub->OpenPluginWindow(&context, request, &response);
}

void EngineClient::closeAllPluginWindows()
{
	grpc::ClientContext context;

	proto::CloseAllPluginWindowsRequest request;
	proto::CloseAllPluginWindowsResponse response;
	stub->CloseAllPluginWindows(&context, request, &response);
}

void EngineClient::toggleBypass(std::uint32_t uid)
{
	grpc::ClientContext context;

	proto::ToggleBypassRequest request;
	request.set_node_id(uid);

	proto::ToggleBypassResponse response;
	stub->ToggleBypass(&context, request, &response);
}

bool EngineClient::savePluginState(std::uint32_t uid, String& outBase64)
{
	grpc::ClientContext context;

	proto::SavePluginStateRequest request;
	request.set_node_id(uid);

	proto::SavePluginStateResponse response;

	if (!stub->SavePluginState(&context, request, &response).ok())
		return false;

	outBase64 = String{response.data_base64()};
	return true;
}

void EngineClient::loadPluginState(std::uint32_t uid, const String& base64)
{
	grpc::ClientContext context;

	proto::LoadPluginStateRequest request;
	request.set_node_id(uid);
	request.set_data_base64(base64.toStdString());

	proto::LoadPluginStateResponse response;
	stub->LoadPluginState(&context, request, &response);
}
