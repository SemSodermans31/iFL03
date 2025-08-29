/*
MIT License

Copyright (c) 2021-2022 L. E. Spalt

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
            float4(0.80f, 0.20f, 0.20f, 1.00f), // 0 - Red
            float4(0.20f, 0.80f, 0.20f, 1.00f), // 1 - Green
            float4(0.80f, 0.20f, 0.80f, 1.00f), // 2 - Magenta
            float4(0.80f, 0.50f, 0.20f, 1.00f), // 3 - Orange
            float4(0.20f, 0.80f, 0.80f, 1.00f), // 4 - Cyan
            float4(0.80f, 0.80f, 0.20f, 1.00f), // 5 - Yellow
            float4(0.50f, 0.20f, 0.80f, 1.00f), // 6 - Purple
            float4(0.20f, 0.50f, 0.80f, 1.00f), // 7 - Blue-gray
            float4(0.80f, 0.40f, 0.60f, 1.00f), // 8 - Pink
            float4(0.40f, 0.80f, 0.40f, 1.00f), // 9 - Light green
        };
        const int count = (int)(sizeof(kColors)/sizeof(kColors[0]));
        if (count == 0) return float4(1,1,1,1);
        const int idx = classId >= 0 ? (classId % count) : 0;
        return kColors[idx];
    }
}


