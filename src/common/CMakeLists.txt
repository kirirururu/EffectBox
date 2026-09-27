find_package(gRPC CONFIG REQUIRED)

add_library(EffectBoxCommon
	EffectBoxTypes.proto
	EffectBoxRPC.proto
	Common.cpp
)

protobuf_generate(TARGET EffectBoxCommon)
protobuf_generate(
	TARGET EffectBoxCommon
	LANGUAGE grpc
	PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
	PLUGIN_OPTIONS generate_mock_code=true
	GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)

target_include_directories(EffectBoxCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(EffectBoxCommon PUBLIC gRPC::grpc++)
