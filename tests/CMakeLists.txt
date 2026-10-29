add_executable(kyra_unit_tests unit_tests.cpp)
target_link_libraries(kyra_unit_tests PRIVATE kyra_core)
kyra_target_options(kyra_unit_tests)
add_test(NAME unit COMMAND kyra_unit_tests ${CMAKE_CURRENT_SOURCE_DIR}/data/aml_eligibility_regkeys.txt
         ${CMAKE_CURRENT_SOURCE_DIR}/data/aml_ra_regkeys.txt)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME e2e COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py
           $<TARGET_FILE:kyra> ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/e2e)
endif()
