cmake_minimum_required(VERSION 3.17)
Project(OpenEphysZMQReader)

include(FetchContent)

FetchContent_Declare(
        cppzmq
        GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
        GIT_SHALLOW	ON
        GIT_TAG v4.6.0
)

FetchContent_GetProperties(cppzmq)
if(NOT cppzmq_POPULATED)
    message(STATUS "Populating zmq-cpp...")
    FetchContent_Populate(cppzmq)
    # Add here options for cppzmq building
    set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "Whether or not to build the tests")
    add_subdirectory(${cppzmq_SOURCE_DIR} ${cppzmq_BINARY_DIR})
    message(STATUS "Done.")
endif()

include_directories(BEFORE SYSTEM "${cppzmq_SOURCE_DIR}")
target_link_libraries(${ZMQ} cppzmq)


FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent.git
        GIT_TAG v3.7.3)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    FetchContent_Populate(json)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()




add_library(ZMQReader ZMQReader.cpp)
target_link_libraries(ZMQReader nlohmann_json::nlohmann_json)

add_executable(OpenEphysZMQReader main.cpp)
target_link_libraries(OpenEphysZMQReader ZMQReader)
