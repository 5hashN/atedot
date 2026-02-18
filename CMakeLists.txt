cmake_minimum_required(VERSION 3.16)

project(atedot LANGUAGES C)

# standard GNU install paths
include(GNUInstallDirs)

# library target
add_library(atedot_lib STATIC)

# collect sources
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")
list(FILTER LIB_SOURCES EXCLUDE REGEX "src/main.c$")

# add sources to the target private scope
target_sources(atedot_lib PRIVATE ${LIB_SOURCES})

# C11 standard
set_target_properties(atedot_lib PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# define include directories
target_include_directories(atedot_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# compiler warnings (MSVC vs GCC/Clang)
if(MSVC)
    target_compile_options(atedot_lib PRIVATE /W4)
else()
    target_compile_options(atedot_lib PRIVATE -Wall -Wextra -pedantic)
endif()

# link math library
if(NOT MSVC)
    target_link_libraries(atedot_lib PRIVATE m)
endif()

# main executable
add_executable(atedot src/main.c)
target_link_libraries(atedot PRIVATE atedot_lib)

# examples
file(GLOB EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.c")
foreach(EX ${EXAMPLES})
    get_filename_component(EX_NAME ${EX} NAME_WE)
    add_executable(${EX_NAME} ${EX})
    target_link_libraries(${EX_NAME} PRIVATE atedot_lib)
endforeach()


# install artifacts to system locations
install(TARGETS atedot atedot_lib
    EXPORT atedotTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# install headers
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# export target configuration
install(EXPORT atedotTargets
    FILE atedotTargets.cmake
    NAMESPACE atedot::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atedot
)

# cross-platorm command to run
add_custom_target(run
    COMMAND $<TARGET_FILE:atedot>
    DEPENDS atedot
    USES_TERMINAL
)