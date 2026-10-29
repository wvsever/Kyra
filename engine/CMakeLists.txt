add_library(kyra_core STATIC
  src/generate.cpp
  src/eligibility.cpp
  src/export.cpp
  src/ra.cpp
)
target_include_directories(kyra_core PUBLIC include)
kyra_target_options(kyra_core)

add_executable(kyra src/main.cpp)
target_link_libraries(kyra PRIVATE kyra_core)
kyra_target_options(kyra)
product_version_add(kyra kyra)
