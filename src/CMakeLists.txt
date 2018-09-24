########################
# Src and header files #
########################
include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB SOURCES "*.cpp" "*.c")


####################
# Library building #
####################
if(ENABLE_SHARED)
  option(NN_STATIC_LIB "" OFF)
  add_subdirectory(${CMAKE_SOURCE_DIR}/src/external/nanomsg)
  add_library(riff SHARED ${SOURCES})
  target_link_libraries(riff nanomsg)
  install(TARGETS riff DESTINATION /usr/lib)
else()
  option(NN_STATIC_LIB "" ON)
  add_subdirectory(${CMAKE_SOURCE_DIR}/src/external/nanomsg)
  add_library(riff_static STATIC ${SOURCES})
  add_dependencies(riff_static nanomsg)
  install(TARGETS riff_static DESTINATION /usr/lib)
  add_custom_command(TARGET riff_static
                   POST_BUILD
                   COMMAND ${CMAKE_AR} -M  < ${CMAKE_SOURCE_DIR}/src/buildlib.ar
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif(ENABLE_SHARED)


####################
# Uninstall target #
####################
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake)
endif()
