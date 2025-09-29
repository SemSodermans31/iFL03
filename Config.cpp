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


#include <atomic>
#include <filesystem>
#include <algorithm>
#include "Config.h"
#include "Logger.h"

Config              g_cfg;

static void configWatcher( std::atomic<bool>* m_hasChanged )
{
    HANDLE dir = CreateFile( ".", FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    if( dir == INVALID_HANDLE_VALUE )
    {
        printf( "Could not start config watch thread.\n" );
        return;
    }

    std::vector<DWORD> buf( 1024*1024 );
    DWORD bytesReturned = 0;

    while( true )
    {        
        if( ReadDirectoryChangesW( dir, buf.data(), (DWORD)buf.size()/sizeof(DWORD), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, &bytesReturned, NULL, NULL ) )
        {
            Sleep( 100 ); 
            *m_hasChanged = true;
        }
    }
}

bool Config::load()
{
    std::string json;
    if( !loadFile(m_filename, json) )
    {
        Logger::instance().logError("Failed to load config file " + m_filename + " (loadFile returned false)");
        return false;
    }

    picojson::value pjval;
    std::string parseError = picojson::parse( pjval, json );
    if( !parseError.empty() )
    {
        Logger::instance().logError("Config file parse error: " + parseError);
        printf("Config file is not valid JSON!\n%s\n", parseError.c_str() );
        return false;
    }

    m_pj = pjval.get<picojson::object>();
    m_hasChanged = false;
    return true;
}

bool Config::save()
{
    const picojson::value value = picojson::value( m_pj );
    const std::string json = value.serialize(true);
    const bool ok = saveFile( m_filename, json );
    if( !ok ) {
        char s[1024];
        GetCurrentDirectory( sizeof(s), s );
        printf("Could not save config file! Please make sure iFL03 is started from a directory for which it has write permissions. The current directory is: %s.\n", s);
        std::string msg = "Could not save config file (" + m_filename + ") from directory " + s;
        Logger::instance().logError(msg);
    }
    return ok;
}

void Config::watchForChanges()
{
    m_configWatchThread = std::thread( configWatcher, &m_hasChanged );
    m_configWatchThread.detach();
}

bool Config::hasChanged()
{
    return m_hasChanged;
}

bool Config::getBool( const std::string& component, const std::string& key, bool defaultVal )
{
    bool existed = false;
    picojson::value& value = getOrInsertValue( component, key, &existed );

    if( !existed )
        value.set<bool>( defaultVal );

    return value.get<bool>();
}

int Config::getInt( const std::string& component, const std::string& key, int defaultVal )
{
    bool existed = false;
    picojson::value& value = getOrInsertValue( component, key, &existed );

    if( !existed )
        value.set<double>( defaultVal );

    return (int)value.get<double>();
}

float Config::getFloat( const std::string& component, const std::string& key, float defaultVal )
{
    bool existed = false;
    picojson::value& value = getOrInsertValue( component, key, &existed );

    if( !existed )
        value.set<double>( defaultVal );

    return (float)value.get<double>();
}

float4 Config::getFloat4( const std::string& component, const std::string& key, const float4& defaultVal )
{
    bool existed = false;
    picojson::value& value = getOrInsertValue( component, key, &existed );

    if( !existed )
    {
        picojson::array arr( 4 );
        arr[0].set<double>( defaultVal.x );
        arr[1].set<double>( defaultVal.y );
        arr[2].set<double>( defaultVal.z );
        arr[3].set<double>( defaultVal.w );
        value.set<picojson::array>( arr );
    }

    picojson::array& arr = value.get<picojson::array>();
    float4 ret;
    ret.x = (float)arr[0].get<double>();
    ret.y = (float)arr[1].get<double>();
    ret.z = (float)arr[2].get<double>();
    ret.w = (float)arr[3].get<double>();
    return ret;
}

std::string Config::getString( const std::string& component, const std::string& key, const std::string& defaultVal )
{
    bool existed = false;
    picojson::value& value = getOrInsertValue( component, key, &existed );

    if( !existed )
        value.set<std::string>( defaultVal );

    return value.get<std::string>();
}

std::vector<std::string> Config::getStringVec( const std::string& component, const std::string& key, const std::vector<std::string>& defaultVal )
{
    bool existed = false;
    picojson::value& value = getOrInsertValue( component, key, &existed );

    if( !existed )
    {
        picojson::array arr( defaultVal.size() );
        for( int i=0; i<(int)defaultVal.size(); ++i )
            arr[i].set<std::string>( defaultVal[i] );
        value.set<picojson::array>( arr );
    }

    picojson::array& arr = value.get<picojson::array>();
    std::vector<std::string> ret;
    ret.reserve( arr.size() );
    for( picojson::value& entry : arr )
        ret.push_back( entry.get<std::string>() );
    return ret;
}

void Config::setStringVec( const std::string& component, const std::string& key, const std::vector<std::string>& v )
{
    picojson::object& pjcomp = getOrInsertComponent( component );
    picojson::array arr;
    arr.reserve(v.size());
    for (const std::string& s : v) {
        picojson::value val;
        val.set<std::string>(s);
        arr.push_back(val);
    }
    pjcomp[key].set<picojson::array>(arr);
}

void Config::setInt( const std::string& component, const std::string& key, int v )
{
    picojson::object& pjcomp = getOrInsertComponent( component );
    double d = double(v);
    pjcomp[key].set<double>( d );
}

void Config::setBool( const std::string& component, const std::string& key, bool v )
{
    picojson::object& pjcomp = getOrInsertComponent( component );
    pjcomp[key].set<bool>( v );
}

void Config::setString( const std::string& component, const std::string& key, const std::string& v )
{
    picojson::object& pjcomp = getOrInsertComponent( component );
    pjcomp[key].set<std::string>( v );
}

void Config::setFloat( const std::string& component, const std::string& key, float v )
{
    picojson::object& pjcomp = getOrInsertComponent( component );
    pjcomp[key].set<double>( static_cast<double>(v) );
}

picojson::object& Config::getOrInsertComponent( const std::string& component, bool* existed )
{
    auto it = m_pj.insert(std::make_pair(component,picojson::object()));
    
    if( existed )
        *existed = !it.second;

    return it.first->second.get<picojson::object>();
}

picojson::value& Config::getOrInsertValue( const std::string& component, const std::string& key, bool* existed )
{
    picojson::object& comp = getOrInsertComponent( component );

    auto it = comp.insert(std::make_pair(key,picojson::value()));

    if( existed )
        *existed = !it.second;

    return it.first->second;
}

std::string Config::sanitizeCarName( const std::string& carName ) const
{
    std::string sanitized = carName;
    // Replace spaces and invalid filename characters with underscores
    std::replace_if(sanitized.begin(), sanitized.end(), 
        [](char c) { return c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; }, 
        '_');
    return sanitized;
}

std::string Config::getCarConfigFilename( const std::string& carName ) const
{
    if( carName.empty() )
        return "config.json";
    return "config_" + sanitizeCarName(carName) + ".json";
}

bool Config::loadCarConfig( const std::string& carName )
{
    std::string carFilename = getCarConfigFilename(carName);
    std::string json;
    
    // Try to load car-specific config
    if( !loadFile(carFilename, json) )
    {
        // If car config doesn't exist, try to load default config as base
        if( !loadFile("config.json", json) )
        {
            Logger::instance().logError("Failed to load car config " + carFilename + " and fallback config.json");
            return false;
        }
    }

    picojson::value pjval;
    std::string parseError = picojson::parse( pjval, json );
    if( !parseError.empty() )
    {
        Logger::instance().logError("Car config parse error: " + parseError);
        printf("Car config file is not valid JSON!\n%s\n", parseError.c_str() );
        return false;
    }

    m_pj = pjval.get<picojson::object>();
    m_filename = carFilename;
    m_currentCarName = carName;
    m_hasChanged = false;
    return true;
}

bool Config::saveCarConfig( const std::string& carName )
{
    std::string carFilename = getCarConfigFilename(carName);
    const picojson::value value = picojson::value( m_pj );
    const std::string json = value.serialize(true);
    const bool ok = saveFile( carFilename, json );
    if( !ok ) {
        char s[1024];
        GetCurrentDirectory( sizeof(s), s );
        printf("Could not save car config file! Please make sure iFL03 is started from a directory for which it has write permissions. The current directory is: %s.\n", s);
    }
    return ok;
}

bool Config::hasCarConfig( const std::string& carName )
{
    std::string carFilename = getCarConfigFilename(carName);
    std::string json;
    return loadFile(carFilename, json);
}

bool Config::copyConfigToCar( const std::string& fromCar, const std::string& toCar )
{
    // Save current state
    picojson::object currentPj = m_pj;
    std::string currentFilename = m_filename;
    std::string currentCarName = m_currentCarName;
    
    // Load source config
    bool loadOk = false;
    if( fromCar.empty() )
    {
        // Copy from default config
        loadOk = load();
    }
    else
    {
        loadOk = loadCarConfig(fromCar);
    }
    
    if( !loadOk )
    {
        Logger::instance().logError("Failed to load source config when copying from " + fromCar + " to " + toCar);
        // Restore previous state
        m_pj = currentPj;
        m_filename = currentFilename;
        m_currentCarName = currentCarName;
        return false;
    }
    
    // Save to target car
    bool saveOk = saveCarConfig(toCar);
    
    // Restore previous state
    m_pj = currentPj;
    m_filename = currentFilename;
    m_currentCarName = currentCarName;
    
    return saveOk;
}

std::vector<std::string> Config::getAvailableCarConfigs()
{
    std::vector<std::string> carConfigs;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator("."))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                if (filename.starts_with("config_") && filename.ends_with(".json"))
                {
                    // Extract car name from filename
                    std::string carName = filename.substr(7);
                    carName = carName.substr(0, carName.length() - 5);
                    
                    // Restore spaces (reverse sanitization - basic version)
                    std::replace(carName.begin(), carName.end(), '_', ' ');
                    carConfigs.push_back(carName);
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& ex) {
        printf("Error reading car configs: %s\n", ex.what());
    }
    
    std::sort(carConfigs.begin(), carConfigs.end());
    return carConfigs;
}

bool Config::deleteCarConfig( const std::string& carName )
{
    if( carName.empty() )
        return false;
        
    std::string carFilename = getCarConfigFilename(carName);
    std::ifstream ifs(carFilename);
    if (!ifs)
    {
        Logger::instance().logError("Failed to open car config file " + carFilename + " for delete check");
        return false;
    }
    ifs.close();
    return DeleteFileA(carFilename.c_str()) != 0;
}
