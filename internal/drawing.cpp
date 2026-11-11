#include "drawing.h"
#include <algorithm>

namespace DrawingAPI {
    DrawingManager& DrawingManager::Get() {
        static DrawingManager instance;
        return instance;
    }

    void DrawingManager::Add(std::shared_ptr<BaseType> obj) {
        m_Objects.push_back(obj);
    }

    void DrawingManager::Remove(std::shared_ptr<BaseType> obj) {
        auto it = std::find(m_Objects.begin(), m_Objects.end(), obj);
        if (it != m_Objects.end()) {
            m_Objects.erase(it);
        }
    }

    void DrawingManager::Clear() {
        for (auto& obj : m_Objects) {
            if (obj) obj->Destroy();
        }
        m_Objects.clear();
    }

    void DrawingManager::RenderAll() {
        // Sort by ZIndex
        std::sort(m_Objects.begin(), m_Objects.end(), [](const auto& a, const auto& b) {
            return a->m_ZIndex < b->m_ZIndex;
        });

        for (auto& obj : m_Objects) {
            if (obj && obj->m_Visible) {
                obj->Render();
            }
        }
    }
}
