project(yaostake)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( yaostake yaostake yaostake.cpp )
target_include_directories( yaostake PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( yaostake ${CMAKE_SOURCE_DIR}/../ricardian )