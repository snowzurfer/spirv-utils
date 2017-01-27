enable_testing()

# Set headers and sources

set(CATCH_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/catch_main.cpp)

#
# add_catch_test(<target> <sources>...)
#
function(add_catch_test TARGET)
  add_executable(${TARGET} ${CATCH_MAIN_SOURCE} ${ARGN})
  target_include_directories(${TARGET} PUBLIC
    ${CATCH_SOURCE_DIR}/single_include)
    
  add_test(${TARGET} ${TARGET})

  add_custom_command(TARGET ${TARGET}
    POST_BUILD
    COMMAND ${TARGET}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ${TARGET}" VERBATIM)
endfunction()

add_catch_test(test_0 test_0.cpp)
target_include_directories(test_0 PUBLIC
  ${SUT_SOURCE_DIR}/include
  ${SPIRV-HEADERS_SOURCE_DIR}/include)
target_link_libraries(test_0
  sut)
