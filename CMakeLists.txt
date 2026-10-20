cmake_minimum_required(VERSION 3.21)
project(Intel_RTL_Analyzer_Next LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_ANALYZER "Build Qt/IPP/RTL-SDR application" ON)
option(BUILD_QT_CHECK "Compile GUI with declaration-only IPP shim (CI only, no runnable app)" OFF)
include(CTest)
if(BUILD_TESTING)
    foreach(name cursorreadout frequencyzoom iqblockassembler spectrumdisplay spectrumhistory waterfallraster measurementmath)
        add_executable(${name}_test tests/${name}_test.cpp)
        # Existing tests use assert; keep assertions enabled in Release too.
        if(MSVC)
            target_compile_options(${name}_test PRIVATE /UNDEBUG /utf-8)
        else()
            target_compile_options(${name}_test PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${name} COMMAND ${name}_test)
    endforeach()
endif()
if(BUILD_QT_CHECK)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
    set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Intel_RTL_analyzer")
    add_library(analyzer_compile_check OBJECT
        ${SOURCE_DIR}/main.cpp ${SOURCE_DIR}/Intel_RTL_analyzer.cpp
        ${SOURCE_DIR}/Intel_RTL_analyzer.h ${SOURCE_DIR}/Intel_RTL_analyzer.ui
        ${SOURCE_DIR}/WaterfallWindow.cpp ${SOURCE_DIR}/WaterfallWindow.h
        ${SOURCE_DIR}/sdrworker.cpp ${SOURCE_DIR}/sdrworker.h
        ${SOURCE_DIR}/displaystate.h ${SOURCE_DIR}/qcustomplot.cpp ${SOURCE_DIR}/qcustomplot.h)
    target_include_directories(analyzer_compile_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/ipp_stub" "${SOURCE_DIR}")
    target_compile_definitions(analyzer_compile_check PRIVATE QT_NO_PRINTER NOMINMAX _USE_MATH_DEFINES)
    target_link_libraries(analyzer_compile_check PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    return()
endif()
if(NOT BUILD_ANALYZER)
    return()
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
set(IPP_ROOT "$ENV{IPPROOT}" CACHE PATH "Intel IPP root: contains include[/ipp] and lib")
find_path(IPP_INCLUDE_DIR ipps.h HINTS "${IPP_ROOT}" PATH_SUFFIXES include/ipp include REQUIRED)
find_library(IPP_SIGNAL_LIBRARY NAMES ipps HINTS "${IPP_ROOT}" PATH_SUFFIXES lib lib/intel64 REQUIRED)
find_library(IPP_CORE_LIBRARY NAMES ippcore HINTS "${IPP_ROOT}" PATH_SUFFIXES lib lib/intel64 REQUIRED)
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Intel_RTL_analyzer")
if(WIN32)
    find_library(RTLSDR_LIBRARY NAMES rtlsdr HINTS "${SOURCE_DIR}/lib" REQUIRED)
else()
    find_library(RTLSDR_LIBRARY NAMES rtlsdr REQUIRED)
endif()
qt_add_executable(Intel_RTL_Analyzer_Next WIN32
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/Intel_RTL_analyzer.cpp ${SOURCE_DIR}/Intel_RTL_analyzer.h
    ${SOURCE_DIR}/Intel_RTL_analyzer.ui ${SOURCE_DIR}/Intel_RTL_analyzer.qrc
    ${SOURCE_DIR}/WaterfallWindow.cpp ${SOURCE_DIR}/WaterfallWindow.h
    ${SOURCE_DIR}/sdrworker.cpp ${SOURCE_DIR}/sdrworker.h
    ${SOURCE_DIR}/displaystate.h ${SOURCE_DIR}/qcustomplot.cpp ${SOURCE_DIR}/qcustomplot.h)
target_compile_definitions(Intel_RTL_Analyzer_Next PRIVATE QT_NO_PRINTER NOMINMAX _USE_MATH_DEFINES)
target_include_directories(Intel_RTL_Analyzer_Next PRIVATE "${IPP_INCLUDE_DIR}" "${SOURCE_DIR}")
target_link_libraries(Intel_RTL_Analyzer_Next PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
    "${IPP_SIGNAL_LIBRARY}" "${IPP_CORE_LIBRARY}" "${RTLSDR_LIBRARY}")
if(MSVC)
    target_compile_options(Intel_RTL_Analyzer_Next PRIVATE /utf-8)
endif()
if(WIN32)
    file(GLOB RTL_RUNTIME "${SOURCE_DIR}/*.dll")
    foreach(dll IN LISTS RTL_RUNTIME)
        add_custom_command(TARGET Intel_RTL_Analyzer_Next POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:Intel_RTL_Analyzer_Next>")
    endforeach()
    # CMake-opened Visual Studio projects do not require Qt VS Tools.
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT)
        add_custom_command(TARGET Intel_RTL_Analyzer_Next POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations "$<TARGET_FILE:Intel_RTL_Analyzer_Next>")
    endif()
endif()
