#pragma once
#include "FastNoise/FastNoise.h"

namespace FastNoise
{
    // Generated trees have no settings, metadata is only needed to satisfy the Generator interface
    const Metadata& CodeGenBenchmarkMetadata();

    // Generator wrapping a node tree compiled to C++ by FastNoise::CodeGen, the whole tree is evaluated in one Gen() call
    template<int TREE>
    class CodeGenBenchmarkTree : public virtual Generator
    {
    public:
        const Metadata& GetMetadata() const override { return CodeGenBenchmarkMetadata(); }
    };
}
