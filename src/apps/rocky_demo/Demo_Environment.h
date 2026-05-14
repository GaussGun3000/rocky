/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/vsg/SkyNode.h>

#include "helpers.h"
using namespace ROCKY_NAMESPACE;

auto Demo_Environment = [](Application& app)
{
    static vsg::RegionOfInterest* roi = nullptr;
    static bool useCameraROI = false;

    auto mainView = app.display.window(0).view(0);
    auto skyNode = mainView.find<SkyNode>();

    if (skyNode == nullptr)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", "Sky is not installed; use --sky");

#if 0
        if (ImGui::Button("Install sky"))
        {
            app.vsgcontext->onNextUpdate([&app, mainView](VSGContext vsgcontext)
                {
                    if (auto light = detail::find<vsg::Light>(app.scene))
                        app.scene->children.erase(std::remove(app.scene->children.begin(), app.scene->children.end(), light), app.scene->children.end());
                    auto skyNode = SkyNode::create(vsgcontext);
                    mainView.vsgView->children.insert(mainView.vsgView->children.begin(), skyNode);
                    vsgcontext->viewer()->compile();
                });
        }
#endif

        app.vsgcontext->requestFrame();
        return;
    }

    if (skyNode->sun->shadowSettings)
    {
        // install a shadowing RegionOfInterest the first time through:
        if (!roi)
        {
            auto region = vsg::RegionOfInterest::create();
            roi = region;
            skyNode->children.emplace_back(region);
            //app.scene->children.insert(app.scene->children.begin(), region);
        }

        // apply the ROI if enabled:
        if (auto manip = MapManipulator::get(mainView.vsgView))
        {
            roi->points.clear();
            if (useCameraROI)
            {
                auto vp = manip->viewpoint();
                double r = vp.range->value();
                roi->points.emplace_back(to_vsg(vp.position() + glm::dvec3(r, 0, 0)));
                roi->points.emplace_back(to_vsg(vp.position() + glm::dvec3(-r, 0, 0)));
                roi->points.emplace_back(to_vsg(vp.position() + glm::dvec3(0, r, 0)));
                roi->points.emplace_back(to_vsg(vp.position() + glm::dvec3(0, -r, 0)));
                roi->points.emplace_back(to_vsg(vp.position() + glm::dvec3(0, 0, r)));
                roi->points.emplace_back(to_vsg(vp.position() + glm::dvec3(0, 0, -r)));
            }
        }
    }

    static DateTime dt;

    if (ImGuiLTable::Begin("environment"))
    {
        float hours = (float)dt.hours();
        if (ImGuiLTable::SliderFloat("Time of day (UTC)", &hours, 0.0f, 23.999f, "%.1f"))
        {
            dt = DateTime(dt.year(), dt.month(), dt.day(), hours);
            skyNode->setDateTime(dt);
        }

        ImGuiLTable::SliderFloat("Ambient intensity", &skyNode->ambient->intensity, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

        static bool atmo = true;
        if (ImGuiLTable::Checkbox("Show atmosphere", &atmo))
        {
            skyNode->setShowAtmosphere(atmo);
        }

        if (skyNode->sun->shadowSettings)
        {
            ImGuiLTable::SeparatorText("Shadows");
            // control the shadow distance in the main view:
            ImGuiLTable::SliderDouble("Shadow max distance", &mainView.vsgView->viewDependentState->maxShadowDistance, 0.0f, 50000.0f, "%.0lf", ImGuiSliderFlags_Logarithmic);
            ImGuiLTable::SliderDouble("Shadow map bias", &mainView.vsgView->viewDependentState->shadowMapBias, 0.0, 0.1, "%.5lf", ImGuiSliderFlags_Logarithmic);
            ImGuiLTable::SliderDouble("Shadow lambda", &mainView.vsgView->viewDependentState->lambda, 0.0, 1.0, "%.3lf");

            // add/move a shadowing ROI on the camera:
            ImGuiLTable::Checkbox("Camera ROI", &useCameraROI);
        }
        ImGuiLTable::End();
    }
};
