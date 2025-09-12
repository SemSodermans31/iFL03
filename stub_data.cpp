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
    
    // Define realistic F1-style driver data with class assignments
    s_stubCars = {
        // name,       carNum, lic, iRating, isSelf, isBuddy, isFlagged, pos, bestLap,  lastLap,   lapCount, pitAge, classId
        // Class 0 (Red)
        {"You",        "31",   'C', 2156,    true,   false,   false,     2,   84.567f,  84.623f,   12,       15,      0},
        {"Norris",     "4",    'B', 3234,    false,  false,   false,     4,   84.612f,  84.667f,   12,       16,      0},
        // Class 1 (Green)
        {"Hamilton",   "44",   'A', 3245,    false,  true,    false,     6,   84.734f,  84.801f,   12,       16,      1},
        {"Piastri",    "81",   'A', 2534,    false,  false,   true,      8,   84.945f,  84.998f,   11,       12,      1},
        // Class 2 (Magenta)
        {"Verstappen", "1",    'A', 4789,    false,  false,   false,     5,   84.456f,  84.512f,   12,       14,      2},
        {"Perez",      "11",   'A', 3602,    false,  false,   false,     9,   84.789f,  84.832f,   12,       13,      2},
        // Class 3 (Orange)
        {"Leclerc",    "16",   'A', 3891,    false,  false,   false,     3,   84.321f,  84.389f,   13,       17,      3},
        {"Sainz",      "55",   'A', 2767,    false,  false,   false,     7,   84.823f,  84.891f,   12,       18,      3},
        // Class 4 (Cyan)
        {"Russell",    "63",   'B', 2945,    false,  false,   false,     1,   84.678f,  84.723f,   13,       1,       4},
        {"Albon",      "23",   'B', 2140,    false,  false,   false,    10,   85.102f,  85.201f,   12,       11,      4},
        // Class 5 (Yellow)
        {"Gasly",      "10",   'A', 2680,    false,  false,   false,    11,   85.345f,  85.401f,   12,       9,       5},
        {"Ocon",       "31",   'A', 2615,    false,  false,   false,    12,   85.389f,  85.445f,   12,       8,       5}
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
    float baseRPM = 4200.0f;
    float variation = 800.0f * std::sin(s_animationTime * 0.16f) + 300.0f * std::sin(s_animationTime * 0.42f);
    return std::max(2000.0f, std::min(7500.0f, baseRPM + variation));
}

float StubDataManager::getStubSpeed()
{
    updateAnimation();
    float rpm = getStubRPM();
    return (rpm / 7500.0f) * 180.0f + 20.0f;
}

int StubDataManager::getStubGear()
{
    float speed = getStubSpeed();
    if (speed < 40) return 1;
    if (speed < 70) return 2;
    if (speed < 100) return 3;
    if (speed < 130) return 4;
    if (speed < 160) return 5;
    return 6;
}

int StubDataManager::getStubLap()
{
    return 8;
}

int StubDataManager::getStubLapsRemaining()
{
    return 15;
}

float StubDataManager::getStubSessionTimeRemaining()
{
    return 1800.0f;
}

int StubDataManager::getStubTargetLap()
{
    return 5;
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
    //Lap time
    return 90.462f;
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
    
    const float deltas[] = {-3.2f, 0.0f, +1.8f, +4.1f, +7.5f, +12.3f, +18.7f, +25.2f};
    
    for (size_t i = 0; i < s_stubCars.size(); ++i)
    {
        RelativeInfo info;
        info.carIdx = (int)i;
        info.delta = deltas[i];
        info.lapDelta = 0;
        info.pitAge = s_stubCars[i].pitAge;
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
