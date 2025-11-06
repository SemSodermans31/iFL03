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

#include "iracing.h"
#include <vector>
#include <cmath>

// Centralized stub data system for preview mode
// Provides realistic racing data for all overlays when iRacing is not connected

class StubDataManager
{
public:
    // Car information for stub data
    struct StubCar
    {
        const char* name;
        const char* carNumber;
        char license;
        int irating;
        bool isSelf;
        bool isBuddy;
        bool isFlagged;
        int position;
        float bestLapTime;
        float lastLapTime;
        int lapCount;
        int pitAge;
        int classId;
        int tireCompound;
    };

    // Initialize stub data (call once)
    static void initialize();
    
    // Check if stub data should be used
    static bool shouldUseStubData();
    
    // Populate ir_session.cars with stub data
    static void populateSessionCars();
    
    // Get stub car data
    static const std::vector<StubCar>& getStubCars();
    
    // DDU-specific stub data
    static float getStubRPM();
    static float getStubSpeed();
    static int getStubGear();
    static int getStubLap();
    static int getStubLapsRemaining();
    static float getStubSessionTimeRemaining();
    static int getStubTargetLap();
    
    // Inputs-specific stub data
    static float getStubThrottle();
    static float getStubBrake();
    static float getStubClutch();
    static float getStubSteering();

    // Delta timing-specific stub data
    static float getStubDeltaToSessionBest();
    static float getStubSessionBestLapTime();
    static bool getStubDeltaValid();
    
    // Relative-specific stub data
    struct RelativeInfo
    {
        int carIdx;
        float delta;
        int lapDelta;
        int pitAge;
        float minimapX;  // X position on track map (0.0 to 1.0)
        float minimapY;  // Y position on track map (0.0 to 1.0)
        int tireCompound;
    };
    static std::vector<RelativeInfo> getRelativeData();
    
    // Get stub car by index
    static const StubCar* getStubCar(int carIdx);
    
    // Fuel-specific stub data
    static float getStubFuelLevel();
    static float getStubFuelLevelPct();
    static float getStubPitServiceFuel();
    static bool getStubFuelFillAvailable();
    static float getStubFuelPerLap();

    // Weather-specific stub data
    static float getStubTrackTemp();
    static float getStubAirTemp();
    static float getStubTrackWetness();
    static float getStubPrecipitation();
    static float getStubWindSpeed();
    static float getStubWindDirection();
    
    // Animation timing
    static float getAnimationTime() { return s_animationTime; }
    static void updateAnimation();

private:
    static std::vector<StubCar> s_stubCars;
    static bool s_initialized;
    static float s_animationTime;
};
