cmake_minimum_required(VERSION 3.0)

project(clc)

set(VERSION_STRING "1.0.1")

if (${CMAKE_GENERATOR} MATCHES "Unix Makefiles")
  set(UNIX_MAKEFILE_GENERATOR 1)
elseif(${CMAKE_GENERATOR} MATCHES "Visual Studio")
  set(MSVC_GENERATOR 1)
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(TARGET_MODIFIER "32")
  set(TARGET_MODIFIER_PATH "x86")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_MODIFIER "64")
  set(TARGET_MODIFIER_PATH "x64")
else()
  message(FATAL_ERROR "unexpected platform")
endif()

find_package(OpenCL REQUIRED)
# message("OpenCL_INCLUDE_DIR   ${OpenCL_INCLUDE_DIR}")
# message("OpenCL_LIBRARIES     ${OpenCL_LIBRARIES}")

include_directories(${OpenCL_INCLUDE_DIR})
add_executable(${PROJECT_NAME} src/clc.cpp)
add_definitions(-DVERSION_STRING="${VERSION_STRING}")

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME                                "clc${TARGET_MODIFIER}"
  )
target_link_libraries(${PROJECT_NAME} ${OpenCL_LIBRARIES})
