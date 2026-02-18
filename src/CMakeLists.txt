
add_executable(MyGame
    core/Game.cpp
    core/main.cpp
    entities/Player.cpp
    physics/Physics.cpp
)

target_include_directories(MyGame PRIVATE src ../include)

target_link_libraries(MyGame
    raylib
)