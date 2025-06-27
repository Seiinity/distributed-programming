cmake_minimum_required(VERSION 3.31)
project(game_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")

find_package(nlohmann_json CONFIG REQUIRED)

set(SOURCES
    main.cpp
    utilities/JsonHelper.cpp
    utilities/GUIDUtils.cpp
    AuthServerClient.cpp
)

set(HEADERS
    structs/Item.h
    enums/PlayerType.h
    structs/Player.h
    structs/JsonMessage.h
    utilities/JsonHelper.h
    utilities/GUIDUtils.h
    structs/StatusResponse.h
    AuthServerClient.h
    structs/ConnectionInfo.h
)

add_executable(game_server
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(game_server PRIVATE
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
    $<$<PLATFORM_ID:Windows>:rpcrt4>
)
