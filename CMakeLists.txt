cmake_minimum_required(VERSION 4.1)
project(file_recovery)

set(CMAKE_CXX_STANDARD 20)

add_executable(file_recovery main.cpp carver.cpp fat_undelete.cpp)

# Optional Qt 6 GUI (runs the CLI binary via QProcess)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    add_executable(file_recovery_gui
        gui/main.cpp
        gui/mainwindow.cpp
        gui/mainwindow.h
        gui/app_style.cpp
        gui/app_style.hpp
    )
    target_link_libraries(file_recovery_gui PRIVATE Qt6::Widgets)
    target_include_directories(file_recovery_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gui)
    if(APPLE)
        set_target_properties(file_recovery_gui PROPERTIES MACOSX_BUNDLE TRUE)
    endif()
    add_custom_command(TARGET file_recovery_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:file_recovery>
            $<TARGET_FILE_DIR:file_recovery_gui>
        COMMENT "Copy file_recovery next to GUI executable")
endif()
