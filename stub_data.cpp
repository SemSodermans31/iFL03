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

#include "stub_data.h"
#include "preview_mode.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include "ClassColors.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Static member definitions
std::vector<StubDataManager::StubCar> StubDataManager::s_stubCars;
bool StubDataManager::s_initialized = false;
float StubDataManager::s_animationTime = 0.0f;

void StubDataManager::initialize()
{
    if (s_initialized) return;
    
    // Define realistic F3-style driver data based on 2025 Hungary race
    // Using authentic F3 lap times from Hungaroring (pole ~1:48.5, race laps ~1:48-1:55)
    s_stubCars = {
        // name,           carNum, lic, iRating, isSelf, isBuddy, isFlagged, pos, bestLap,    lastLap,     lapCount, pitAge, classId
        // Class 0 (Red) - F3 Championship contenders
        {"You",                "31",   'A', 2850,    true,   false,   false,     3,   108.542f,   108.623f,   15,       8,       0},
        {"Alex Thompson",      "7",    'A', 3120,    false,  false,   false,     1,   108.456f,   108.512f,   15,       7,       0},
        // Class 1 (Green) - Mid-field competitive
        {"Carlos Martinez",    "12",   'A', 2980,    false,  true,    false,     5,   108.734f,   108.801f,   15,       6,       1},
        {"Miguel Rodriguez",   "22",   'A', 2765,    false,  false,   true,      8,   109.145f,   109.198f,   14,       5,       1},
        // Class 2 (Magenta) - Points contenders
        {"Jae-woo Kim",        "9",    'A', 3025,    false,  false,   false,     2,   108.498f,   108.567f,   15,       9,       2},
        {"Wei Chen",           "15",   'A', 2890,    false,  false,   false,     7,   108.945f,   108.987f,   15,       4,       2},
        // Class 3 (Orange) - Backmarkers with pace
        {"Arjun Patel",        "18",   'B', 2650,    false,  false,   false,     6,   109.321f,   109.389f,   15,       10,      3},
        {"Pierre Dubois",      "25",   'B', 2520,    false,  false,   false,     9,   109.623f,   109.691f,   15,       3,       3},
        // Class 4 (Cyan) - Rookie drivers
        {"Lukas Novak",        "4",    'B', 2380,    false,  false,   false,     4,   108.678f,   108.723f,   15,       2,       4},
        {"Erik Andersson",     "11",   'C', 2150,    false,  false,   false,    10,   110.102f,   110.201f,   14,       11,      4},
        // Class 5 (Yellow) - Strugglers
        {"Antonio Silva",      "27",   'C', 1980,    false,  false,   false,    11,   110.845f,   110.901f,   15,       12,      5},
        {"Marek Kowalski",     "30",   'C', 1920,    false,  false,   false,    12,   111.089f,   111.145f,   15,       13,      5}
    };
    
    s_initialized = true;
}

bool StubDataManager::shouldUseStubData()
{
    return preview_mode_should_use_stub_data();
}

