cmake_minimum_required(VERSION 3.20)

aux_source_directory(. SRC)
set(LIBRARY_OUTPUT_PATH ${LIBPATH})
add_library(Common STATIC ${SRC})
# add_executable(Common ${SRC})

