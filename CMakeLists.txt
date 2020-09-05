#
# SETUP
#

# initialization
cmake_minimum_required(VERSION 3.16)
project(retrolab
        VERSION 0.9.13
        LANGUAGES C
        HOMEPAGE_URL https://retrolab.gamesmith.uk/)

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
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}" -DHOMEPAGE="${CMAKE_PROJECT_HOMEPAGE_URL}")

#
# INTERMEDIATE FILES
#

# compile retrolab compiler
flex_target(lexer compiler/retrolab.l ${CMAKE_CURRENT_BINARY_DIR}/lex.c DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lex.h)
bison_target(parser compiler/retrolab.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
add_flex_bison_dependency(lexer parser)

# build font file
add_custom_command(
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/font.h
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/emulator && xxd -i font.bmp > ${CMAKE_CURRENT_BINARY_DIR}/font.h)

# build constants files
add_custom_command(
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/constants/constants.yaml ${CMAKE_CURRENT_SOURCE_DIR}/constants/constants.py
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/mmap.h
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/constants && python3 ./constants.py --lang c > ${CMAKE_CURRENT_BINARY_DIR}/mmap.h
)
add_custom_command(
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/constants/constants.yaml ${CMAKE_CURRENT_SOURCE_DIR}/constants/constants.py
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/retrolab.def.h
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/constants && python3 ./constants.py --lang asm_c > ${CMAKE_CURRENT_BINARY_DIR}/retrolab.def.h
)

# define C source files
set(SOURCES
        emulator/breakpoints.c
        emulator/cpu.c
        emulator/emulator.c
        emulator/joystick.c
        emulator/keyboard.c
        emulator/memory.c
        emulator/timer.c
        emulator/video.c
        compiler/compctx.c
        compiler/compiler.c
        compiler/debug.c
        compiler/input.c
        compiler/output.c
        compiler/parameter.c
        compiler/symtbl.c
        ${BISON_parser_OUTPUT_SOURCE}
        ${FLEX_lexer_OUTPUTS})

set(HEADERS
        emulator/breakpoints.h
        emulator/cpu.h
        emulator/emulator.h
        emulator/interrupts.h
        emulator/joystick.h
        emulator/keyboard.h
        emulator/memory.h
        emulator/timer.h
        emulator/video.h
        compiler/compctx.h
        compiler/compiler.h
        compiler/debug.h
        compiler/error.h
        compiler/input.h
        compiler/output.h
        compiler/parameter.h
        compiler/symtbl.h
        compiler/bytearray.h
        ${BISON_parser_OUTPUT_HEADER}
        ${CMAKE_CURRENT_BINARY_DIR}/font.h
        ${CMAKE_CURRENT_BINARY_DIR}/mmap.h
        ${CMAKE_CURRENT_BINARY_DIR}/retrolab.def.h)

#
# OUTPUTS
#

# version
add_custom_target(retrolab-version
        COMMAND echo ${PROJECT_VERSION})

# retrolab.def
add_custom_target(retrolab-def
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/constants && python3 ./constants.py --lang asm > ${CMAKE_CURRENT_BINARY_DIR}/retrolab-${CMAKE_PROJECT_VERSION}.def)

# retrolab executable
add_executable(retrolab main.c exec/exec.c ${SOURCES} ${HEADERS} exec/exec.c exec/exec.h)
target_compile_options(retrolab PRIVATE -Wall -Wextra)
target_link_libraries(retrolab ${SDL2_LIBRARIES})

# tests
enable_testing()
add_executable(retrolab_test tests.c exec/exec.c ${SOURCES} ${HEADERS})
target_compile_options(retrolab_test PRIVATE -Wall -Wextra -DHEADLESS -DTESTING)
target_link_libraries(retrolab_test ${SDL2_LIBRARIES})
add_test(test retrolab_test)

add_custom_target(test-video
        DEPENDS retrolab
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/retrolab -s ${CMAKE_CURRENT_SOURCE_DIR}/asm/demo.s)

add_custom_target(test-leaks
        DEPENDS retrolab_test
        COMMAND valgrind --leak-check=full --show-leak-kinds=all --suppressions=${CMAKE_CURRENT_SOURCE_DIR}/retrolab.supp ${CMAKE_CURRENT_BINARY_DIR}/retrolab_test
        USES_TERMINAL)

# test sanitizer
add_executable(retrolab_test_sanitize tests.c exec/exec.c ${SOURCES} ${HEADERS})
target_compile_options(retrolab_test_sanitize PRIVATE -Wall -Wextra -DHEADLESS -DTESTING -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(retrolab_test_sanitize -lasan ${SDL2_LIBRARIES})

# WASM
set(CMAKE_VERBOSE_MAKEFILE ON)
set(WASM_SOURCES ${SOURCES} emulator/glue.c)
set(WASM_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/retrolab-${CMAKE_PROJECT_VERSION})
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES ${WASM_OUTPUT}.js)
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES ${WASM_OUTPUT}.wasm)
set(WASM_OPTIONS -o ${WASM_OUTPUT}.js -Wall -Wextra -s WASM=1 -s USE_SDL=2 -s EXTRA_EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"print\",\"printErr\",\"addFunction\",\"free\"]' -s EXPORTED_FUNCTIONS='[\"_free\",\"_malloc\"]' -s NO_EXIT_RUNTIME=1 -s RESERVED_FUNCTION_POINTERS=10 -I${CMAKE_CURRENT_BINARY_DIR} -Icompiler -Iemulator -DVERSION=\\"${CMAKE_PROJECT_VERSION}\\" -s ALLOW_TABLE_GROWTH=1 -s ALLOW_MEMORY_GROWTH=1)
set(WASM_RELEASE -O3)
set(WASM_DEBUG -O0 -g4 --source-map-base 'http://localhost:8080/' -fno-omit-frame-pointer -s ASSERTIONS=1)
set(WASM_SANITIZER -fsanitize=address)
set(WASM_DEPENDENCIES ${WASM_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/font.h ${CMAKE_CURRENT_BINARY_DIR}/mmap.h ${CMAKE_CURRENT_BINARY_DIR}/retrolab.def.h)
add_custom_target(wasm-debug
        DEPENDS ${WASM_DEPENDENCIES}
        COMMAND emcc ${WASM_OPTIONS} ${WASM_DEBUG} ${WASM_SOURCES}# ${WASM_SANITIZER}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_target(wasm-release
        DEPENDS ${WASM_DEPENDENCIES}
        COMMAND emcc ${WASM_OPTIONS} ${WASM_RELEASE} ${WASM_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# new version
add_custom_target(web-zip-debug
        DEPENDS wasm-debug retrolab-def
        COMMAND rm -f retrolab-wasm.zip && zip retrolab-wasm.zip retrolab-${CMAKE_PROJECT_VERSION}.js retrolab-${CMAKE_PROJECT_VERSION}.wasm retrolab-${CMAKE_PROJECT_VERSION}.wasm.map retrolab-${CMAKE_PROJECT_VERSION}.def)
add_custom_target(web-zip-release
        DEPENDS wasm-release retrolab-def
        COMMAND rm -f retrolab-wasm.zip && zip retrolab-wasm.zip retrolab-${CMAKE_PROJECT_VERSION}.js retrolab-${CMAKE_PROJECT_VERSION}.wasm retrolab-${CMAKE_PROJECT_VERSION}.def)
