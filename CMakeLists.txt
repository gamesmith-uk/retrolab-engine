# initialization
cmake_minimum_required(VERSION 3.16)
project(retrolab C)

# install cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
include(FindBISON)
include(FindFLEX)

# find libraries
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# C flags
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(".")
include_directories("./compiler")
include_directories("./emulator")
set(CMAKE_C_STANDARD 11)

# compile retrolab compiler
flex_target(lexer compiler/retrolab.l ${CMAKE_CURRENT_BINARY_DIR}/lex.c DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lex.h)
bison_target(parser compiler/retrolab.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
add_flex_bison_dependency(lexer parser)

# build font file
add_custom_command(
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/font.h
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/emulator && xxd -i font.bmp > ${CMAKE_CURRENT_BINARY_DIR}/font.h)

# define C source files
set(SOURCES
        emulator/breakpoints.h emulator/breakpoints.c
        emulator/cpu.c         emulator/cpu.h
        emulator/emulator.c    emulator/emulator.h
        emulator/interrupts.h  emulator/joystick.c
        emulator/joystick.h    emulator/keyboard.c
        emulator/keyboard.h
        emulator/memory.c      emulator/memory.h
        emulator/timer.c       emulator/timer.h
        emulator/video.c       emulator/video.h
        compiler/compctx.c     compiler/compctx.h
        compiler/compiler.c    compiler/compiler.h
        compiler/debug.c       compiler/debug.h
        compiler/error.h
        compiler/input.c      compiler/input.h
        compiler/output.c     compiler/output.h
        compiler/parameter.c  compiler/parameter.h
        compiler/symtbl.c     compiler/symtbl.h
        compiler/bytearray.h
        ${BISON_parser_OUTPUTS}
        ${FLEX_lexer_OUTPUTS}
        ${CMAKE_CURRENT_BINARY_DIR}/font.h)

# output - retrolab executable
add_executable(retrolab main.c ${SOURCES})
target_compile_options(retrolab PRIVATE -Wall -Wextra)
target_link_libraries(retrolab ${SDL2_LIBRARIES})

# tests
enable_testing()
add_executable(retrolab_test tests.c ${SOURCES})
target_compile_options(retrolab_test PRIVATE -Wall -Wextra -DHEADLESS)
target_link_libraries(retrolab_test ${SDL2_LIBRARIES})
add_test(test retrolab_test)