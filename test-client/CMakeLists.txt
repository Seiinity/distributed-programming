cmake_minimum_required(VERSION 3.31)
project(test_client)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")

find_package(nlohmann_json CONFIG REQUIRED)

set(SOURCES
    main.cpp
    GameClient.cpp
    utilities/Utilities.cpp
)

set(HEADERSs
    GameClient.h
    utilities/Utilities.h
)

add_executable(test_client
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(test_client PRIVATE
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Windows>:ws2_32>
)