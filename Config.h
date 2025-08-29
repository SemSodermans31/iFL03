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

#include <windows.h>
#include <atomic>
#include <thread>
#include <vector>
#include "picojson.h"
#include "util.h"

class Config
{
    public:

        bool                        load();
        bool                        save();
        
        // Car-specific config methods
        bool                        loadCarConfig( const std::string& carName );
        bool                        saveCarConfig( const std::string& carName );
        bool                        hasCarConfig( const std::string& carName );
        bool                        copyConfigToCar( const std::string& fromCar, const std::string& toCar );
        std::vector<std::string>    getAvailableCarConfigs();
        bool                        deleteCarConfig( const std::string& carName );
        std::string                 getCurrentCarName() const { return m_currentCarName; }
        void                        setCurrentCarName( const std::string& carName ) { m_currentCarName = carName; }

        void                        watchForChanges();
        bool                        hasChanged();

        bool                        getBool( const std::string& component, const std::string& key, bool defaultVal );
        int                         getInt( const std::string& component, const std::string& key, int defaultVal );
        float                       getFloat( const std::string& component, const std::string& key, float defaultVal );
        float4                      getFloat4( const std::string& component, const std::string& key, const float4& defaultVal );
        std::string                 getString( const std::string& component, const std::string& key, const std::string& defaultVal );
        std::vector<std::string>    getStringVec( const std::string& component, const std::string& key, const std::vector<std::string>& defaultVal );

        void                        setInt( const std::string& component, const std::string& key, int v );
        void                        setBool( const std::string& component, const std::string& key, bool v );
        void                        setString( const std::string& component, const std::string& key, const std::string& v );
        void                        setFloat( const std::string& component, const std::string& key, float v );

    private:

        picojson::object&           getOrInsertComponent( const std::string& component, bool* existed=nullptr );
        picojson::value&            getOrInsertValue( const std::string& component, const std::string& key, bool* existed=nullptr );
        std::string                 sanitizeCarName( const std::string& carName ) const;
        std::string                 getCarConfigFilename( const std::string& carName ) const;

        picojson::object    m_pj;
        std::atomic<bool>   m_hasChanged = false;
        std::thread         m_configWatchThread;
        std::string         m_filename = "config.json";
        std::string         m_currentCarName;
};

extern Config        g_cfg;

