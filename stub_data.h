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
    
    // Inputs-specific stub data
    static float getStubThrottle();
    static float getStubBrake();
    static float getStubSteering();
    
    // Relative-specific stub data
    struct RelativeInfo
    {
        int carIdx;
        float delta;
        int lapDelta;
        int pitAge;
    };
    static std::vector<RelativeInfo> getRelativeData();
    
    // Get stub car by index
    static const StubCar* getStubCar(int carIdx);
    
    // Weather-specific stub data
    static float getStubTrackTemp();
    static float getStubAirTemp();
    static float getStubTrackWetness();
    static float getStubPrecipitation();
    static float getStubWindSpeed();
    static float getStubWindDirection();

private:
    static std::vector<StubCar> s_stubCars;
    static bool s_initialized;
    static float s_animationTime;
    
    // Update animation time
    static void updateAnimation();
};
