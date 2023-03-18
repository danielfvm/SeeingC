cmake_minimum_required(VERSION 2.8) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
PROJECT(Seeing)

set(NAME_SRC src/WebServer.cpp src/serial.cpp src/Image.cpp src/Settings.cpp src/Profil.cpp src/main.cpp)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories(${CMAKE_BINARY_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
add_executable(seeing ${NAME_SRC} ${NAME_HEADERS})

target_compile_features(seeing PRIVATE cxx_std_17)
target_link_libraries(seeing "/usr/lib/libturbojpeg.so.0.3.0" "/usr/lib/libtiff.so" "/usr/lib/libASICamera2.so" "/usr/lib/libcfitsio.so" )
