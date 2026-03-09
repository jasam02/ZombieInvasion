cmake_minimum_required(VERSION 3.20)
project(ZombieInvasion)

set(CMAKE_CXX_STANDARD 17)

set(SFML_DIR "C:/Libraries/SFML-3.0.2/lib/cmake/SFML")

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(ZombieInvasion main.cpp)

target_link_libraries(ZombieInvasion
    SFML::Graphics
    SFML::Window
    SFML::System
)

add_custom_command(TARGET ZombieInvasion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:ZombieInvasion>/assets
)