void StubDataManager::populateSessionCars()
{
    if (!shouldUseStubData()) return;
    
    initialize();
    
    ir_session.sessionType = SessionType::PRACTICE;
    ir_session.driverCarIdx = -1;

    auto makeColor = [](float r, float g, float b){ return float4(r, g, b, 1.0f); };
    auto licenseColorFor = [&](char lic)->float4{
        switch(lic){
            case 'A': return makeColor(0.10f, 0.45f, 0.95f); 
            case 'B': return makeColor(0.15f, 0.70f, 0.20f); 
            case 'C': return makeColor(0.95f, 0.80f, 0.10f); 
            case 'D': return makeColor(0.95f, 0.55f, 0.10f); 
            default:  return makeColor(0.50f, 0.50f, 0.50f);
        }
    };
    auto licenseSrFor = [&](char lic)->float{
        switch(lic){
            case 'A': return 4.50f;
            case 'B': return 3.50f;
            case 'C': return 2.50f;
            case 'D': return 1.50f;
            default:  return 0.0f;
        }
    };

    for (size_t i = 0; i < s_stubCars.size() && i < IR_MAX_CARS; ++i)
    {
        const StubCar& stubCar = s_stubCars[i];
        Car& car = const_cast<Car&>(ir_session.cars[i]);
        
        car.userName = stubCar.name;
        car.teamName = stubCar.name;
        car.carNumberStr = stubCar.carNumber;
        car.carNumber = std::stoi(stubCar.carNumber);
        car.licenseChar = stubCar.license;
        car.licenseSR = licenseSrFor(stubCar.license);
        car.licenseCol = licenseColorFor(stubCar.license);
        car.irating = stubCar.irating;
        car.isSelf = stubCar.isSelf ? 1 : 0;
        car.isPaceCar = 0;
        car.isSpectator = 0;
        car.isBuddy = stubCar.isBuddy ? 1 : 0;
        car.isFlagged = stubCar.isFlagged ? 1 : 0;
        car.classId = stubCar.classId;
        car.classCol = ClassColors::get(car.classId);
        
        // Assign car brands for icon display in preview mode (names chosen to match available PNG files)
        const char* carBrands[] = {
            "Ferrari 296 GT3", "Mercedes AMG", "BMW M4", "McLaren 720S",
            "Aston Martin Vantage", "Alpine A110", "Ford GT", "Porsche 911",
            "Alfa Romeo Giulia", "Chevrolet Corvette", "Audi R8", "Lamborghini Huracan",
            "Toyota Supra", "Mazda MX-5", "Subaru BRZ", "Honda NSX",
            "Volvo XC90", "Tesla Model S", "VW Golf", "Mini Cooper"
        };
        car.carName = carBrands[i % (sizeof(carBrands)/sizeof(carBrands[0]))];
        car.carID = (int)i + 1;  // Unique car ID for icon mapping
        car.practice.position = (int)i + 1;
        car.qualy.position = (int)i + 1;
        car.race.position = stubCar.position > 0 ? stubCar.position : (int)i + 1;
        car.practice.lastTime = stubCar.lastLapTime;
        car.practice.fastestTime = stubCar.bestLapTime;
        car.lastLapInPits = stubCar.lapCount - stubCar.pitAge;

        if (car.isSelf)
            ir_session.driverCarIdx = (int)i;
    }

    // Fallback if none marked self
    if (ir_session.driverCarIdx < 0 && !s_stubCars.empty())
        ir_session.driverCarIdx = 0;
}

const std::vector<StubDataManager::StubCar>& StubDataManager::getStubCars()
{
    initialize();
    return s_stubCars;
}

void StubDataManager::updateAnimation()
{
    s_animationTime += 0.016f; // ~60 FPS
}

// DDU-specific stub data
float StubDataManager::getStubRPM()
{
    updateAnimation();
    float baseRPM = 4800.0f; // More realistic for F3 turbo engine
    float variation = 1200.0f * std::sin(s_animationTime * 0.16f) + 400.0f * std::sin(s_animationTime * 0.42f);
    return std::max(2500.0f, std::min(6800.0f, baseRPM + variation)); // F3 rev limit ~6800 RPM
}

float StubDataManager::getStubSpeed()
{
    updateAnimation();
    float rpm = getStubRPM();
    return (rpm / 6800.0f) * 160.0f + 25.0f; // More realistic for F3 at Hungaroring
}

int StubDataManager::getStubGear()
{
    float speed = getStubSpeed();
    if (speed < 35) return 1;
    if (speed < 55) return 2;
    if (speed < 75) return 3;
    if (speed < 100) return 4;
    if (speed < 125) return 5;
    return 6;
}

int StubDataManager::getStubLap()
{
    return 8;
}

int StubDataManager::getStubLapsRemaining()
{
    return 12; // More realistic for F3 race (typically 20-25 total laps)
}

float StubDataManager::getStubSessionTimeRemaining()
{
    return 1310.0f; // ~21.8 minutes for 12 laps at ~109s each
}

int StubDataManager::getStubTargetLap()
{
    return 8; // More realistic target lap for mid-race
}

float StubDataManager::getStubThrottle()
{
    updateAnimation();
    float throttle = 0.6f + 0.3f * std::sin(s_animationTime * 0.032f) + 0.1f * std::sin(s_animationTime * 0.084f);
    return std::max(0.0f, std::min(1.0f, throttle));
}

float StubDataManager::getStubBrake()
{
    float throttle = getStubThrottle();
    float brake = throttle < 0.4f ? (0.8f - throttle * 1.5f) : 0.0f;
    return std::max(0.0f, std::min(1.0f, brake));
}

float StubDataManager::getStubClutch()
{
    updateAnimation();
    int gear = getStubGear();
    static int lastGear = gear;
    static float clutchAnimation = 0.0f;
    
    if (gear != lastGear)
    {
        clutchAnimation = 1.0f;
        lastGear = gear;
    }
    
    clutchAnimation = std::max(0.0f, clutchAnimation - 0.01f);  

    float clutchSlip = 0.1f * std::sin(s_animationTime * 0.12f); 
    return std::max(0.0f, std::min(1.0f, clutchAnimation + clutchSlip));
}

