cmake_minimum_required(VERSION 3.16)

project(g729
    VERSION 0.1.0
    DESCRIPTION "Clean-room G.729 Annex A-compatible C codec library"
    LANGUAGES C)

include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(G729_TOP_LEVEL_DEFAULT ON)
else()
    set(G729_TOP_LEVEL_DEFAULT OFF)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(G729_BUILD_TOOLS "Build g729enc, g729dec, and g729bench" ON)
option(G729_BUILD_TESTS "Build test programs" ${G729_TOP_LEVEL_DEFAULT})
option(G729_BUILD_EXAMPLES "Build example programs" ${G729_TOP_LEVEL_DEFAULT})
option(G729_ENABLE_WARNINGS "Enable compiler warning flags" ON)
option(G729_ENABLE_WERROR "Treat compiler warnings as errors" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(G729_SOURCES
    src/g729.c
    src/g729_bitstream.c
    src/g729_closedloop.c
    src/g729_fcb.c
    src/g729_fcb_search.c
    src/g729_fixed.c
    src/g729_gain.c
    src/g729_gain_quant.c
    src/g729_gain_tables.c
    src/g729_hp.c
    src/g729_lpc.c
    src/g729_lsp.c
    src/g729_lsp_tables.c
    src/g729_openloop.c
    src/g729_pcm.c
    src/g729_pitch.c
    src/g729_postfilter.c
    src/g729_synth.c)

function(g729_target_warnings target)
    if(NOT G729_ENABLE_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(G729_ENABLE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(G729_ENABLE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(g729 ${G729_SOURCES})
add_library(g729::g729 ALIAS g729)
target_include_directories(g729
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(g729 PROPERTIES
    OUTPUT_NAME g729
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    PUBLIC_HEADER include/g729.h)
g729_target_warnings(g729)

if(G729_BUILD_TOOLS OR G729_BUILD_TESTS)
    add_executable(g729enc tools/g729enc.c)
    target_link_libraries(g729enc PRIVATE g729)
    g729_target_warnings(g729enc)

    add_executable(g729dec tools/g729dec.c)
    target_link_libraries(g729dec PRIVATE g729)
    g729_target_warnings(g729dec)

    add_executable(g729bench tools/g729bench.c)
    target_link_libraries(g729bench PRIVATE g729)
    g729_target_warnings(g729bench)
endif()

if(G729_BUILD_EXAMPLES)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    add_executable(api_smoke examples/api_smoke.c)
    target_link_libraries(api_smoke PRIVATE g729)
    g729_target_warnings(api_smoke)

    add_executable(cpp_smoke examples/cpp_smoke.cpp)
    target_link_libraries(cpp_smoke PRIVATE g729)
    if(G729_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(cpp_smoke PRIVATE /W4)
            if(G729_ENABLE_WERROR)
                target_compile_options(cpp_smoke PRIVATE /WX)
            endif()
        else()
            target_compile_options(cpp_smoke PRIVATE -Wall -Wextra)
            if(G729_ENABLE_WERROR)
                target_compile_options(cpp_smoke PRIVATE -Werror)
            endif()
        endif()
    endif()
endif()

if(G729_BUILD_TESTS)
    include(CTest)
    enable_testing()

    set(G729_C_TESTS
        test_api
        test_bitstream
        test_closedloop
        test_decode
        test_encode
        test_fcb
        test_fcb_search
        test_fixed
        test_gain
        test_gain_quant
        test_hp
        test_lpc
        test_loopback
        test_lsp
        test_lsp_encode
        test_openloop
        test_pcm
        test_pitch
        test_postfilter
        test_stress
        test_synth)

    foreach(test_name IN LISTS G729_C_TESTS)
        add_executable(${test_name} tests/${test_name}.c)
        target_link_libraries(${test_name} PRIVATE g729)
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        g729_target_warnings(${test_name})
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    add_executable(test_cli_decode tests/test_cli_decode.c)
    target_link_libraries(test_cli_decode PRIVATE g729)
    target_include_directories(test_cli_decode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_cli_decode PRIVATE
        "G729DEC_BIN=\"$<TARGET_FILE:g729dec>\""
        "G729_CLI_TEST_DIR=\"$<TARGET_FILE_DIR:test_cli_decode>\"")
    add_dependencies(test_cli_decode g729dec)
    g729_target_warnings(test_cli_decode)
    add_test(NAME test_cli_decode COMMAND test_cli_decode)

    add_executable(test_cli_encode tests/test_cli_encode.c)
    target_link_libraries(test_cli_encode PRIVATE g729)
    target_include_directories(test_cli_encode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(test_cli_encode PRIVATE
        "G729ENC_BIN=\"$<TARGET_FILE:g729enc>\""
        "G729_CLI_TEST_DIR=\"$<TARGET_FILE_DIR:test_cli_encode>\"")
    add_dependencies(test_cli_encode g729enc)
    g729_target_warnings(test_cli_encode)
    add_test(NAME test_cli_encode COMMAND test_cli_encode)

    if(TARGET api_smoke)
        add_test(NAME api_smoke COMMAND api_smoke)
    endif()
    if(TARGET cpp_smoke)
        add_test(NAME cpp_smoke COMMAND cpp_smoke)
    endif()
endif()

install(TARGETS g729
    EXPORT g729Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(G729_BUILD_TOOLS)
    install(TARGETS g729enc g729dec g729bench
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/g729ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/g729Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/g729Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/g729")

install(EXPORT g729Targets
    NAMESPACE g729::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/g729")

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/g729Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/g729ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/g729")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/g729.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/g729.pc"
    @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/g729.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
