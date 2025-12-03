cmake_minimum_required(VERSION 3.16)

project(AlarmingClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network Widgets Core QuickControls2)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system asio)

# Include directories
include_directories(base)
include_directories(ClientUI/CTP/include)

# Source files
set(SOURCES
    ClientUI/main.cpp
    ClientUI/Backend.cpp
    ClientUI/Backend.h
    ClientUI/AuthManager.cpp
    ClientUI/AuthManager.h
    ClientUI/WarningManager.cpp
    ClientUI/WarningManager.h
    ClientUI/QuoteClient.cpp
    ClientUI/QuoteClient.h
    base/FuturesClient.cpp
    base/FuturesClient.h
)

# Executable
qt_add_executable(ClientUI WIN32 ${SOURCES})

# Link libraries
target_link_libraries(ClientUI PRIVATE
    Qt6::Quick
    Qt6::Network
    Qt6::Widgets
    Qt6::Core
    Qt6::QuickControls2
    nlohmann_json::nlohmann_json
    Boost::system
    Boost::asio
)

# CTP Libs
find_library(CTP_MD_LIB thostmduserapi_se PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ClientUI/CTP/lib NO_DEFAULT_PATH)
find_library(CTP_TRADER_LIB thosttraderapi_se PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ClientUI/CTP/lib NO_DEFAULT_PATH)
target_link_libraries(ClientUI PRIVATE ${CTP_MD_LIB} ${CTP_TRADER_LIB})

# Copy CTP DLLs to output directory
add_custom_command(TARGET ClientUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/ClientUI/CTP/bin
    $<TARGET_FILE_DIR:ClientUI>
    COMMENT "Copying CTP DLLs..."
)

# Copy QML files to output directory (preserving structure)
# This ensures engine.load("Main.qml") works
file(GLOB_RECURSE QML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ClientUI/*.qml")
foreach(QML_FILE ${QML_FILES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ClientUI" "${QML_FILE}")
    add_custom_command(TARGET ClientUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QML_FILE}"
        "$<TARGET_FILE_DIR:ClientUI>/${REL_PATH}"
    )
endforeach()
