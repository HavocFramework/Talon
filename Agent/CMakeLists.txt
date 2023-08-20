cmake_minimum_required( VERSION 3.23 )
project( Talon C )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_COMPILER x86_64-w64-mingw32-gcc )

include_directories(Include)
file(GLOB SOURCES "Source/*")

add_executable(Agent ${SOURCES})
target_link_libraries( Agent iphlpapi winhttp )