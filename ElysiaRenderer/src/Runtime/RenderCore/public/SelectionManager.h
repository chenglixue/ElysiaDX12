#pragma once
#include "stdafx.h"

namespace ElysiaEngine
{
    struct Entity;
}

namespace ElysiaRenderer
{
    // Single source of truth for the editor selection state.
    // Scene Hierarchy / Inspector / Viewport all share this state:
    // a write from any view (tree click / viewport click) updates the others.
    //
    // Viewport picking follows the UE HitProxy scheme:
    //   1. A click only records a pending pick request (pixel coordinates).
    //   2. The renderer draws the entity IDs into a R32_UINT pick RT during
    //      the same frame and copies the clicked pixel to a readback buffer.
    //   3. On the next frame (after the GPU fence) the pixel is mapped back
    //      and resolved into an Entity, then Select() applies it.
    class SelectionManager
    {
    public:
        static SelectionManager& GetInstance()
        {
            static SelectionManager instance;
            return instance;
        }

        void Select(ElysiaEngine::Entity* pEntity);
        void Clear();

        ElysiaEngine::Entity* GetSelected() const noexcept
        {
            return m_pSelected;
        }

        // Called by the Viewport UI on click. Zero cost until the renderer
        // consumes the request.
        void RequestPick(const Vector2& viewportUV)
        {
            m_PickUV = viewportUV;
            m_bPickRequested = true;
        }

        // Consumed once per frame by the renderer that owns the pick RT.
        bool ConsumePickRequest(Vector2& outUV)
        {
            if (!m_bPickRequested)
            {
                return false;
            }
            outUV = m_PickUV;
            m_bPickRequested = false;
            return true;
        }

        // Called by the renderer after the readback resolved. A null entity
        // means "clicked background" and clears the selection.
        void ResolvePick(ElysiaEngine::Entity* pEntity);

        // Hierarchy scroll-to-selected request (set by viewport picking,
        // consumed once when the selected tree node is drawn).
        bool ConsumeScrollToSelected() noexcept
        {
            const bool value = m_bScrollToSelected;
            m_bScrollToSelected = false;
            return value;
        }

        // True while a scroll request is pending for an entity nested under
        // pEntity. The hierarchy uses it to auto-expand the ancestor chain, so a
        // collapsed child can actually become visible and be scrolled to.
        bool IsScrollTargetAncestor(const ElysiaEngine::Entity* pEntity) const;

    private:
        SelectionManager() = default;

        ElysiaEngine::Entity* m_pSelected = nullptr;
        bool m_bScrollToSelected = false;

        bool m_bPickRequested = false;
        Vector2 m_PickUV = Vector2::Zero;
    };
}
