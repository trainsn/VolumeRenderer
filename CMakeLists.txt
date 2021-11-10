cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(NyxVR)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add .lib files
link_directories(
    "${CMAKE_SOURCE_DIR}/lib"
)
	
# Add header files
file(GLOB_RECURSE HEADER_FILES 
	${CMAKE_SOURCE_DIR}/src/*.h
	${CMAKE_SOURCE_DIR}/src/*.hpp)
	
# We need a CMAKE_DIR with some code to find external dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

#######################################
# LOOK for the packages that we need! #
#######################################

# OpenGL EGL
find_package(OpenGL REQUIRED COMPONENTS EGL)

# GLM
find_package(GLM REQUIRED)
message(STATUS "GLM included at ${GLM_INCLUDE_DIR}")

# GLAD
add_library(glad OBJECT third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include/)
target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})

add_executable(eglinfo src/eglinfo.c)
target_link_libraries(
    eglinfo
    PRIVATE
    OpenGL::EGL
    glad
)

# Define the executable
add_executable(${PROJECT_NAME} ${HEADER_FILES} src/main.cpp)

# Define the include DIRs
include_directories(
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/include"
)

# Put all libraries into a variable
set(LIBS OpenGL GL X11 pthread Xrandr Xi dl glad OpenGL::EGL)

# Define the link libraries
target_link_libraries(${PROJECT_NAME} ${LIBS})
