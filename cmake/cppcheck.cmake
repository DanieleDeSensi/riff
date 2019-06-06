# additional target to perform cppcheck run, requires cppcheck
find_package(cppcheck)

if(!CPPCHECK_FOUND)
  message("cppcheck not found. Please install it to run 'make cppcheck'")
endif()

add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} --xml --xml-version=2 --enable=warning,performance,style --error-exitcode=1 --suppressions-list=${PROJECT_SOURCE_DIR}/test/cppcheck/suppressions-list.txt -UNN_EXPORT ${PROJECT_SOURCE_DIR}/src/ ${PROJECT_SOURCE_DIR}/include/ -i${PROJECT_BINARY_DIR} -i${PROJECT_SOURCE_DIR}/src/ -i${PROJECT_SOURCE_DIR}/demo/ -i${PROJECT_SOURCE_DIR}/cmake/ external -itest 2> cppcheck-report.xml || (cat cppcheck-report.xml && exit 2) 
)


