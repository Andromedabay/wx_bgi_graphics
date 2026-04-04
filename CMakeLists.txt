cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(wx_bgigraphics_wrapper LANGUAGES C CXX)

# wxWidgets on macOS uses Objective-C++ (.mm files) for the Cocoa backend.
# CMake must have OBJCXX enabled before FetchContent builds wxWidgets, otherwise
# CMAKE_OBJCXX_COMPILE_OBJECT is never set and the wx configure step fails.
if(APPLE)
    enable_language(OBJCXX)
endif()

include(CTest)

include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps_fc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Internal test seams are intentionally OFF by default so release/shared binaries
# do not expose synthetic input injection entry points.
option(
    WXBGI_ENABLE_TEST_SEAMS
    "Enable internal test seam APIs for automated testing (never enable for public release artifacts)."
    OFF
)

# Use static MSVC runtime on Windows (/MT or /MTd) so the binary
# has no dependency on the MSVC Redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# Backend options
# ---------------------------------------------------------------------------

# Make wxWidgets the default window backend.
option(WXBGI_ENABLE_WX
    "Build with wxWidgets window backend (default: ON; fetches wxWidgets 3.2.5)"
    ON)

# GLFW standalone backend (auto-enabled when WX is OFF).
option(WXBGI_ENABLE_GLFW
    "Build with GLFW standalone window backend (default: ON when WXBGI_ENABLE_WX is OFF)"
    OFF)
if(NOT WXBGI_ENABLE_WX)
    set(WXBGI_ENABLE_GLFW ON CACHE BOOL "GLFW fallback" FORCE)
endif()

message(STATUS "Fetching GLEW...")

FetchContent_Declare(
   glew
   URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
   DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glew)

message(STATUS "Fetching GLFW...")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
   glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glfw)

# Apple Clang 16 (Xcode 16.x) tightened C conformance in Objective-C translation
# units (.m files) and now treats implicit void*→typed-pointer conversions as hard
# errors (-Wincompatible-pointer-types). GLFW 3.4's cocoa_monitor.m has several
# such casts (CFTypeRef→CFNumberRef, _glfw_calloc→CGDirectDisplayID* etc.) that
# predate this stricter enforcement. Suppress for the glfw target only.
if(APPLE)
    target_compile_options(glfw PRIVATE -Wno-incompatible-pointer-types)
endif()

set_target_properties(glfw PROPERTIES POSITION_INDEPENDENT_CODE ON)

message(STATUS "Fetching GLM...")

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glm)

find_package(OpenGL REQUIRED)

message(STATUS "Fetching nlohmann/json...")

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

message(STATUS "Fetching yaml-cpp...")

# yaml-cpp 0.8.0 uses cmake_minimum_required(VERSION 2.8.12) which CMake 4.0+
# rejects unless this policy variable is set.
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE INTERNAL "")

set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL       OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    yaml-cpp
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

