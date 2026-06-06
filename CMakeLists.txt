cmake_minimum_required(VERSION 3.16)
project(ProcessamentoGraficoTarefa4 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)

add_executable(tarefa_presencial6
    src/tarefa_presencial6.cpp
    common/glad.c
)

target_include_directories(tarefa_presencial6 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/glfw/include
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tarefa_presencial6 PRIVATE -static-libgcc -static-libstdc++)
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(CXX_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    if(EXISTS "${CXX_COMPILER_DIR}/libwinpthread-1.dll")
        add_custom_command(TARGET tarefa_presencial6 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CXX_COMPILER_DIR}/libwinpthread-1.dll"
                "$<TARGET_FILE_DIR:tarefa_presencial6>/libwinpthread-1.dll"
        )
    endif()
endif()

if(WIN32)
    target_link_libraries(tarefa_presencial6 PRIVATE glfw opengl32)
elseif(APPLE)
    find_library(OpenGL_LIBRARY OpenGL)
    target_link_libraries(tarefa_presencial6 PRIVATE glfw ${OpenGL_LIBRARY})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(tarefa_presencial6 PRIVATE glfw ${OPENGL_gl_LIBRARY})
endif()

set_target_properties(tarefa_presencial6 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}
)
