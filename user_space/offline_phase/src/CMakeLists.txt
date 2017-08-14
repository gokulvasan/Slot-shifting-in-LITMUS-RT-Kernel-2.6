
cmake_minimum_required(VERSION 2.6.0)

project(prep)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake )


add_definitions(-Wall -g)


#add_subdirectory(dbusif2cpp)
# set include directories and compiler definitions

set(testlibSrc app.cpp outFileGenerator.cpp )


add_library(testlib STATIC ${testlibSrc})


# build and install the executable
set(mySources main.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(prep.x ${mySources})


# this is needed, otherwise no time measurement
target_link_libraries(prep.x testlib -lrt)


#install(TARGETS setsched DESTINATION bin )
