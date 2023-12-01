
# CMake minimum version
cmake_minimum_required(VERSION 3.5)

# Set policy
cmake_policy(SET CMP0079 NEW)

# project name and languages
project(PicoDataEdit C CXX RC)

# Read RATL if any
add_subdirectory(RATL)
get_directory_property(RATL_DEFINITIONS DIRECTORY RATL DEFINITION RATL_DEFINITIONS)
get_directory_property(RATL_INCLUDE_DIRS DIRECTORY RATL DEFINITION RATL_INCLUDE_DIRS)
get_directory_property(RATL_LIBRARIES DIRECTORY RATL DEFINITION RATL_LIBRARIES)

# PicoDataEdit.exe
add_executable(PicoDataEdit WIN32 main.cpp main_res.rc)
target_compile_definitions(PicoDataEdit PRIVATE ${RATL_DEFINITIONS})
target_include_directories(PicoDataEdit PRIVATE ${RATL_INCLUDE_DIRS})
target_link_libraries(PicoDataEdit PRIVATE ${RATL_LIBRARIES} comctl32 comdlg32 shlwapi)
if(MSVC)
    target_link_options(PicoDataEdit PRIVATE /MANIFEST:NO)
endif()
