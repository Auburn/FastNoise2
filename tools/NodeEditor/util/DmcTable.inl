namespace DMC
{
    enum EdgeCode : uint16_t
    {
        EDGE0 = 1,
        EDGE1 = 1 << 1,
        EDGE2 = 1 << 2,
        EDGE3 = 1 << 3,
        EDGE4 = 1 << 4,
        EDGE5 = 1 << 5,
        EDGE6 = 1 << 6,
        EDGE7 = 1 << 7,
        EDGE8 = 1 << 8,
        EDGE9 = 1 << 9,
        EDGE10 = 1 << 10,
        EDGE11 = 1 << 11,
    };

    //  Coordinate system
    //
    //       y
    //       |
    //       |
    //       |
    //       0-----x
    //      /
    //     /
    //    z
    //

    // Cell Corners
    // (Corners are voxels. Number correspond to Morton codes of corner coordinates)
    //
    //       2-------------------3
    //      /|                  /|
    //     / |                 / |
    //    /  |                /  |
    //   6-------------------7   |
    //   |   |               |   |
    //   |   |               |   |
    //   |   |               |   |
    //   |   |               |   |
    //   |   0---------------|---1
    //   |  /                |  /
    //   | /                 | /
    //   |/                  |/
    //   4-------------------5
    //


    //         Cell Edges
    //
    //       o--------4----------o
    //      /|                  /|
    //     7 |                 5 |
    //    /  |                /  |
    //   o--------6----------o   |
    //   |   8               |   9
    //   |   |               |   |
    //   |   |               |   |
    //   11  |               10  |
    //   |   o--------0------|---o
    //   |  /                |  /
    //   | 3                 | 1
    //   |/                  |/
    //   o--------2----------o
    //

    // Encodes the edge vertices for the 256 marching cubes cases.
    // A marching cube case produces up to four faces and ,thus, up to four
    // dual points.

    const uint16_t kDualPointsList[256][4] = {
        { 0, 0, 0, 0 }, // 0
        { EDGE0 | EDGE3 | EDGE8, 0, 0, 0 }, // 1
        { EDGE0 | EDGE1 | EDGE9, 0, 0, 0 }, // 2
        { EDGE1 | EDGE3 | EDGE8 | EDGE9, 0, 0, 0 }, // 3
        { EDGE4 | EDGE7 | EDGE8, 0, 0, 0 }, // 4
        { EDGE0 | EDGE3 | EDGE4 | EDGE7, 0, 0, 0 }, // 5
        { EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 6
        { EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE9, 0, 0, 0 }, // 7
        { EDGE4 | EDGE5 | EDGE9, 0, 0, 0 }, // 8
        { EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 9
        { EDGE0 | EDGE1 | EDGE4 | EDGE5, 0, 0, 0 }, // 10
        { EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE8, 0, 0, 0 }, // 11
        { EDGE5 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 12
        { EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE9, 0, 0, 0 }, // 13
        { EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE8, 0, 0, 0 }, // 14
        { EDGE1 | EDGE3 | EDGE5 | EDGE7, 0, 0, 0 }, // 15
        { EDGE2 | EDGE3 | EDGE11, 0, 0, 0 }, // 16
        { EDGE0 | EDGE2 | EDGE8 | EDGE11, 0, 0, 0 }, // 17
        { EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 18
        { EDGE1 | EDGE2 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 19
        { EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 20
        { EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE11, 0, 0, 0 }, // 21
        { EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, 0 }, // 22
        { EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 23
        { EDGE4 | EDGE5 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 24
        { EDGE0 | EDGE2 | EDGE8 | EDGE11, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 25
        { EDGE0 | EDGE1 | EDGE4 | EDGE5, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 26
        { EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE8 | EDGE11, 0, 0, 0 }, // 27
        { EDGE5 | EDGE7 | EDGE8 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 28
        { EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 29
        { EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 30
        { EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE11, 0, 0, 0 }, // 31
        { EDGE1 | EDGE2 | EDGE10, 0, 0, 0 }, // 32
        { EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 33
        { EDGE0 | EDGE2 | EDGE9 | EDGE10, 0, 0, 0 }, // 34
        { EDGE2 | EDGE3 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 35
        { EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 36
        { EDGE0 | EDGE3 | EDGE4 | EDGE7, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 37
        { EDGE0 | EDGE2 | EDGE9 | EDGE10, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 38
        { EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 39
        { EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 40
        { EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0 }, // 41
        { EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE10, 0, 0, 0 }, // 42
        { EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, 0, 0, 0 }, // 43
        { EDGE5 | EDGE7 | EDGE8 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 44
        { EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE9, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 45
        { EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 46
        { EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE10, 0, 0, 0 }, // 47
        { EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0, 0 }, // 48
        { EDGE0 | EDGE1 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 49
        { EDGE0 | EDGE3 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 50
        { EDGE8 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 51
        { EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0 }, // 52
        { EDGE0 | EDGE1 | EDGE4 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 53
        { EDGE0 | EDGE3 | EDGE9 | EDGE10 | EDGE11, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 54
        { EDGE4 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 55
        { EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0 }, // 56
        { EDGE0 | EDGE1 | EDGE8 | EDGE10 | EDGE11, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 57
        { EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE10 | EDGE11, 0, 0, 0 }, // 58
        { EDGE4 | EDGE5 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 59
        { EDGE5 | EDGE7 | EDGE8 | EDGE9, EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0 }, // 60
        { EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 61
        { EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 62
        { EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 63
        { EDGE6 | EDGE7 | EDGE11, 0, 0, 0 }, // 64
        { EDGE0 | EDGE3 | EDGE8, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 65
        { EDGE0 | EDGE1 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 66
        { EDGE1 | EDGE3 | EDGE8 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 67
        { EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 68
        { EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE11, 0, 0, 0 }, // 69
        { EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0 }, // 70
        { EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 71
        { EDGE4 | EDGE5 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 72
        { EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0 }, // 73
        { EDGE0 | EDGE1 | EDGE4 | EDGE5, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 74
        { EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE8, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 75
        { EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 76
        { EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 77
        { EDGE0 | EDGE1 | EDGE5 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 78
        { EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE11, 0, 0, 0 }, // 79
        { EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0, 0 }, // 80
        { EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE8, 0, 0, 0 }, // 81
        { EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0 }, // 82
        { EDGE1 | EDGE2 | EDGE6 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 83
        { EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE8, 0, 0, 0 }, // 84
        { EDGE0 | EDGE2 | EDGE4 | EDGE6, 0, 0, 0 }, // 85
        { EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE8, 0, 0 }, // 86
        { EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE9, 0, 0, 0 }, // 87
        { EDGE4 | EDGE5 | EDGE9, EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0 }, // 88
        { EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE8, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 89
        { EDGE0 | EDGE1 | EDGE4 | EDGE5, EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0 }, // 90
        { EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE8, 0, 0, 0 }, // 91
        { EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE8 | EDGE9, 0, 0, 0 }, // 92
        { EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE9, 0, 0, 0 }, // 93
        { EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE8, 0, 0, 0 }, // 94
        { EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0, 0 }, // 95
        { EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 96
        { EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0 }, // 97
        { EDGE0 | EDGE2 | EDGE9 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 98
        { EDGE2 | EDGE3 | EDGE8 | EDGE9 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 99
        { EDGE4 | EDGE6 | EDGE8 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 100
        { EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 101
        { EDGE0 | EDGE2 | EDGE9 | EDGE10, EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0 }, // 102
        { EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 103
        { EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0 }, // 104
        { EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE2 | EDGE10, EDGE6 | EDGE7 | EDGE11 }, // 105
        { EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 106
        { EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 107
        { EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 108
        { EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 109
        { EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 110
        { EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE10 | EDGE11, 0, 0, 0 }, // 111
        { EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE10, 0, 0, 0 }, // 112
        { EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 113
        { EDGE0 | EDGE3 | EDGE6 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 114
        { EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 115
        { EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE8 | EDGE10, 0, 0, 0 }, // 116
        { EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE10, 0, 0, 0 }, // 117
        { EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 118
        { EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0, 0 }, // 119
        { EDGE4 | EDGE5 | EDGE9, EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE10, 0, 0 }, // 120
        { EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, EDGE4 | EDGE5 | EDGE9, 0, 0 }, // 121
        { EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE10, 0, 0, 0 }, // 122
        { EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 123
        { EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 124
        { EDGE0 | EDGE1 | EDGE5 | EDGE6 | EDGE9 | EDGE10, 0, 0, 0 }, // 125
        { EDGE0 | EDGE3 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 126
        { EDGE5 | EDGE6 | EDGE10, 0, 0, 0 }, // 127
        { EDGE5 | EDGE6 | EDGE10, 0, 0, 0 }, // 128
        { EDGE0 | EDGE3 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 129
        { EDGE0 | EDGE1 | EDGE9, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 130
        { EDGE1 | EDGE3 | EDGE8 | EDGE9, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 131
        { EDGE4 | EDGE7 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 132
        { EDGE0 | EDGE3 | EDGE4 | EDGE7, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 133
        { EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, EDGE5 | EDGE6 | EDGE10, 0 }, // 134
        { EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE9, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 135
        { EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0, 0 }, // 136
        { EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0 }, // 137
        { EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE10, 0, 0, 0 }, // 138
        { EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE8 | EDGE10, 0, 0, 0 }, // 139
        { EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 140
        { EDGE0 | EDGE3 | EDGE6 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 141
        { EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 142
        { EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE10, 0, 0, 0 }, // 143
        { EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 144
        { EDGE0 | EDGE2 | EDGE8 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 145
        { EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0 }, // 146
        { EDGE1 | EDGE2 | EDGE8 | EDGE9 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 147
        { EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0 }, // 148
        { EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 149
        { EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE7 | EDGE8, EDGE2 | EDGE3 | EDGE11, EDGE5 | EDGE6 | EDGE10 }, // 150
        { EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, EDGE5 | EDGE6 | EDGE10, 0, 0 }, // 151
        { EDGE4 | EDGE6 | EDGE9 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 152
        { EDGE0 | EDGE2 | EDGE8 | EDGE11, EDGE4 | EDGE6 | EDGE9 | EDGE10, 0, 0 }, // 153
        { EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 154
        { EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 155
        { EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 156
        { EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 157
        { EDGE0 | EDGE1 | EDGE6 | EDGE7 | EDGE8 | EDGE10, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 158
        { EDGE1 | EDGE2 | EDGE6 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 159
        { EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0, 0 }, // 160
        { EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0 }, // 161
        { EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE9, 0, 0, 0 }, // 162
        { EDGE2 | EDGE3 | EDGE5 | EDGE6 | EDGE8 | EDGE9, 0, 0, 0 }, // 163
        { EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0 }, // 164
        { EDGE0 | EDGE3 | EDGE4 | EDGE7, EDGE1 | EDGE2 | EDGE5 | EDGE6, 0, 0 }, // 165
        { EDGE0 | EDGE2 | EDGE5 | EDGE6 | EDGE9, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 166
        { EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE9, 0, 0, 0 }, // 167
        { EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE9, 0, 0, 0 }, // 168
        { EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE4 | EDGE6 | EDGE9, 0, 0 }, // 169
        { EDGE0 | EDGE2 | EDGE4 | EDGE6, 0, 0, 0 }, // 170
        { EDGE2 | EDGE3 | EDGE4 | EDGE6 | EDGE8, 0, 0, 0 }, // 171
        { EDGE1 | EDGE2 | EDGE6 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 172
        { EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE6 | EDGE7 | EDGE9, 0, 0, 0 }, // 173
        { EDGE0 | EDGE2 | EDGE6 | EDGE7 | EDGE8, 0, 0, 0 }, // 174
        { EDGE2 | EDGE3 | EDGE6 | EDGE7, 0, 0, 0 }, // 175
        { EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE11, 0, 0, 0 }, // 176
        { EDGE0 | EDGE1 | EDGE5 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 177
        { EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 178
        { EDGE5 | EDGE6 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 179
        { EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE3 | EDGE5 | EDGE6 | EDGE11, 0, 0 }, // 180
        { EDGE0 | EDGE1 | EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE11, 0, 0, 0 }, // 181
        { EDGE0 | EDGE3 | EDGE5 | EDGE6 | EDGE9 | EDGE11, EDGE4 | EDGE7 | EDGE8, 0, 0 }, // 182
        { EDGE4 | EDGE5 | EDGE6 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 183
        { EDGE1 | EDGE3 | EDGE4 | EDGE6 | EDGE9 | EDGE11, 0, 0, 0 }, // 184
        { EDGE0 | EDGE1 | EDGE4 | EDGE6 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 185
        { EDGE0 | EDGE3 | EDGE4 | EDGE6 | EDGE11, 0, 0, 0 }, // 186
        { EDGE4 | EDGE6 | EDGE8 | EDGE11, 0, 0, 0 }, // 187
        { EDGE1 | EDGE3 | EDGE6 | EDGE7 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 188
        { EDGE0 | EDGE1 | EDGE9, EDGE6 | EDGE7 | EDGE11, 0, 0 }, // 189
        { EDGE0 | EDGE3 | EDGE6 | EDGE7 | EDGE8 | EDGE11, 0, 0, 0 }, // 190
        { EDGE6 | EDGE7 | EDGE11, 0, 0, 0 }, // 191
        { EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 192
        { EDGE0 | EDGE3 | EDGE8, EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0 }, // 193
        { EDGE0 | EDGE1 | EDGE9, EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0 }, // 194
        { EDGE1 | EDGE3 | EDGE8 | EDGE9, EDGE5 | EDGE7 | EDGE10 | EDGE11, 0, 0 }, // 195
        { EDGE4 | EDGE5 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 196
        { EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE10 | EDGE11, 0, 0, 0 }, // 197
        { EDGE0 | EDGE1 | EDGE9, EDGE4 | EDGE5 | EDGE8 | EDGE10 | EDGE11, 0, 0 }, // 198
        { EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 199
        { EDGE4 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 200
        { EDGE0 | EDGE3 | EDGE8, EDGE4 | EDGE7 | EDGE9 | EDGE10 | EDGE11, 0, 0 }, // 201
        { EDGE0 | EDGE1 | EDGE4 | EDGE7 | EDGE10 | EDGE11, 0, 0, 0 }, // 202
        { EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 203
        { EDGE8 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 204
        { EDGE0 | EDGE3 | EDGE9 | EDGE10 | EDGE11, 0, 0, 0 }, // 205
        { EDGE0 | EDGE1 | EDGE8 | EDGE10 | EDGE11, 0, 0, 0 }, // 206
        { EDGE1 | EDGE3 | EDGE10 | EDGE11, 0, 0, 0 }, // 207
        { EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE10, 0, 0, 0 }, // 208
        { EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE8 | EDGE10, 0, 0, 0 }, // 209
        { EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE10, 0, 0 }, // 210
        { EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 211
        { EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, 0, 0, 0 }, // 212
        { EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE10, 0, 0, 0 }, // 213
        { EDGE0 | EDGE1 | EDGE9, EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE10, 0, 0 }, // 214
        { EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE9 | EDGE10, 0, 0, 0 }, // 215
        { EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE9 | EDGE10, 0, 0, 0 }, // 216
        { EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 217
        { EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE10, 0, 0, 0 }, // 218
        { EDGE4 | EDGE7 | EDGE8, EDGE1 | EDGE2 | EDGE10, 0, 0 }, // 219
        { EDGE2 | EDGE3 | EDGE8 | EDGE9 | EDGE10, 0, 0, 0 }, // 220
        { EDGE0 | EDGE2 | EDGE9 | EDGE10, 0, 0, 0 }, // 221
        { EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE8 | EDGE10, 0, 0, 0 }, // 222
        { EDGE1 | EDGE2 | EDGE10, 0, 0, 0 }, // 223
        { EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE11, 0, 0, 0 }, // 224
        { EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE5 | EDGE7 | EDGE11, 0, 0 }, // 225
        { EDGE0 | EDGE2 | EDGE5 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 226
        { EDGE2 | EDGE3 | EDGE5 | EDGE7 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 227
        { EDGE1 | EDGE2 | EDGE4 | EDGE5 | EDGE8 | EDGE11, 0, 0, 0 }, // 228
        { EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE4 | EDGE5 | EDGE11, 0, 0, 0 }, // 229
        { EDGE0 | EDGE2 | EDGE4 | EDGE5 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 230
        { EDGE4 | EDGE5 | EDGE9, EDGE2 | EDGE3 | EDGE11, 0, 0 }, // 231
        { EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, 0, 0, 0 }, // 232
        { EDGE0 | EDGE3 | EDGE8, EDGE1 | EDGE2 | EDGE4 | EDGE7 | EDGE9 | EDGE11, 0, 0 }, // 233
        { EDGE0 | EDGE2 | EDGE4 | EDGE7 | EDGE11, 0, 0, 0 }, // 234
        { EDGE2 | EDGE3 | EDGE4 | EDGE7 | EDGE8 | EDGE11, 0, 0, 0 }, // 235
        { EDGE1 | EDGE2 | EDGE8 | EDGE9 | EDGE11, 0, 0, 0 }, // 236
        { EDGE0 | EDGE1 | EDGE2 | EDGE3 | EDGE9 | EDGE11, 0, 0, 0 }, // 237
        { EDGE0 | EDGE2 | EDGE8 | EDGE11, 0, 0, 0 }, // 238
        { EDGE2 | EDGE3 | EDGE11, 0, 0, 0 }, // 239
        { EDGE1 | EDGE3 | EDGE5 | EDGE7, 0, 0, 0 }, // 240
        { EDGE0 | EDGE1 | EDGE5 | EDGE7 | EDGE8, 0, 0, 0 }, // 241
        { EDGE0 | EDGE3 | EDGE5 | EDGE7 | EDGE9, 0, 0, 0 }, // 242
        { EDGE5 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 243
        { EDGE1 | EDGE3 | EDGE4 | EDGE5 | EDGE8, 0, 0, 0 }, // 244
        { EDGE0 | EDGE1 | EDGE4 | EDGE5, 0, 0, 0 }, // 245
        { EDGE0 | EDGE3 | EDGE4 | EDGE5 | EDGE8 | EDGE9, 0, 0, 0 }, // 246
        { EDGE4 | EDGE5 | EDGE9, 0, 0, 0 }, // 247
        { EDGE1 | EDGE3 | EDGE4 | EDGE7 | EDGE9, 0, 0, 0 }, // 248
        { EDGE0 | EDGE1 | EDGE4 | EDGE7 | EDGE8 | EDGE9, 0, 0, 0 }, // 249
        { EDGE0 | EDGE3 | EDGE4 | EDGE7, 0, 0, 0 }, // 250
        { EDGE4 | EDGE7 | EDGE8, 0, 0, 0 }, // 251
        { EDGE1 | EDGE3 | EDGE8 | EDGE9, 0, 0, 0 }, // 252
        { EDGE0 | EDGE1 | EDGE9, 0, 0, 0 }, // 253
        { EDGE0 | EDGE3 | EDGE8, 0, 0, 0 }, // 254
        { 0, 0, 0, 0 } // 255
    };

} // namespace dualmc
