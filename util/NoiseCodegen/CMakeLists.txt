add_executable(NoiseCodegen
    main.cpp
)

target_link_libraries(NoiseCodegen PRIVATE
    FastNoise
)
