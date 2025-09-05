#pragma once

#include <unordered_map>
#include <string>

namespace engine
{
    struct AxisBinding
    {
        int positiveKey = 0;
        int negativeKey = 0;
        float scale = 1.0f;
    };

    class InputMap
    {
    public:
        void bindAction(const std::string& name, int key) { m_actions[name] = key; }
        void bindAxis(const std::string& name, const AxisBinding& b) { m_axes[name] = b; }
        void setAxisKey(const std::string& name, bool positive, int key)
        {
            auto& a = m_axes[name];
            if (positive) a.positiveKey = key; else a.negativeKey = key;
        }

        int actionKey(const std::string& name) const
        {
            auto it = m_actions.find(name);
            return it == m_actions.end() ? 0 : it->second;
        }

        AxisBinding axis(const std::string& name) const
        {
            auto it = m_axes.find(name);
            return it == m_axes.end() ? AxisBinding{} : it->second;
        }

    private:
        std::unordered_map<std::string, int> m_actions;
        std::unordered_map<std::string, AxisBinding> m_axes;
    };
}


