########################
# Src and header files #
########################
include_directories(${PROJECT_SOURCE_DIR}/include)
file(GLOB SOURCES "riff.cpp" "${PROJECT_SOURCE_DIR}/include/riff/archdata.hpp")

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/riff
        DESTINATION include)

####################
# Library building #
####################
find_library(ANL_LIBRARY NAMES anl)

option(NN_STATIC_LIB "" ON)
add_subdirectory(${PROJECT_SOURCE_DIR}/src/external/nanomsg)
add_library(riff SHARED ${SOURCES})
target_link_libraries(riff nanomsg pthread ${ANL_LIBRARY})
install(TARGETS riff DESTINATION lib)

add_library(riff_static STATIC ${SOURCES})
target_link_libraries(riff_static nanomsg pthread ${ANL_LIBRARY})
install(TARGETS riff_static DESTINATION lib)
add_custom_command(TARGET riff_static
                   POST_BUILD
                   COMMAND ${CMAKE_AR} -M  < ${PROJECT_SOURCE_DIR}/src/buildlib.ar
                   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		   )

add_custom_target(archdata
                  COMMAND ${PROJECT_SOURCE_DIR}/src/build_archdata.sh ${PROJECT_SOURCE_DIR}/include/riff/archdata.hpp
                  DEPENDS ticksPerNs
		  )
		   
add_executable(ticksPerNs ticksPerNs.cpp)
add_dependencies(riff archdata)
add_dependencies(riff_static archdata)

####################
# Uninstall target #
####################
if(NOT TARGET uninstall)
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake)
endif()
