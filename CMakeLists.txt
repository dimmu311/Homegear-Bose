cmake_minimum_required(VERSION 3.8)
project(homegear_bose)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES
        src/PhysicalInterfaces/EventServer.cpp
        src/PhysicalInterfaces/EventServer.h
        src/PhysicalInterfaces/IBoseInterface.cpp
        src/PhysicalInterfaces/IBoseInterface.h
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/Interfaces.cpp
        src/Interfaces.h
        src/Bose.cpp
        src/Bose.h
        src/BoseCentral.cpp
        src/BoseCentral.h
        src/BosePacket.cpp
        src/BosePacket.h
        src/BosePeer.cpp
        src/BosePeer.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})
set(CMAKE_SHARED_LIBRARY_PREFIX "mod_")

add_library(bose SHARED ${SOURCE_FILES})
install(TARGETS bose DESTINATION /var/lib/homegear/modules)