float StubDataManager::getStubSteering()
{
    updateAnimation();
    float steer = 0.5f + 0.25f * std::sin(s_animationTime * 0.1f) + 0.1f * std::sin(s_animationTime * 0.24f);
    return std::max(0.1f, std::min(0.9f, steer));
}

float StubDataManager::getStubDeltaToSessionBest()
{
    updateAnimation();
    float baseDelta = std::sin(s_animationTime * 0.02f) * 1.5f - 0.2f; 

    float trackProgress = std::fmod(s_animationTime * 0.008f, 1.0f); 
    float sectorVariation = std::sin(trackProgress * 6.28318f * 3.0f) * 0.5f;
    
    return baseDelta + sectorVariation;
}

float StubDataManager::getStubSessionBestLapTime()
{
    // F3 pole time from 2025 Hungary race
    return 108.456f;
}

bool StubDataManager::getStubDeltaValid()
{
    updateAnimation();
    return s_animationTime > 5.0f;
}

std::vector<StubDataManager::RelativeInfo> StubDataManager::getRelativeData()
{
    initialize();

    std::vector<RelativeInfo> relatives;

    // Reorganize to show YOU in P3 position in relative overlay
    // Show cars around YOU's position (positions 1-7 relative to YOU)
    const int relativeOrder[] = {1, 2, 0, 4, 5, 6, 3}; // Car indices: Thompson(1), Kim(2), You(0), Novak(4), Patel(5), Chen(6), Martinez(3)
    const float deltas[] = {-2.1f, -1.2f, 0.0f, +1.8f, +3.2f, +5.4f, +7.8f}; // Deltas relative to YOU

    // Minimap positions for Hungaroring (simplified oval approximation)
    const float minimapPositions[][2] = {
        {0.85f, 0.3f},   // Thompson (P1) - ahead
        {0.75f, 0.25f},  // Kim (P2) - ahead
        {0.65f, 0.2f},   // YOU (P3) - reference
        {0.55f, 0.35f},  // Novak (P4) - behind
        {0.45f, 0.4f},   // Patel (P5) - behind
        {0.35f, 0.45f},  // Chen (P6) - behind
        {0.25f, 0.5f}    // Martinez (P7) - behind
    };

    for (size_t i = 0; i < 7 && i < sizeof(relativeOrder)/sizeof(relativeOrder[0]); ++i)
    {
        int carIdx = relativeOrder[i];
        RelativeInfo info;
        info.carIdx = carIdx;
        info.delta = deltas[i];
        info.lapDelta = 0;
        info.pitAge = s_stubCars[carIdx].pitAge;
        info.minimapX = minimapPositions[i][0];
        info.minimapY = minimapPositions[i][1];
        relatives.push_back(info);
    }

    return relatives;
}

const StubDataManager::StubCar* StubDataManager::getStubCar(int carIdx)
{
    initialize();
    if (carIdx >= 0 && carIdx < (int)s_stubCars.size())
        return &s_stubCars[carIdx];
    return nullptr;
}

// Weather-specific stub data
float StubDataManager::getStubTrackTemp()
{
    updateAnimation();
    return 32.5f + 2.0f * std::sin(s_animationTime * 0.02f);
}

float StubDataManager::getStubAirTemp()
{
    updateAnimation();
    return 28.0f + 1.5f * std::sin(s_animationTime * 0.016f);
}

float StubDataManager::getStubTrackWetness()
{
    updateAnimation();
    float baseWetness = 0.3f + 0.2f * std::sin(s_animationTime * 0.01f);
    return std::max(0.0f, std::min(1.0f, baseWetness));
}

float StubDataManager::getStubPrecipitation()
{
    updateAnimation();
    float basePrecip = 0.15f + 0.1f * std::sin(s_animationTime * 0.006f);
    return std::max(0.0f, std::min(1.0f, basePrecip));
}

float StubDataManager::getStubWindSpeed()
{
    updateAnimation();
    return 5.0f + 3.0f * std::sin(s_animationTime * 0.04f);
}

float StubDataManager::getStubWindDirection()
{
    updateAnimation();
    return static_cast<float>(fmod(s_animationTime * 0.02f, 2.0f * M_PI));
}
