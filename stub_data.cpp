#include "stub_data.h"
#include "preview_mode.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

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
    
    // Define realistic F1-style driver data
    s_stubCars = {
        // name,       carNum, lic, iRating, isSelf, isBuddy, isFlagged, pos, bestLap,  lastLap,   lapCount, pitAge
        {"You",        "77",   'A', 2156,    true,   false,   false,     2,   84.567f,  84.623f,   12,       15},
        {"Hamilton",   "44",   'A', 3245,    false,  true,    false,     6,   84.734f,  84.801f,   12,       16}, 
        {"Verstappen", "1",    'A', 4789,    false,  false,   false,     5,   84.456f,  84.512f,   12,       14},
        {"Leclerc",    "16",   'A', 3891,    false,  false,   false,     3,   84.321f,  84.389f,   13,       17}, // Fastest lap
        {"Russell",    "63",   'B', 2945,    false,  false,   false,     1,   84.678f,  84.723f,   13,       1},  // Recent pit
        {"Sainz",      "55",   'A', 2767,    false,  false,   false,     7,   84.823f,  84.891f,   12,       18},
        {"Norris",     "4",    'B', 3234,    false,  false,   false,     4,   84.612f,  84.667f,   12,       16},
        {"Piastri",    "81",   'A', 2534,    false,  false,   true,      8,   84.945f,  84.998f,   11,       12}  // Flagged
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
    
    // Clear existing car data and populate with stub data
    for (size_t i = 0; i < s_stubCars.size() && i < IR_MAX_CARS; ++i)
    {
        const StubCar& stubCar = s_stubCars[i];
        Car& car = const_cast<Car&>(ir_session.cars[i]);
        
        car.userName = stubCar.name;
        car.carNumberStr = stubCar.carNumber;
        car.carNumber = std::stoi(stubCar.carNumber);
        car.licenseChar = stubCar.license;
        car.irating = stubCar.irating;
        car.isSelf = stubCar.isSelf ? 1 : 0;
        car.isPaceCar = 0;
        car.isSpectator = 0;
        car.isBuddy = stubCar.isBuddy ? 1 : 0;
        car.isFlagged = stubCar.isFlagged ? 1 : 0;
        car.lastLapInPits = stubCar.lapCount - stubCar.pitAge;
    }
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
    // Create realistic RPM pattern with gear shifts
    float baseRPM = 4200.0f;
    float variation = 800.0f * std::sin(s_animationTime * 0.8f) + 300.0f * std::sin(s_animationTime * 2.1f);
    return std::max(2000.0f, std::min(7500.0f, baseRPM + variation));
}

float StubDataManager::getStubSpeed()
{
    updateAnimation();
    // Speed correlates with RPM but with some variation
    float rpm = getStubRPM();
    return (rpm / 7500.0f) * 180.0f + 20.0f; // 20-200 km/h range
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
    return 8; // Currently on lap 8
}

int StubDataManager::getStubLapsRemaining()
{
    return 15; // 15 laps remaining in race
}

float StubDataManager::getStubSessionTimeRemaining()
{
    return 1800.0f; // 30 minutes remaining
}

// Inputs-specific stub data
float StubDataManager::getStubThrottle()
{
    updateAnimation();
    // Create dynamic racing-like inputs
    float throttle = 0.6f + 0.3f * std::sin(s_animationTime * 0.8f) + 0.1f * std::sin(s_animationTime * 2.1f);
    return std::max(0.0f, std::min(1.0f, throttle));
}

float StubDataManager::getStubBrake()
{
    float throttle = getStubThrottle();
    // Brake opposite to throttle with some trail braking
    float brake = throttle < 0.4f ? (0.8f - throttle * 1.5f) : 0.0f;
    return std::max(0.0f, std::min(1.0f, brake));
}

float StubDataManager::getStubSteering()
{
    updateAnimation();
    // Steering with cornering patterns  
    float steer = 0.5f + 0.25f * std::sin(s_animationTime * 0.5f) + 0.1f * std::sin(s_animationTime * 1.2f);
    return std::max(0.1f, std::min(0.9f, steer));
}

// Relative-specific stub data
std::vector<StubDataManager::RelativeInfo> StubDataManager::getRelativeData()
{
    initialize();
    
    std::vector<RelativeInfo> relatives;
    
    // Define deltas relative to "You" (position 2)
    const float deltas[] = {-3.2f, 0.0f, +1.8f, +4.1f, +7.5f, +12.3f, +18.7f, +25.2f};
    
    for (size_t i = 0; i < s_stubCars.size(); ++i)
    {
        RelativeInfo info;
        info.carIdx = (int)i;
        info.delta = deltas[i];
        info.lapDelta = 0; // All on same lap for simplicity
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
    // Realistic track temperature that varies slightly
    return 32.5f + 2.0f * std::sin(s_animationTime * 0.1f);
}

float StubDataManager::getStubAirTemp()
{
    updateAnimation();
    // Air temperature usually cooler than track
    return 28.0f + 1.5f * std::sin(s_animationTime * 0.08f);
}

float StubDataManager::getStubTrackWetness()
{
    updateAnimation();
    // Simulate varying track wetness (0.0 = dry, 1.0 = extremely wet)
    float baseWetness = 0.3f + 0.2f * std::sin(s_animationTime * 0.05f);
    return std::max(0.0f, std::min(1.0f, baseWetness));
}

float StubDataManager::getStubPrecipitation()
{
    updateAnimation();
    // Simulate precipitation percentage (0.0 to 1.0)
    float basePrecip = 0.15f + 0.1f * std::sin(s_animationTime * 0.03f);
    return std::max(0.0f, std::min(1.0f, basePrecip));
}

float StubDataManager::getStubWindSpeed()
{
    updateAnimation();
    // Wind speed in m/s (0-15 m/s range)
    return 5.0f + 3.0f * std::sin(s_animationTime * 0.2f);
}

float StubDataManager::getStubWindDirection()
{
    updateAnimation();
    // Wind direction in radians, slowly rotating
    return fmod(s_animationTime * 0.1f, 2.0f * M_PI);
}
