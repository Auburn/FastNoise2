[![discord](https://img.shields.io/discord/703636892901441577?style=flat-square&logo=discord "Discord")](https://discord.gg/SHVaVfV)

# FastNoise2

WIP successor to [FastNoiseSIMD](https://github.com/Auburn/FastNoiseSIMD)

Modular node based approach to noise generation using modern C++17 features and templates

Uses FastSIMD to compile classes with multiple SIMD types

Currently being developed on Visual Studio 2019 x64, will be more crossplatform in the future

Check the [releases](https://github.com/Auburns/FastNoise2/releases) for early versions of the Noise Tool

![NoiseTool](https://user-images.githubusercontent.com/1349548/84082690-31196780-a9d8-11ea-8db7-168e27599f90.png)

# Getting Started

There are 2 ways to use FastNoise 2, creating a node tree structure in code or importing a serialised node tree created using the NoiseTool.

This is creating a Simplex Fractal FBm with 5 octaves from code:
```
auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();

fnFractal->SetSource( fnSimplex );
fnFractal->SetOctaveCount( 5 );

fnFractal->GenUniformGrid2D( ... );
```

Here is the same Simplex Fractal FBm with 5 octaves but using serialised data from the NoiseTool:
```
FastNoise::SmartNode<> fnGenerator = FastNoise::NewFromEncodedNodeTree( "DQAFAAAAAAAAQAgAAAAAAD8=" );

fnGenerator->GenUniformGrid2D( ... );
```
This is the node graph for the above from the NoiseTool

![SimplexFractalNodes](https://user-images.githubusercontent.com/1349548/90897006-72f16180-e3bc-11ea-8cc3-a68daed7b6c1.png)
