#include "scripting/LuaEngine.h"
#include "scene/Scene.h"

#include <lua.hpp>
#include <cstdio>
#include <sys/stat.h>

namespace engine
{
    static long long file_mtime(const std::string& path)
    {
        struct _stat64 st; if (_stat64(path.c_str(), &st) == 0) return (long long)st.st_mtime; return 0;
    }

    LuaEngine::LuaEngine() = default;
    LuaEngine::~LuaEngine() { shutdown(); }

    bool LuaEngine::initialize()
    {
        m_L = luaL_newstate();
        if (!m_L) return false;
        luaL_openlibs(m_L);
        // register C API
        lua_register(m_L, "get_entity_pos", l_get_entity_pos);
        lua_register(m_L, "set_entity_pos", l_set_entity_pos);
        return true;
    }

    void LuaEngine::shutdown()
    {
        if (m_L) { lua_close(m_L); m_L = nullptr; }
        m_entities.clear();
        m_fileTimes.clear();
    }

    bool LuaEngine::pushModule(const std::string& path)
    {
        if (luaL_dofile(m_L, path.c_str()) != LUA_OK)
        {
            const char* err = lua_tostring(m_L, -1);
            std::fprintf(stderr, "[Lua] %s\n", err ? err : "unknown error");
            lua_pop(m_L, 1);
            return false;
        }
        return true;
    }

    bool LuaEngine::loadScript(const std::string& path)
    {
        bool ok = pushModule(path);
        if (ok) m_fileTimes[path] = file_mtime(path);
        return ok;
    }

    void LuaEngine::unloadScript(const std::string& path)
    {
        // naive: clear package cache and GC
        lua_getglobal(m_L, "package");
        lua_getfield(m_L, -1, "loaded");
        lua_pushstring(m_L, path.c_str());
        lua_pushnil(m_L);
        lua_settable(m_L, -3);
        lua_pop(m_L, 2);
        lua_gc(m_L, LUA_GCCOLLECT, 0);
    }

    void LuaEngine::onUpdate(float dt)
    {
        if (!m_L) return;
        if (m_hotReload) pollHotReload();
        // call global update(dt) if present
        lua_getglobal(m_L, "update");
        if (lua_isfunction(m_L, -1))
        {
            lua_pushnumber(m_L, dt);
            if (lua_pcall(m_L, 1, 0, 0) != LUA_OK)
            {
                const char* err = lua_tostring(m_L, -1);
                std::fprintf(stderr, "[Lua] update error: %s\n", err ? err : "");
                lua_pop(m_L, 1);
            }
        }
        else lua_pop(m_L, 1);
    }

    void LuaEngine::bindEntity(int id, Entity* e)
    {
        m_entities.push_back({id, e});
    }

    void LuaEngine::pollHotReload()
    {
        for (auto& kv : m_fileTimes)
        {
            long long now = file_mtime(kv.first);
            if (now != 0 && now != kv.second)
            {
                std::fprintf(stderr, "[Lua] Reloading %s\n", kv.first.c_str());
                unloadScript(kv.first);
                pushModule(kv.first);
                kv.second = now;
            }
        }
    }

    int LuaEngine::l_get_entity_pos(lua_State* L)
    {
        // get by index passed from lua side; for demo return (0,0,0)
        (void)L; lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); return 3;
    }
    int LuaEngine::l_set_entity_pos(lua_State* L)
    {
        (void)L; return 0;
    }
}


