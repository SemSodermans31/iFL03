/*
MIT License

Copyright (c) 2021-2025 L. E. Spalt & Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "util.h"

namespace ClassColors
{
    // Special cars
    inline float4 self()      { return float4(0.20f, 0.40f, 1.00f, 1.00f); } // Blue
    inline float4 paceCar()   { return float4(1.00f, 1.00f, 1.00f, 1.00f); } // White
    inline float4 safetyCar() { return float4(1.00f, 0.50f, 0.00f, 1.00f); } // Orange

    // Multiclass palette (extendable; indexes map to classId)
    inline float4 get(int classId)
    {
        static const float4 kColors[] = {
            float4(0.63f, 0.24f, 0.35f, 1.00f), // 0 - #a03c5a
            float4(0.69f, 0.60f, 0.06f, 1.00f), // 1 - #b1990e
            float4(0.09f, 0.45f, 0.57f, 1.00f), // 2 - #187492
            float4(0.09f, 0.58f, 0.30f, 1.00f), // 3 - #18934c
            float4(0.09f, 0.14f, 0.58f, 1.00f), // 4 - #182493
        };
        const int count = (int)(sizeof(kColors)/sizeof(kColors[0]));
        if (count == 0) return float4(1,1,1,1);
        const int idx = classId >= 0 ? (classId % count) : 0;
        return kColors[idx];
    }

    // Lighter variants for labels/badges, kept in sync with primary palette
    inline float4 getLight(int classId)
    {
        static const float4 kLight[] = {
            float4(1.0f, 0.345f, 0.533f, 1.0f), // 0 - #ff5888
            float4(1.0f, 0.855f, 0.349f, 1.0f), // 1 - #ffda59
            float4(0.200f, 0.808f, 1.0f, 1.0f), // 2 - #33ceff
            float4(0.278f, 1.0f, 0.584f, 1.0f), // 3 - #47ff95
            float4(0.369f, 0.431f, 1.0f, 1.0f)  // 4 - #5e6eff
        };
        const int count = (int)(sizeof(kLight)/sizeof(kLight[0]));
        if (count == 0) return float4(1,1,1,1);
        const int idx = classId >= 0 ? (classId % count) : 0;
        return kLight[idx];
    }

    // Darker variants for text, kept in sync with primary palette
    inline float4 getDark(int classId)
    {
        static const float4 kDark[] = {
            float4(0.314f, 0.106f, 0.169f, 1.0f), // 0 - #501b2b
            float4(0.349f, 0.298f, 0.122f, 1.0f), // 1 - #594c1f
            float4(0.063f, 0.255f, 0.318f, 1.0f), // 2 - #104151
            float4(0.082f, 0.294f, 0.173f, 1.0f), // 3 - #154b2c
            float4(0.102f, 0.125f, 0.329f, 1.0f)  // 4 - #1a2054
        };
        const int count = (int)(sizeof(kDark)/sizeof(kDark[0]));
        if (count == 0) return float4(1,1,1,1);
        const int idx = classId >= 0 ? (classId % count) : 0;
        return kDark[idx];
    }
}