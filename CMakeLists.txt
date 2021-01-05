
cmake_minimum_required(VERSION 3.5.1)

project(License C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Default build type: RelWithDebInfo" FORCE)
endif()

# run only on linux
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(Threads REQUIRED)

# find protobuf installation
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")


# cause cmake version below 3.19.x don't stop when it found no program, we need to check failed error manufatually.
find_program(_PROTOBUF_PROTOC protoc REQUIRED)
if (${_PROTOBUF_PROTOC} STREQUAL "_PROTOBUF_PROTOC-NOTFOUND")
    message(FATAL_ERROR "No Found program:protoc")
endif()

find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
if (${_GRPC_CPP_PLUGIN_EXECUTABLE} STREQUAL "_GRPC_CPP_PLUGIN_EXECUTABLE-NOTFOUND")
    message(FATAL_ERROR "No Found program:grpc_cpp_plugin")
endif()

# Find gRPC installation
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")


# proto file
get_filename_component(license_proto "proto/license.proto" ABSOLUTE)
get_filename_component(license_proto_path "${license_proto}" PATH)

# generate protobuf files
set(license_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/license.pb.cc")
set(license_proto_headers "${CMAKE_CURRENT_BINARY_DIR}/license.pb.h")
set(license_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/license.grpc.pb.cc")
set(license_grpc_headers "${CMAKE_CURRENT_BINARY_DIR}/license.grpc.pb.h")
add_custom_command(
      OUTPUT "${license_proto_srcs}" "${license_proto_headers}" "${license_grpc_srcs}" "${license_grpc_headers}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${license_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${license_proto}"
      DEPENDS "${license_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
# Include other dependency header files
include_directories("${CMAKE_SOURCE_DIR}/include")

# compile 3rdparty into static library
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/spdlog)
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/googletest)

# set dependency library  
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
set(_GRPC_GRPCPP gRPC::grpc++)
set(LOGGER "spdlog::spdlog")
set(GTEST "gtest")

set(ClientTarget "TestLicenseClient")
set(ClientTestMain "test/client_test.cc")
set(ClientSrc "src/client.cc")
add_executable(${ClientTarget} 
    ${ClientSrc} 
    ${ClientTestMain}
    ${license_proto_srcs} 
    ${license_grpc_srcs})
target_link_libraries(${ClientTarget}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${LOGGER}
    ${GTEST})


set(ServerTarget "LicenseServer")
set(ServerSrc "src/server.cc")
add_executable(${ServerTarget} ${ServerSrc} 
    ${license_proto_srcs} 
    ${license_grpc_srcs})
target_link_libraries(${ServerTarget}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${LOGGER})


