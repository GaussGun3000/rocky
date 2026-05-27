/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"

using namespace ROCKY_NAMESPACE;

namespace
{
    struct SceneGraphInspectorVisitor : public vsg::Inherit<vsg::Visitor, SceneGraphInspectorVisitor>
    {
        using vsg::Visitor::apply;

        int depth = 0;

        void apply(vsg::Object& object) override
        {
            render(object);
        }

        void apply(vsg::Group& group) override
        {
            render(group);
        }

        void apply(vsg::QuadGroup& group) override
        {
            render(group);
        }

        void render(vsg::Object& object)
        {
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            bool root = depth == 0;

            if (root)
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            bool open = root ?
                ImGui::TreeNodeEx(&object, flags, "app.scene (%s)", object.className()) :
                ImGui::TreeNodeEx(&object, flags, "%s", object.className());

            if (open)
            {
                ++depth;
                object.traverse(*this);
                --depth;

                ImGui::TreePop();
            }
        }
    };
}

auto Demo_SceneGraphInspector = [](Application& app)
{
    if (app.scene)
    {
        SceneGraphInspectorVisitor inspector;
        app.scene->accept(inspector);
    }
    else
    {
        ImGui::TextUnformatted("No scene graph.");
    }
};
