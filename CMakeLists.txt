cmake_minimum_required(VERSION 3.13)

set( CMAKE_CXX_STANDARD 17 )

project(madco2 VERSION 0.1 )

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(json)

# install via brew install openssl
set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)

# install via brew install cpprestsdk
find_package(cpprestsdk REQUIRED)

# install via brew install boost
find_package(Boost REQUIRED COMPONENTS system thread filesystem)

set(_files
    main.cpp
    hidapi++.h
    server.h
    co2gatherer.h
    index.html
    Chart.bundle.min.js)

if(APPLE)
    set(_files ./hidapi/mac/hid.c ${_files})
endif()

add_executable(madco2 ${_files})

configure_file(index.html index.html)
configure_file(Chart.bundle.min.js Chart.bundle.min.js)

target_link_libraries( madco2 PRIVATE
    nlohmann_json::nlohmann_json
    cpprestsdk::cpprest
    Boost::system
    Boost::thread
    Boost::filesystem )

if(APPLE)
    target_link_libraries(madco2 PRIVATE
        "-framework IOKit")
endif()

target_include_directories(madco2 PRIVATE ./hidapi/hidapi)