# ---------------------------------------------------------------------------
# Optional wxWidgets 3.2.5 (fetched when WXBGI_ENABLE_WX=ON)
# ---------------------------------------------------------------------------
if(WXBGI_ENABLE_WX)
    message(STATUS "Fetching wxWidgets 3.2.5...")
    set(wxBUILD_SHARED  OFF CACHE BOOL "" FORCE)
    set(wxBUILD_TESTS   OFF CACHE STRING "" FORCE)
    set(wxBUILD_SAMPLES OFF CACHE STRING "" FORCE)
    set(wxBUILD_DEMOS   OFF CACHE STRING "" FORCE)
    FetchContent_Declare(
        wxWidgets
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG        v3.2.5
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(wxWidgets)
endif()

# Build a static GLEW libraryfrom fetched sources to keep the build self-contained.
add_library(glew_static STATIC ${glew_SOURCE_DIR}/src/glew.c)
add_library(GLEW::GLEW ALIAS glew_static)

set_target_properties(glew_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(glew_static
    PUBLIC
        ${glew_SOURCE_DIR}/include
)

target_compile_definitions(glew_static
    PUBLIC
        GLEW_STATIC
)

target_link_libraries(glew_static
    PUBLIC
        OpenGL::GL
)

if(UNIX AND NOT APPLE)
    target_link_libraries(glew_static PUBLIC ${CMAKE_DL_LIBS})
endif()

set(SRCS
    src/bgi_state.cpp
    src/bgi_draw.cpp
    src/bgi_font.cpp
    src/bgi_image.cpp
    src/bgi_api.cpp
    src/bgi_modern_api.cpp
    src/bgi_camera.cpp
    src/bgi_camera_api.cpp
    src/bgi_ucs.cpp
    src/bgi_ucs_api.cpp
    src/bgi_world_api.cpp
    src/bgi_dds.cpp
    src/bgi_dds_api.cpp
    src/bgi_dds_serial.cpp
    src/bgi_dds_render.cpp
    src/bgi_solid_api.cpp
    src/bgi_solid_render.cpp
    src/bgi_export.cpp
    src/bgi_gl.cpp
    src/bgi_overlay.cpp
    src/bgi_overlay_api.cpp
)

if(WXBGI_ENABLE_WX)
    list(APPEND SRCS
        src/wx/wx_bgi_canvas.cpp
        src/wx/bgi_wx_standalone.cpp
    )
endif()

add_library(wx_bgi_opengl SHARED ${SRCS})

target_include_directories(wx_bgi_opengl PUBLIC src)
target_compile_definitions(wx_bgi_opengl PRIVATE GLEW_STATIC)
target_link_libraries(wx_bgi_opengl PRIVATE GLEW::GLEW glfw OpenGL::GL glm::glm nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp)

if(WXBGI_ENABLE_WX)
    target_link_libraries(wx_bgi_opengl PRIVATE wxcore wxgl wxbase)
    target_include_directories(wx_bgi_opengl PRIVATE
        ${CMAKE_SOURCE_DIR}/src/wx
        ${glew_SOURCE_DIR}/include)
    target_compile_definitions(wx_bgi_opengl PRIVATE WXBGI_ENABLE_WX)
    set_target_properties(wx_bgi_opengl PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(WXBGI_ENABLE_TEST_SEAMS)
    # Expose compile-time guard to both the library and consumers of the public
    # header so declarations/definitions stay in sync during test builds.
    target_compile_definitions(wx_bgi_opengl PUBLIC WXBGI_ENABLE_TEST_SEAMS=1)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(wx_bgi_opengl PRIVATE ${CMAKE_DL_LIBS} pthread)
endif()

if(WIN32)
    set_target_properties(wx_bgi_opengl PROPERTIES PREFIX "")
endif()

add_executable(bgi_api_coverage_cpp examples/cpp/bgi_api_coverage.cpp)
target_link_libraries(bgi_api_coverage_cpp PRIVATE wx_bgi_opengl)

add_custom_command(
    TARGET bgi_api_coverage_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:bgi_api_coverage_cpp>
)

add_executable(wxbgi_keyboard_queue_cpp examples/cpp/wxbgi_keyboard_queue.cpp)
target_link_libraries(wxbgi_keyboard_queue_cpp PRIVATE wx_bgi_opengl)

add_custom_command(
    TARGET wxbgi_keyboard_queue_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:wxbgi_keyboard_queue_cpp>
)

add_executable(wxbgi_camera_demo_cpp examples/cpp/wxbgi_camera_demo.cpp)
target_link_libraries(wxbgi_camera_demo_cpp PRIVATE wx_bgi_opengl)

add_custom_command(
    TARGET wxbgi_camera_demo_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:wxbgi_camera_demo_cpp>
)

# Screenshot capture utility ---------------------------------------------------

add_executable(capture_screenshots tools/capture_screenshots.cpp)
target_link_libraries(capture_screenshots PRIVATE wx_bgi_opengl glfw)

add_custom_command(
    TARGET capture_screenshots POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:capture_screenshots>
)

# Phase E tests ---------------------------------------------------------------

add_executable(test_dds_serialize examples/cpp/test_dds_serialize.cpp)
target_link_libraries(test_dds_serialize PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET test_dds_serialize POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_dds_serialize>
)

add_executable(test_dds_deserialize examples/cpp/test_dds_deserialize.cpp)
target_link_libraries(test_dds_deserialize PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET test_dds_deserialize POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_dds_deserialize>
)

add_executable(test_dds_clear examples/cpp/test_dds_clear.cpp)
target_link_libraries(test_dds_clear PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET test_dds_clear POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_dds_clear>
)

add_executable(test_dds_cam2d_yz examples/cpp/test_dds_cam2d_yz.cpp)
target_link_libraries(test_dds_cam2d_yz PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET test_dds_cam2d_yz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_dds_cam2d_yz>
)

add_executable(test_dds_cam3d_persp examples/cpp/test_dds_cam3d_persp.cpp)
target_link_libraries(test_dds_cam3d_persp PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET test_dds_cam3d_persp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_dds_cam3d_persp>
)

add_executable(test_solids examples/cpp/test_solids.cpp)
target_link_libraries(test_solids PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET test_solids POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_solids>
)

# Input hook automated test (Phase 2 requires WXBGI_ENABLE_TEST_SEAMS)
add_executable(test_input_hooks examples/cpp/test_input_hooks.cpp)
target_link_libraries(test_input_hooks PRIVATE wx_bgi_opengl)
target_compile_definitions(test_input_hooks PRIVATE WXBGI_ENABLE_TEST_SEAMS=1)
add_custom_command(
    TARGET test_input_hooks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_input_hooks>
)

# Input hook interactive demo
add_executable(wxbgi_input_hooks_cpp examples/cpp/wxbgi_input_hooks.cpp)
target_link_libraries(wxbgi_input_hooks_cpp PRIVATE wx_bgi_opengl)
add_custom_command(
    TARGET wxbgi_input_hooks_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:wxbgi_input_hooks_cpp>
)

# Scroll + bypass + hk_dds automated test (requires WXBGI_ENABLE_TEST_SEAMS)
add_executable(test_input_bypass examples/cpp/test_input_bypass.cpp)
target_link_libraries(test_input_bypass PRIVATE wx_bgi_opengl)
target_compile_definitions(test_input_bypass PRIVATE WXBGI_ENABLE_TEST_SEAMS=1)
add_custom_command(
    TARGET test_input_bypass POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:test_input_bypass>
)

find_package(Python3 COMPONENTS Interpreter)

find_package(Doxygen QUIET)
find_package(LATEX COMPONENTS PDFLATEX QUIET)

if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/doxygen)
    set(DOXYGEN_CONFIG_FILE ${CMAKE_BINARY_DIR}/Doxyfile)

    configure_file(
        ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in
        ${DOXYGEN_CONFIG_FILE}
        @ONLY
    )

    add_custom_target(
        api_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating Doxygen API documentation"
        VERBATIM
    )

    if(PDFLATEX_COMPILER)
        if(WIN32)
            add_custom_target(
                api_docs_pdf
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
                COMMAND ${CMAKE_COMMAND} -E chdir ${DOXYGEN_OUTPUT_DIR}/latex make.bat
                COMMENT "Generating Doxygen PDF documentation"
                VERBATIM
            )
        else()
            add_custom_target(
                api_docs_pdf
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
                COMMAND ${CMAKE_COMMAND} -E chdir ${DOXYGEN_OUTPUT_DIR}/latex ${CMAKE_MAKE_PROGRAM}
                COMMENT "Generating Doxygen PDF documentation"
                VERBATIM
            )
        endif()
    else()
        add_custom_target(
            api_docs_pdf
            COMMAND ${CMAKE_COMMAND} -E echo "pdflatex not found. Install a LaTeX distribution (e.g., TeX Live/MiKTeX) and re-run CMake configure."
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "LaTeX (pdflatex) is required for api_docs_pdf target"
            VERBATIM
        )
    endif()
else()
    add_custom_target(
        api_docs
        COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found. Install Doxygen and re-run CMake configure."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Doxygen is required for api_docs target"
        VERBATIM
    )

    add_custom_target(
        api_docs_pdf
        COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found. Install Doxygen and re-run CMake configure."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Doxygen is required for api_docs_pdf target"
        VERBATIM
    )
endif()

set(FPC_CANDIDATES)
if(WIN32)
    list(APPEND FPC_CANDIDATES ppcrossx64 ppcx64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND FPC_CANDIDATES ppcx64)
else()
    list(APPEND FPC_CANDIDATES ppc386 fpc)
endif()

unset(FPC_COMPILER CACHE)
find_program(FPC_COMPILER NAMES ${FPC_CANDIDATES})

if(FPC_COMPILER)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bgidemo_pascal)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/keyboard_queue_pascal)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/bgidemo_pascal/bgidemo${CMAKE_EXECUTABLE_SUFFIX}
        COMMAND ${FPC_COMPILER}
                -Fu${CMAKE_SOURCE_DIR}/examples/bgidemo-pascal
                -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                -FE${CMAKE_BINARY_DIR}/bgidemo_pascal
                -FU${CMAKE_BINARY_DIR}/bgidemo_pascal
                ${CMAKE_SOURCE_DIR}/examples/bgidemo-pascal/bgidemo.pas
        DEPENDS
            wx_bgi_opengl
            ${CMAKE_SOURCE_DIR}/examples/bgidemo-pascal/bgidemo.pas
            ${CMAKE_SOURCE_DIR}/examples/bgidemo-pascal/Graph.pas
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM
    )

    add_custom_target(
        bgidemo_pascal_build
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                ${CMAKE_BINARY_DIR}/bgidemo_pascal
        DEPENDS
            wx_bgi_opengl
            ${CMAKE_BINARY_DIR}/bgidemo_pascal/bgidemo${CMAKE_EXECUTABLE_SUFFIX}
    )

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/keyboard_queue_pascal/demo_wxbgi_keyboard_queue${CMAKE_EXECUTABLE_SUFFIX}
        COMMAND ${FPC_COMPILER}
                -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                -FE${CMAKE_BINARY_DIR}/keyboard_queue_pascal
                -FU${CMAKE_BINARY_DIR}/keyboard_queue_pascal
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_wxbgi_keyboard_queue.pas
        DEPENDS
            wx_bgi_opengl
            ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_wxbgi_keyboard_queue.pas
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM
    )

    add_custom_target(
        wxbgi_keyboard_queue_pascal_build
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                ${CMAKE_BINARY_DIR}/keyboard_queue_pascal
        DEPENDS
            wx_bgi_opengl
            ${CMAKE_BINARY_DIR}/keyboard_queue_pascal/demo_wxbgi_keyboard_queue${CMAKE_EXECUTABLE_SUFFIX}
    )
