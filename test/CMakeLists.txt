########################
# Src and header files #
########################
include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB SOURCES "*.cpp")

include(FindOpenMP)

add_compile_options(${OpenMP_CXX_FLAGS})

if(OPENMP_FOUND)
  file(GLOB APP_SOURCES *.cpp )
  find_library(ANL_LIBRARY NAMES anl)
  foreach( testsourcefile ${APP_SOURCES} )
      get_filename_component(testname ${testsourcefile} NAME_WE)
      add_executable( ${testname} ${testsourcefile} )
      if(ENABLE_SHARED)
        target_link_libraries( ${testname} riff ${OpenMP_CXX_LIBRARIES} ${ANL_LIBRARY})
      else()
        target_link_libraries( ${testname} riff_static ${OpenMP_CXX_LIBRARIES} ${ANL_LIBRARY})
      endif()
  endforeach( testsourcefile ${APP_SOURCES} )
endif(OPENMP_FOUND)