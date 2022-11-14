cmake_minimum_required(VERSION 3.1.0)

project(OpenSMO++)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Source)

# set the main startup program.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
