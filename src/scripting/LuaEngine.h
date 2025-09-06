#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace engine
{
    struct Entity;

    class LuaEngine
    {
    public:
        LuaEngine();
        ~LuaEngine();

        bool initialize();
        void shutdown();

        // Load or reload a script file; returns a module key
        bool loadScript(const std::string& path);
        void unloadScript(const std::string& path);

        // Callbacks
        void onUpdate(float dt);

        // Bind entity API
        void bindEntity(int id, Entity* e);

        // Simple hot-reload: checks file timestamps
        void setHotReloadEnabled(bool enabled) { m_hotReload = enabled; }
        void pollHotReload();

    private:
        bool pushModule(const std::string& path);
        static int l_get_entity_pos(lua_State* L);
        static int l_set_entity_pos(lua_State* L);

    private:
        lua_State* m_L = nullptr;
        bool m_hotReload = true;
        std::unordered_map<std::string, long long> m_fileTimes;
        std::vector<std::pair<int, Entity*>> m_entities; // simple registry
    };
}


