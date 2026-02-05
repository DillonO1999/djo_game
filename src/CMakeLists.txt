
add_executable(MyGame
    Game.cpp
    main.cpp
)

target_include_directories(MyGame PRIVATE src ../include)

target_link_libraries(MyGame
    raylib
)