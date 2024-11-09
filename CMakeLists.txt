cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(PROJECT_NAME SokuStreaming)
project("${PROJECT_NAME}" C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include_directories(include shady-packer/src/Core "${CMAKE_BINARY_DIR}/shady-packer/thirdparty/zlib")
add_definitions(-DWINVER=0x600 -D_WIN32_WINNT=0x600)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND "${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++11-narrowing -Wno-microsoft-cast")
endif ()
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /Brepro")
SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /Brepro")
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /Brepro")

# SokuLib
add_subdirectory(SokuLib)

# shady-core
set(SHADY_ENABLE_EXECUTABLE OFF)
set(SHADY_ENABLE_MODULE OFF)
set(ENABLE_BZ2 OFF)
set(ENABLE_LZMA OFF)
set(ENABLE_ZSTD OFF)
set(ENABLE_COMMONCRYPTO OFF)
set(ENABLE_GNUTLS OFF)
set(ENABLE_MBEDTLS OFF)
set(ENABLE_OPENSSL OFF)
set(ENABLE_WINDOWS_CRYPTO OFF)
add_subdirectory(shady-packer)

# Module
add_library(
	"${PROJECT_NAME}"
	MODULE
	src/Network/Socket.cpp
	src/Network/Socket.hpp
	src/Exceptions.hpp
	src/Network/WebServer.cpp
	src/Network/WebServer.hpp
	src/Network/WebSocket.cpp
	src/Network/WebSocket.hpp
	src/Utils/ShiftJISDecoder.cpp
	src/Utils/ShiftJISDecoder.hpp
	src/main.cpp
	src/State.cpp
	src/State.hpp
	src/Network/Handlers.cpp
	src/Network/Handlers.hpp
	src/Utils/InputBox.cpp
	src/Utils/InputBox.hpp
)
target_compile_options("${PROJECT_NAME}" PRIVATE /Zi)
target_compile_definitions("${PROJECT_NAME}" PRIVATE DIRECTINPUT_VERSION=0x0800 CURL_STATICLIB _CRT_SECURE_NO_WARNINGS $<$<CONFIG:Debug>:_DEBUG>)
target_include_directories("${PROJECT_NAME}" PRIVATE include SokuLib/directx src)
target_link_directories("${PROJECT_NAME}" PRIVATE lib)
target_link_libraries(
	"${PROJECT_NAME}"
	shady-core
	SokuLib
	shlwapi
	d3d9
	d3dx9
	ws2_32
)