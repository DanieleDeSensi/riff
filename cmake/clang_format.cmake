## get all project files
find_package(ClangFormat)
   
if(NOT CLANG_FORMAT_FOUND)
  message("clang-format not found. Please install it to run 'make clangformat'")
endif()

file(GLOB SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/include/riff/*.hpp )
 
add_custom_target(
        clangformat
        COMMAND ${CLANG_FORMAT_EXECUTABLE}
        -style=google
        -i
        ${SOURCE_FILES}
)