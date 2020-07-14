cmake_minimum_required(VERSION 3.16)
project(retrolab C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
include(FindBISON)
include(FindFLEX)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

flex_target(lexer compiler/retrolab.l ${CMAKE_CURRENT_BINARY_DIR}/lex.c DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lex.h)
bison_target(parser compiler/retrolab.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
add_flex_bison_dependency(lexer parser)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(".")
include_directories("./compiler")

set(CMAKE_C_STANDARD 11)
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
        ${FLEX_lexer_OUTPUTS})

add_executable(retrolab main.c ${SOURCES})
target_link_libraries(retrolab ${SDL2_LIBRARIES})