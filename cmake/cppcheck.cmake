# additional target to perform cppcheck run, requires cppcheck
find_package(cppcheck)

if(!CPPCHECK_FOUND)
  message("cppcheck not found. Please install it to run 'make cppcheck'")
endif()

add_custom_target(
        cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} --xml --xml-version=2 --enable=warning,performance,style --error-exitcode=1 --suppressions-list=${CMAKE_SOURCE_DIR}/test/cppcheck/suppressions-list.txt -UNN_EXPORT ${CMAKE_SOURCE_DIR}/src/ ${CMAKE_SOURCE_DIR}/include/ -i${CMAKE_SOURCE_DIR}/src/external -itest 2> cppcheck-report.xml || (cat cppcheck-report.xml && exit 2) 
)


