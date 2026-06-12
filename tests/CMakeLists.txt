cmake_minimum_required(VERSION 3.20)

add_executable(test_bsp     test_bsp.cpp)
add_executable(test_movement test_movement.cpp)
add_executable(test_wad     test_wad.cpp)
add_executable(test_entity  test_entity.cpp)

foreach(t test_bsp test_movement test_wad test_entity)
  target_link_libraries(${t} PRIVATE ostrike_engine)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