endif()

if(BUILD_TESTING)
    add_test(NAME bgi_api_coverage_cpp COMMAND $<TARGET_FILE:bgi_api_coverage_cpp>)
    set_tests_properties(bgi_api_coverage_cpp PROPERTIES TIMEOUT 20)

    add_test(NAME wxbgi_camera_demo_cpp COMMAND $<TARGET_FILE:wxbgi_camera_demo_cpp> --test)
    set_tests_properties(wxbgi_camera_demo_cpp PROPERTIES TIMEOUT 20)

    add_test(NAME test_dds_serialize     COMMAND $<TARGET_FILE:test_dds_serialize>)
    set_tests_properties(test_dds_serialize     PROPERTIES TIMEOUT 20)

    add_test(NAME test_dds_deserialize   COMMAND $<TARGET_FILE:test_dds_deserialize>)
    set_tests_properties(test_dds_deserialize   PROPERTIES TIMEOUT 20)

    add_test(NAME test_dds_clear         COMMAND $<TARGET_FILE:test_dds_clear>)
    set_tests_properties(test_dds_clear         PROPERTIES TIMEOUT 20)

    add_test(NAME test_dds_cam2d_yz      COMMAND $<TARGET_FILE:test_dds_cam2d_yz>)
    set_tests_properties(test_dds_cam2d_yz      PROPERTIES TIMEOUT 20)

    add_test(NAME test_dds_cam3d_persp   COMMAND $<TARGET_FILE:test_dds_cam3d_persp>)
    set_tests_properties(test_dds_cam3d_persp   PROPERTIES TIMEOUT 20)

    add_test(NAME test_solids            COMMAND $<TARGET_FILE:test_solids>)
    set_tests_properties(test_solids            PROPERTIES TIMEOUT 30)

    add_test(NAME test_input_hooks       COMMAND $<TARGET_FILE:test_input_hooks>)
    set_tests_properties(test_input_hooks       PROPERTIES TIMEOUT 20)

    add_test(NAME test_input_bypass      COMMAND $<TARGET_FILE:test_input_bypass>)
    set_tests_properties(test_input_bypass      PROPERTIES TIMEOUT 20)

    if(Python3_Interpreter_FOUND)
        add_test(
            NAME bgi_api_coverage_python
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/examples/python/bgi_api_coverage.py
                    $<TARGET_FILE:wx_bgi_opengl>
        )
        set_tests_properties(bgi_api_coverage_python PROPERTIES TIMEOUT 20)
    endif()

    if(FPC_COMPILER)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_coverage)

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_coverage/demo_bgi_api_coverage${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_coverage
                    -FU${CMAKE_BINARY_DIR}/pascal_coverage
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_api_coverage.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_api_coverage.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            bgi_api_coverage_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_coverage
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_coverage/demo_bgi_api_coverage${CMAKE_EXECUTABLE_SUFFIX}
        )

        add_test(
            NAME bgi_api_coverage_pascal_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target bgi_api_coverage_pascal_build
        )
        set_tests_properties(bgi_api_coverage_pascal_build PROPERTIES TIMEOUT 120)

        if(UNIX AND NOT APPLE)
            add_test(
                NAME bgi_api_coverage_pascal_run
                COMMAND ${CMAKE_COMMAND} -E env
                        "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/pascal_coverage:$<TARGET_FILE_DIR:wx_bgi_opengl>:$ENV{LD_LIBRARY_PATH}"
                        ${CMAKE_BINARY_DIR}/pascal_coverage/demo_bgi_api_coverage${CMAKE_EXECUTABLE_SUFFIX}
            )
        else()
            add_test(
                NAME bgi_api_coverage_pascal_run
                COMMAND ${CMAKE_BINARY_DIR}/pascal_coverage/demo_bgi_api_coverage${CMAKE_EXECUTABLE_SUFFIX}
            )
        endif()
        set_tests_properties(
            bgi_api_coverage_pascal_run
            PROPERTIES
            DEPENDS bgi_api_coverage_pascal_build
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_coverage
            TIMEOUT 20
        )

        # -------------------------------------------------------------------
        # wx-canvas BGI coverage Pascal test (no GLFW — wx-only path)
        # -------------------------------------------------------------------
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_canvas_coverage)

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_canvas_coverage/demo_bgi_canvas_coverage${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_canvas_coverage
                    -FU${CMAKE_BINARY_DIR}/pascal_canvas_coverage
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_canvas_coverage.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_canvas_coverage.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            bgi_canvas_coverage_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_canvas_coverage
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_canvas_coverage/demo_bgi_canvas_coverage${CMAKE_EXECUTABLE_SUFFIX}
        )

        add_test(
            NAME bgi_canvas_coverage_pascal_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target bgi_canvas_coverage_pascal_build
        )
        set_tests_properties(bgi_canvas_coverage_pascal_build PROPERTIES TIMEOUT 120)

        if(UNIX AND NOT APPLE)
            add_test(
                NAME bgi_canvas_coverage_pascal_run
                COMMAND ${CMAKE_COMMAND} -E env
                        "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/pascal_canvas_coverage:$<TARGET_FILE_DIR:wx_bgi_opengl>:$ENV{LD_LIBRARY_PATH}"
                        ${CMAKE_BINARY_DIR}/pascal_canvas_coverage/demo_bgi_canvas_coverage${CMAKE_EXECUTABLE_SUFFIX}
            )
        else()
            add_test(
                NAME bgi_canvas_coverage_pascal_run
                COMMAND ${CMAKE_BINARY_DIR}/pascal_canvas_coverage/demo_bgi_canvas_coverage${CMAKE_EXECUTABLE_SUFFIX}
            )
        endif()
        set_tests_properties(
            bgi_canvas_coverage_pascal_run
            PROPERTIES
            DEPENDS bgi_canvas_coverage_pascal_build
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_canvas_coverage
            TIMEOUT 20
        )

        # -------------------------------------------------------------------
        # Input hook Pascal test
        # -------------------------------------------------------------------
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_input_hooks)

        # When WXBGI_ENABLE_TEST_SEAMS is ON, pass the matching define to FPC
        # so {$IFDEF WXBGI_ENABLE_TEST_SEAMS} blocks are compiled in.
        set(WXBGI_FPC_SEAMS_FLAG "")
        if(WXBGI_ENABLE_TEST_SEAMS)
            set(WXBGI_FPC_SEAMS_FLAG "-dWXBGI_ENABLE_TEST_SEAMS")
        endif()

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_hooks${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    ${WXBGI_FPC_SEAMS_FLAG}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_input_hooks
                    -FU${CMAKE_BINARY_DIR}/pascal_input_hooks
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/test_input_hooks.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/test_input_hooks.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            test_input_hooks_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_input_hooks
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_hooks${CMAKE_EXECUTABLE_SUFFIX}
        )

        add_test(
            NAME test_input_hooks_pascal_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG>
                    --target test_input_hooks_pascal_build
        )
        set_tests_properties(test_input_hooks_pascal_build PROPERTIES TIMEOUT 120)

        if(UNIX AND NOT APPLE)
            add_test(
                NAME test_input_hooks_pascal_run
                COMMAND ${CMAKE_COMMAND} -E env
                        "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/pascal_input_hooks:$<TARGET_FILE_DIR:wx_bgi_opengl>:$ENV{LD_LIBRARY_PATH}"
                        ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_hooks${CMAKE_EXECUTABLE_SUFFIX}
            )
        else()
            add_test(
                NAME test_input_hooks_pascal_run
                COMMAND ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_hooks${CMAKE_EXECUTABLE_SUFFIX}
            )
        endif()
        set_tests_properties(
            test_input_hooks_pascal_run
            PROPERTIES
            DEPENDS test_input_hooks_pascal_build
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_input_hooks
            TIMEOUT 20
        )

        # -------------------------------------------------------------------
        # Input hook Pascal interactive demo (build target only, not a test)
        # -------------------------------------------------------------------
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_input_hooks/demo_input_hooks${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_input_hooks
                    -FU${CMAKE_BINARY_DIR}/pascal_input_hooks
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_input_hooks.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_input_hooks.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            demo_input_hooks_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_input_hooks
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_input_hooks/demo_input_hooks${CMAKE_EXECUTABLE_SUFFIX}
        )

        # -------------------------------------------------------------------
        # Bypass/scroll/hk test — Pascal (build + run)
        # -------------------------------------------------------------------
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_bypass${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    ${WXBGI_FPC_SEAMS_FLAG}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_input_hooks
                    -FU${CMAKE_BINARY_DIR}/pascal_input_hooks
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/test_input_bypass.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/test_input_bypass.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            test_input_bypass_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_input_hooks
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_bypass${CMAKE_EXECUTABLE_SUFFIX}
        )

        add_test(
            NAME test_input_bypass_pascal_build
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG>
                    --target test_input_bypass_pascal_build
        )
        set_tests_properties(test_input_bypass_pascal_build PROPERTIES TIMEOUT 120)

        if(UNIX AND NOT APPLE)
            add_test(
                NAME test_input_bypass_pascal_run
                COMMAND ${CMAKE_COMMAND} -E env
                        "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/pascal_input_hooks:$<TARGET_FILE_DIR:wx_bgi_opengl>:$ENV{LD_LIBRARY_PATH}"
                        ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_bypass${CMAKE_EXECUTABLE_SUFFIX}
            )
        else()
            add_test(
                NAME test_input_bypass_pascal_run
                COMMAND ${CMAKE_BINARY_DIR}/pascal_input_hooks/test_input_bypass${CMAKE_EXECUTABLE_SUFFIX}
            )
        endif()
        set_tests_properties(
            test_input_bypass_pascal_run
            PROPERTIES
            DEPENDS test_input_bypass_pascal_build
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_input_hooks
            TIMEOUT 20
        )

        # -------------------------------------------------------------------
        # demo_bgi_wrapper — GLFW-mode BGI wrapper demo (build target only)
        # -------------------------------------------------------------------
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/pascal_demos)

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_demos/demo_bgi_wrapper${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_demos
                    -FU${CMAKE_BINARY_DIR}/pascal_demos
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_wrapper.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_wrapper.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            demo_bgi_wrapper_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_demos
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_demos/demo_bgi_wrapper${CMAKE_EXECUTABLE_SUFFIX}
        )

        # -------------------------------------------------------------------
        # demo_bgi_wrapper_gui — wx-mode BGI wrapper GUI demo (build target only)
        # -------------------------------------------------------------------
        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/pascal_demos/demo_bgi_wrapper_gui${CMAKE_EXECUTABLE_SUFFIX}
            COMMAND ${FPC_COMPILER}
                    -Fu${CMAKE_SOURCE_DIR}/examples/demoFreePascal
                    -Fl$<TARGET_FILE_DIR:wx_bgi_opengl>
                    -FE${CMAKE_BINARY_DIR}/pascal_demos
                    -FU${CMAKE_BINARY_DIR}/pascal_demos
                    ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_wrapper_gui.pas
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_SOURCE_DIR}/examples/demoFreePascal/demo_bgi_wrapper_gui.pas
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )

        add_custom_target(
            demo_bgi_wrapper_gui_pascal_build
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:wx_bgi_opengl>
                    ${CMAKE_BINARY_DIR}/pascal_demos
            DEPENDS
                wx_bgi_opengl
                ${CMAKE_BINARY_DIR}/pascal_demos/demo_bgi_wrapper_gui${CMAKE_EXECUTABLE_SUFFIX}
        )

    endif()
