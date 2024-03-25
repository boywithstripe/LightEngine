cmake_minimum_required(VERSION 3.20)
project(LightEngine)

aux_source_directory(${PROJECT_SOURCE_DIR} SRC)

set(LIBPATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECPATH ${PROJECT_SOURCE_DIR}/bin)
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS" )
add_definitions("-DUNICODE" "-D_UNICODE")

include_directories(${PROJECT_SOURCE_DIR}/Common)
include_directories(${PROJECT_SOURCE_DIR}/Thirdparty/D3D12)
add_subdirectory(Common)

add_executable(LightEngine ${SRC})
target_link_libraries(LightEngine Common)