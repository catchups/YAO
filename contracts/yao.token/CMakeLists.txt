add_contract(yao.token yao.token ${CMAKE_CURRENT_SOURCE_DIR}/src/yao.token.cpp)

target_include_directories(yao.token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(yao.token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/yao.token.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/yao.token.contracts.md @ONLY )

target_compile_options( yao.token PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
