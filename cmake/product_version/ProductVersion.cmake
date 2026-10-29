# ProductVersion.cmake -- LaneTools product versioning spec (identical copy in every repo).
# product_version_add(<exe target> <product>) links <product>_version_info, a static library built from
# one generated .cpp. The SHA never enters a header or a compile definition: only that one object
# changes when HEAD changes, every other TU keeps its compiler-cache hit.
set(_PV_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")
function(product_version_add target product)
  set(lib "${product}_version_info")
  if(NOT TARGET ${lib})
    set(out "${CMAKE_CURRENT_BINARY_DIR}/${product}_version_info.cpp")
    set(gen ${CMAKE_COMMAND} -DPV_PRODUCT=${product} -DPV_SRC=${CMAKE_CURRENT_SOURCE_DIR}
        -DPV_OUT=${out} -P ${_PV_DIR}/product_version_gen.cmake)
    execute_process(COMMAND ${gen})
    add_custom_target(${product}_version_gen COMMAND ${gen} BYPRODUCTS ${out} VERBATIM)
    add_library(${lib} STATIC ${out})
    target_include_directories(${lib} PUBLIC ${_PV_DIR})
    add_dependencies(${lib} ${product}_version_gen)
  endif()
  target_link_libraries(${target} PRIVATE ${lib})
  # Deferred to the end of the top directory: enable_testing() there may come after this call.
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]] CALL _pv_add_test [[${target}]] [[${product}]])")
endfunction()

function(_pv_add_test target product)
  add_test(NAME product_version_${product} COMMAND ${target} --version-json)
  set_tests_properties(product_version_${product} PROPERTIES LABELS "fast;version"
    PASS_REGULAR_EXPRESSION "\"product\":\"${product}\",\"version\":\"[0-9]+[.][0-9]+[.][0-9]+\",\"sha\":")
endfunction()
