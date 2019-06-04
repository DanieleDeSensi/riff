########################
# Src and header files #
########################
include_directories(${PROJECT_SOURCE_DIR}/include)
file(GLOB SOURCES "*.cpp")

include(FindOpenMP)

add_compile_options(${OpenMP_CXX_FLAGS})

if(${CMAKE_VERSION} VERSION_LESS "3.9") 
    set(OpenMP_CXX_LIBRARIES "-fopenmp")
endif()

if(OPENMP_FOUND)
  file(GLOB APP_SOURCES *.cpp )
  find_library(ANL_LIBRARY NAMES anl)
  foreach( testsourcefile ${APP_SOURCES} )
      get_filename_component(testname ${testsourcefile} NAME_WE)
      add_executable( ${testname} ${testsourcefile} )
      target_link_libraries( ${testname} riff ${OpenMP_CXX_LIBRARIES} ${ANL_LIBRARY})
  endforeach( testsourcefile ${APP_SOURCES} )
endif(OPENMP_FOUND)