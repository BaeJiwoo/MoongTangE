# Build the compiler and C++ runtime from the same release. No system package fallback.
include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(absl
  URL https://codeload.github.com/abseil/abseil-cpp/tar.gz/76bb24329e8bf5f39704eb10d21b9a80befa7c81
  URL_HASH SHA256=ed8f7d9f39139c449e79fd19765e23c96fdb774172d32d191323d3e3ea06e5ff
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(absl)

set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBUPB ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
set(protobuf_LOCAL_DEPENDENCIES_ONLY ON CACHE BOOL "" FORCE)
FetchContent_Declare(protobuf
  URL https://codeload.github.com/protocolbuffers/protobuf/tar.gz/2c74169b34066ceb8ddb6b882fcb3fb32d737a55
  URL_HASH SHA256=ef957adfc3ec0e2794031697e6a94bbc213103cead96c26a1fccb0dbdadd3404
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(protobuf)

set(sandbox_proto_dir "${PROJECT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${sandbox_proto_dir}")
set(sandbox_schema "${PROJECT_SOURCE_DIR}/Network/Protocol/game.proto")
add_custom_command(
  OUTPUT "${sandbox_proto_dir}/game.pb.cc" "${sandbox_proto_dir}/game.pb.h"
  COMMAND protobuf::protoc
    "--cpp_out=${sandbox_proto_dir}"
    "--proto_path=${PROJECT_SOURCE_DIR}/Network/Protocol"
    "${sandbox_schema}"
  DEPENDS protobuf::protoc "${sandbox_schema}"
  VERBATIM)
add_library(ProtocolMessages "${sandbox_proto_dir}/game.pb.cc")
target_include_directories(ProtocolMessages SYSTEM PUBLIC "${sandbox_proto_dir}")
target_link_libraries(ProtocolMessages PUBLIC protobuf::libprotobuf)

# Generated Protobuf types are private to the codec. Game state remains plain C++.
add_library(NetworkProtocol "${PROJECT_SOURCE_DIR}/Network/Protocol/Codec.cpp")
target_link_libraries(NetworkProtocol PUBLIC Common PRIVATE ProtocolMessages)
