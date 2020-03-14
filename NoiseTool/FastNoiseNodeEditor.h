#pragma once
#include <vector>

class FastNoiseNodeEditor
{
public:
    void Update();


private:
    struct Node
    {
        int id;
        float value;
    };

    struct Link
    {
        int id;
        int startAttr, endAttr;
    };

    std::vector<Node> mNodes;
    std::vector<Link> mLinks;
    int mCurrentNodeId = 0;
};