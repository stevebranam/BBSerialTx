cmake_minimum_required(VERSION 3.13)
project(BBSerialTx)

# Provide symbols to run with GDB
set(CMAKE_BUILD_TYPE Debug)

# Set compiler flags for gcov
SET(CMAKE_CXX_FLAGS "-g -O0 --coverage -fprofile-arcs -ftest-coverage")
SET(CMAKE_C_FLAGS "-g -O0 --coverage -fprofile-arcs -ftest-coverage")

# Replace object name extension so gcov can find files
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE ON)
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE ON)

add_library(BBSerialTx)
add_executable(testBBSerialTx "")

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_subdirectory(source)
add_subdirectory(test)
target_include_directories(testBBSerialTx PRIVATE source)
target_link_libraries(testBBSerialTx BBSerialTx gtest gmock pthread)

# Locate Doxygen. Require dot, treat the other components as optional
find_package(Doxygen
             REQUIRED dot
             OPTIONAL_COMPONENTS mscgen dia)

# Customize Doxygen configuration.
set(DOXYGEN_EXTRACT_ALL       YES)
set(DOXYGEN_EXTRACT_PRIVATE   YES)
set(DOXYGEN_EXTRACT_STATIC    YES)
set(DOXYGEN_EXCLUDE           LICENSE.txt)
set(DOXYGEN_CALL_GRAPH        YES)
set(DOXYGEN_CALLER_GRAPH      YES)
set(DOXYGEN_GENERATE_LATEX    NO)
set(DOXYGEN_TAB_SIZE          8)
set(DOXYGEN_OUTPUT_DIRECTORY  ../documents/)
set(DOXYGEN_PLANTUML_JAR_PATH /usr/share/plantuml/plantuml.jar)

# Configure Doxygen.
doxygen_add_docs(
    doxygen
    mainpage.md
    source
)
