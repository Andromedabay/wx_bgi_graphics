cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(wx_bgigraphics_wrapper LANGUAGES C CXX)

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

# Build a static GLEW library from fetched sources to keep the build self-contained.
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
)

add_library(wx_bgi_opengl SHARED ${SRCS})

target_include_directories(wx_bgi_opengl PUBLIC src)
target_compile_definitions(wx_bgi_opengl PRIVATE GLEW_STATIC)
target_link_libraries(wx_bgi_opengl PRIVATE GLEW::GLEW glfw OpenGL::GL glm::glm nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp)

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
target_link_libraries(wxbgi_keyboard_queue_cpp PRIVATE wx_bgi_opengl glfw)

add_custom_command(
    TARGET wxbgi_keyboard_queue_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wx_bgi_opengl>
            $<TARGET_FILE_DIR:wxbgi_keyboard_queue_cpp>
)

add_executable(wxbgi_camera_demo_cpp examples/cpp/wxbgi_camera_demo.cpp)
target_link_libraries(wxbgi_camera_demo_cpp PRIVATE wx_bgi_opengl glfw)

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
        set_tests_properties(bgi_api_coverage_pascal_build PROPERTIES TIMEOUT 20)

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
    endif()
endif()
