
add_executable(WikiGenerator
    main.cpp
)

target_link_libraries(WikiGenerator PRIVATE
    FastNoise
)