add_executable(ec_smoke smoke.c)
target_link_libraries(ec_smoke PRIVATE editingcore)
set_target_properties(ec_smoke PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
add_test(NAME smoke COMMAND ec_smoke "${CMAKE_CURRENT_SOURCE_DIR}/smoke.lua")
