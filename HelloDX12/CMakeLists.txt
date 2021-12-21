cmake_minimum_required (VERSION 3.18)
project (learndx12 LANGUAGES CXX)

# Enable multithreaded builds
if( MSVC )
    add_compile_options(/MP)
endif()

set(SOURCES
    # src/helper.cpp
    src/main.cpp
)

if (MSVC_VERSION GREATER_EQUAL "1914")
    add_compile_options("/Zc:__cplusplus") 
endif()
 
if (MSVC_VERSION GREATER_EQUAL "1900")
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("/std:c++17" _compiler_supports_cxx17)
    if (_compiler_supports_cxx17)
        add_compile_options("/std:c++17")
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../target/)


add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
)
# DX12 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d12.lib dxgi.lib dxguid.lib
)