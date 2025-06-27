cmake_minimum_required(VERSION 3.31)
project(authentication_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")

find_package(nlohmann_json CONFIG REQUIRED)

set(SOURCES
    main.cpp
    utilities/JsonHelper.cpp
    utilities/HashUtils.cpp
)

set(HEADERS
    enums/UserType.h
    structs/JsonMessage.h
    structs/User.h
    structs/StatusResponse.h
    utilities/JsonHelper.h
    utilities/HashUtils.h
)

add_executable(authentication_server
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(authentication_server PRIVATE
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
)