endif()

# ---------------------------------------------------------------------------
# wxWidgets static integration library (wx_bgi_wx) — for C++ apps that embed
# WxBgiCanvas inside their own wxFrame.  The standalone C API
# (wxbgi_wx_app_create / wxbgi_wx_frame_create / wxbgi_wx_app_main_loop) is
# already compiled into wx_bgi_opengl.dll when WXBGI_ENABLE_WX=ON.
# ---------------------------------------------------------------------------
if(WXBGI_ENABLE_WX)
    # wx_bgi_wx — STATIC lib that compiles WxBgiCanvas into the APPLICATION,
    # not the DLL.  This is critical: the canvas must use the EXE's copy of
    # wxWidgets so that wxTheApp, wxGLContext, etc. resolve correctly.
    # It links wx_bgi_opengl for the exported C API (wxbgi_wx_*).
    add_library(wx_bgi_wx STATIC src/wx/wx_bgi_canvas.cpp)
    target_include_directories(wx_bgi_wx
        PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src
               ${glew_SOURCE_DIR}/include)
    target_compile_definitions(wx_bgi_wx PUBLIC WXBGI_ENABLE_WX GLEW_STATIC)
    target_link_libraries(wx_bgi_wx PUBLIC wx_bgi_opengl GLEW::GLEW wxcore wxgl wxbase)
    # Match the MSVC runtime of the core DLL (/MT or /MTd).
    set_target_properties(wx_bgi_wx PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Automated headless solids test — CONSOLE exe, no wx event loop required.
    # Tests that the BGI API works correctly in wx-embedded mode (initForWxCanvas +
    # solid primitives + camera setup) without needing a display.
    add_executable(wx_bgi_solids_test examples/wx/wx_bgi_solids_test.cpp)
    target_link_libraries(wx_bgi_solids_test PRIVATE wx_bgi_wx)
    add_custom_command(
        TARGET wx_bgi_solids_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                $<TARGET_FILE_DIR:wx_bgi_solids_test>
    )

    if(BUILD_TESTING)
        add_test(NAME wx_bgi_solids_test COMMAND $<TARGET_FILE:wx_bgi_solids_test>)
        set_tests_properties(wx_bgi_solids_test PROPERTIES TIMEOUT 15)
    endif()

    # Animated 3-D orbit test — WIN32 exe so no console window pops up.
    # Renders 135 frames of a 360° camera orbit, then exits 0.
    if(WIN32)
        add_executable(wx_bgi_3d_orbit_test WIN32
            examples/wx/wx_bgi_3d_orbit_test.cpp
            main.exe.manifest
            resources.rc
        )
    elseif(APPLE)
        add_executable(wx_bgi_3d_orbit_test MACOSX_BUNDLE
            examples/wx/wx_bgi_3d_orbit_test.cpp)
        set_target_properties(wx_bgi_3d_orbit_test PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist)
    else()
        add_executable(wx_bgi_3d_orbit_test examples/wx/wx_bgi_3d_orbit_test.cpp)
    endif()
    target_link_libraries(wx_bgi_3d_orbit_test PRIVATE wx_bgi_wx)
    add_custom_command(
        TARGET wx_bgi_3d_orbit_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                $<TARGET_FILE_DIR:wx_bgi_3d_orbit_test>
    )

    if(BUILD_TESTING)
        add_test(NAME wx_bgi_3d_orbit_test
                 COMMAND $<TARGET_FILE:wx_bgi_3d_orbit_test>)
        # 30 s: 4.5 s animation + generous build/launch headroom
        set_tests_properties(wx_bgi_3d_orbit_test PROPERTIES TIMEOUT 60)
    endif()

    # Automated CTest: BGI API coverage using WxBgiCanvas inside a wxFrame (C++ mirror of Pascal demo)
    if(WIN32)
        add_executable(wx_bgi_canvas_coverage_test WIN32
            examples/wx/wx_bgi_canvas_coverage_test.cpp
            main.exe.manifest
            resources.rc
        )
    elseif(APPLE)
        add_executable(wx_bgi_canvas_coverage_test MACOSX_BUNDLE
            examples/wx/wx_bgi_canvas_coverage_test.cpp)
        set_target_properties(wx_bgi_canvas_coverage_test PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist)
    else()
        add_executable(wx_bgi_canvas_coverage_test
            examples/wx/wx_bgi_canvas_coverage_test.cpp)
    endif()
    target_link_libraries(wx_bgi_canvas_coverage_test PRIVATE wx_bgi_wx)
    add_custom_command(
        TARGET wx_bgi_canvas_coverage_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                $<TARGET_FILE_DIR:wx_bgi_canvas_coverage_test>
    )
    if(BUILD_TESTING)
        add_test(NAME wx_bgi_canvas_coverage_test
                 COMMAND $<TARGET_FILE:wx_bgi_canvas_coverage_test>)
        # 3 s colorful + 3 s final message + generous build/launch headroom
        set_tests_properties(wx_bgi_canvas_coverage_test PROPERTIES TIMEOUT 30)
    endif()

    # Interactive demo (WIN32/MACOSX_BUNDLE — not a CTest target)
    if(WIN32)
        add_executable(wx_bgi_app WIN32
            examples/wx/wx_bgi_app.cpp
            main.exe.manifest
            resources.rc
        )
    elseif(APPLE)
        add_executable(wx_bgi_app MACOSX_BUNDLE examples/wx/wx_bgi_app.cpp)
        set_target_properties(wx_bgi_app PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist)
    else()
        add_executable(wx_bgi_app examples/wx/wx_bgi_app.cpp)
    endif()
    target_link_libraries(wx_bgi_app PRIVATE wx_bgi_wx)
    add_custom_command(
        TARGET wx_bgi_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                $<TARGET_FILE_DIR:wx_bgi_app>
    )

    # 3-D interactive demo — orbit camera, solid primitives, mouse/keyboard
    if(WIN32)
        add_executable(wx_bgi_3d_app WIN32
            examples/wx/wx_bgi_3d_app.cpp
            main.exe.manifest
            resources.rc
        )
    elseif(APPLE)
        add_executable(wx_bgi_3d_app MACOSX_BUNDLE examples/wx/wx_bgi_3d_app.cpp)
        set_target_properties(wx_bgi_3d_app PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist)
    else()
        add_executable(wx_bgi_3d_app examples/wx/wx_bgi_3d_app.cpp)
    endif()
    target_link_libraries(wx_bgi_3d_app PRIVATE wx_bgi_wx)
    add_custom_command(
        TARGET wx_bgi_3d_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:wx_bgi_opengl>
                $<TARGET_FILE_DIR:wx_bgi_3d_app>
    )
endif()