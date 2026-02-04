
add_executable(MyGame
    Game.cpp
    main.cpp
    glad.c
    Camera.cpp
)

target_include_directories(MyGame PRIVATE src ../include)

target_link_libraries(MyGame
    SFML::Graphics
    SFML::Window
    SFML::System
    OpenGL::GL
)