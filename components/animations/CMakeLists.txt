idf_component_register(
    SRCS
        "engine.cpp"
        "anims/plasma.cpp"
        "anims/game_of_life.cpp"
        "anims/starfield.cpp"
        "anims/fire.cpp"
        "anims/sorting.cpp"
        "anims/maze.cpp"
        "anims/langton.cpp"
        "anims/matrix_rain.cpp"
        "anims/boids.cpp"
        "anims/asteroids.cpp"
        "anims/fireworks.cpp"
        "anims/lava_lamp.cpp"
        "anims/snake.cpp"
        "anims/synthwave.cpp"
        "anims/cube3d.cpp"
        "anims/fractal_zoom.cpp"
        "anims/bouncing_balls.cpp"
    INCLUDE_DIRS "include"
    REQUIRES panel
    PRIV_REQUIRES esp_timer nvs_flash
)
