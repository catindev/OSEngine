cmake_minimum_required(VERSION 3.20)

add_executable(test_bsp      test_bsp.cpp)
add_executable(test_movement test_movement.cpp)
add_executable(test_wad      test_wad.cpp)
add_executable(test_entity   test_entity.cpp)
add_executable(test_weapon   test_weapon.cpp)
add_executable(test_grenade  test_grenade.cpp)
add_executable(test_mdl_anim test_mdl_anim.cpp)

foreach(t test_bsp test_movement test_wad test_entity test_weapon test_grenade test_mdl_anim)
  target_link_libraries(${t} PRIVATE ostrike_engine)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
