/**
 * rocky c++
 * Copyright 2026 Pelican Mapping
 * MIT License
 */
#pragma once
#include "helpers.h"
#include <rocky/vsg/imgui/ImGuiImage.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/Value.h>
#include <vsg/nodes/Compilable.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/QuadGroup.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Switch.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/DescriptorSet.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>

using namespace ROCKY_NAMESPACE;

namespace
{
    template<typename T>
    std::string scene_graph_inspector_to_string(const T& value)
    {
        std::ostringstream buf;
        buf << value;
        return buf.str();
    }

    template<typename T>
    std::string scene_graph_inspector_format_vector(const T& value)
    {
        std::ostringstream buf;
        buf << std::setprecision(8) << "(";
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (i > 0)
                buf << ", ";
            buf << +value[i];
        }
        buf << ")";
        return buf.str();
    }

    std::string scene_graph_inspector_format_object(const vsg::Object* object)
    {
        char address[32];
        std::snprintf(address, sizeof(address), "%p", object);

        if (!object)
            return "<null>";
        else if (auto value = object->cast<vsg::stringValue>())
            return value->value();
        else if (auto value = object->cast<vsg::pathValue>())
            return value->value().string();
        else if (auto value = object->cast<vsg::boolValue>())
            return value->value() ? "true" : "false";
        else if (auto value = object->cast<vsg::intValue>())
            return scene_graph_inspector_to_string(value->value());
        else if (auto value = object->cast<vsg::uintValue>())
            return scene_graph_inspector_to_string(value->value());
        else if (auto value = object->cast<vsg::floatValue>())
            return scene_graph_inspector_to_string(value->value());
        else if (auto value = object->cast<vsg::doubleValue>())
            return scene_graph_inspector_to_string(value->value());
        else if (auto value = object->cast<vsg::vec2Value>())
            return scene_graph_inspector_format_vector(value->value());
        else if (auto value = object->cast<vsg::vec3Value>())
            return scene_graph_inspector_format_vector(value->value());
        else if (auto value = object->cast<vsg::vec4Value>())
            return scene_graph_inspector_format_vector(value->value());
        else if (auto value = object->cast<vsg::dvec2Value>())
            return scene_graph_inspector_format_vector(value->value());
        else if (auto value = object->cast<vsg::dvec3Value>())
            return scene_graph_inspector_format_vector(value->value());
        else if (auto value = object->cast<vsg::dvec4Value>())
            return scene_graph_inspector_format_vector(value->value());

        return std::string(object->className()) + " @ " + address;
    }

    bool scene_graph_inspector_child_count(const vsg::Node& node, std::size_t& count)
    {
        if (auto group = node.cast<vsg::Group>())
        {
            count = group->children.size();
            return true;
        }
        else if (auto group = node.cast<vsg::QuadGroup>())
        {
            count = group->children.size();
            return true;
        }
        else if (auto group = node.cast<vsg::Switch>())
        {
            count = group->children.size();
            return true;
        }
        else if (auto lod = node.cast<vsg::LOD>())
        {
            count = lod->children.size();
            return true;
        }
        else if (auto lod = node.cast<vsg::PagedLOD>())
        {
            count = lod->children.size();
            return true;
        }

        return false;
    }

    bool scene_graph_inspector_section(const char* label)
    {
        return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
    }

    void scene_graph_inspector_render_sphere(const char* label, const vsg::dsphere& bound)
    {
        if (scene_graph_inspector_section(label) && ImGuiLTable::Begin(label))
        {
            ImGuiLTable::Text("Center:", "%.3lf, %.3lf, %.3lf", bound.x, bound.y, bound.z);
            ImGuiLTable::Text("Radius:", "%.3lf", bound.r);
            ImGuiLTable::TextUnformatted("Valid:", bound.valid() ? "true" : "false");
            ImGuiLTable::End();
        }
    }

    const char* scene_graph_inspector_request_status(vsg::PagedLOD::RequestStatus status)
    {
        switch (status)
        {
        case vsg::PagedLOD::NoRequest: return "No request";
        case vsg::PagedLOD::ReadRequest: return "Read request";
        case vsg::PagedLOD::Reading: return "Reading";
        case vsg::PagedLOD::Compiling: return "Compiling";
        case vsg::PagedLOD::MergeRequest: return "Merge request";
        case vsg::PagedLOD::Merging: return "Merging";
        case vsg::PagedLOD::DeleteRequest: return "Delete request";
        case vsg::PagedLOD::Deleting: return "Deleting";
        default: return "Unknown";
        }
    }

    struct SceneGraphInspectorTexturePreview
    {
        struct Holder : public vsg::Inherit<vsg::Compilable, Holder>
        {
            void compile(vsg::Context& context) override
            {
                descriptorSet->compile(context);
            }

            vsg::ref_ptr<vsg::DescriptorSet> descriptorSet;
        };

        vsg::observer_ptr<vsg::ImageInfo> source;
        vsg::ref_ptr<vsg::ImageInfo> imageInfo;
        vsg::ref_ptr<Holder> holder;
        std::uint32_t deviceID = 0;
    };

    ImGuiTextureHandle scene_graph_inspector_texture_handle(vsg::DescriptorSet* descriptorSet, std::uint32_t deviceID)
    {
        auto id = descriptorSet ?
            reinterpret_cast<ImTextureID>(descriptorSet->vk(deviceID)) :
            ImTextureID{};

#if IMGUI_VERSION_NUM >= 19200
        return ImTextureRef(id);
#else
        return id;
#endif
    }

    SceneGraphInspectorTexturePreview* scene_graph_inspector_get_texture_preview(vsg::ImageInfo* imageInfo, VSGContext vsgcontext)
    {
        if (!imageInfo || !imageInfo->imageView || !imageInfo->imageView->image || !vsgcontext)
            return nullptr;

        static std::map<vsg::ImageInfo*, SceneGraphInspectorTexturePreview> previews;

        auto& preview = previews[imageInfo];
        const auto deviceID = vsgcontext->device()->deviceID;
        const bool needsRebuild =
            !preview.source ||
            preview.source.ref_ptr().get() != imageInfo ||
            preview.deviceID != deviceID ||
            !preview.imageInfo ||
            preview.imageInfo->imageView != imageInfo->imageView ||
            preview.imageInfo->imageLayout != imageInfo->imageLayout ||
            !preview.holder;

        if (needsRebuild)
        {
            auto sampler = imageInfo->sampler;
            if (!sampler)
            {
                sampler = vsg::Sampler::create();
                sampler->magFilter = VK_FILTER_LINEAR;
                sampler->minFilter = VK_FILTER_LINEAR;
                sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                sampler->maxLod = 1.0f;
            }

            preview.source = imageInfo;
            preview.deviceID = deviceID;
            preview.imageInfo = vsg::ImageInfo::create(sampler, imageInfo->imageView, imageInfo->imageLayout);
            preview.holder = SceneGraphInspectorTexturePreview::Holder::create();

            vsg::DescriptorSetLayoutBindings bindings{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} };

            auto dsl = vsg::DescriptorSetLayout::create(bindings);
            auto texture = vsg::DescriptorImage::create(preview.imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            preview.holder->descriptorSet = vsg::DescriptorSet::create(dsl, vsg::Descriptors{ texture });
            vsgcontext->compile(preview.holder);
        }

        return &preview;
    }

    ImVec2 scene_graph_inspector_preview_size(vsg::ImageInfo* imageInfo)
    {
        float width = 0.0f;
        float height = 0.0f;

        if (imageInfo && imageInfo->imageView && imageInfo->imageView->image)
        {
            auto image = imageInfo->imageView->image;
            if (image->data)
            {
                width = static_cast<float>(image->data->width());
                height = static_cast<float>(image->data->height());
            }
            else
            {
                width = static_cast<float>(image->extent.width);
                height = static_cast<float>(image->extent.height);
            }
        }

        if (width <= 0.0f || height <= 0.0f)
            return ImVec2(0.0f, 0.0f);

        const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float maxWidth = std::min(availableWidth, 320.0f);
        const float maxHeight = 240.0f;
        const float scale = std::min(maxWidth / width, maxHeight / height);

        return ImVec2(std::max(1.0f, width * scale), std::max(1.0f, height * scale));
    }

    const char* scene_graph_inspector_descriptor_type(VkDescriptorType descriptorType)
    {
        switch (descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_SAMPLER: return "Sampler";
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "Combined image sampler";
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "Sampled image";
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "Storage image";
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "Uniform texel buffer";
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "Storage texel buffer";
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "Uniform buffer";
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "Storage buffer";
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "Input attachment";
        default: return "Other";
        }
    }

    const char* scene_graph_inspector_vk_format(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
        case VK_FORMAT_R8_SNORM: return "VK_FORMAT_R8_SNORM";
        case VK_FORMAT_R8_USCALED: return "VK_FORMAT_R8_USCALED";
        case VK_FORMAT_R8_SSCALED: return "VK_FORMAT_R8_SSCALED";
        case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
        case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
        case VK_FORMAT_R8_SRGB: return "VK_FORMAT_R8_SRGB";
        case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
        case VK_FORMAT_R8G8_SNORM: return "VK_FORMAT_R8G8_SNORM";
        case VK_FORMAT_R8G8_UINT: return "VK_FORMAT_R8G8_UINT";
        case VK_FORMAT_R8G8_SINT: return "VK_FORMAT_R8G8_SINT";
        case VK_FORMAT_R8G8_SRGB: return "VK_FORMAT_R8G8_SRGB";
        case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
        case VK_FORMAT_R8G8B8_SNORM: return "VK_FORMAT_R8G8B8_SNORM";
        case VK_FORMAT_R8G8B8_UINT: return "VK_FORMAT_R8G8B8_UINT";
        case VK_FORMAT_R8G8B8_SINT: return "VK_FORMAT_R8G8B8_SINT";
        case VK_FORMAT_R8G8B8_SRGB: return "VK_FORMAT_R8G8B8_SRGB";
        case VK_FORMAT_B8G8R8_UNORM: return "VK_FORMAT_B8G8R8_UNORM";
        case VK_FORMAT_B8G8R8_SNORM: return "VK_FORMAT_B8G8R8_SNORM";
        case VK_FORMAT_B8G8R8_UINT: return "VK_FORMAT_B8G8R8_UINT";
        case VK_FORMAT_B8G8R8_SINT: return "VK_FORMAT_B8G8R8_SINT";
        case VK_FORMAT_B8G8R8_SRGB: return "VK_FORMAT_B8G8R8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SNORM: return "VK_FORMAT_R8G8B8A8_SNORM";
        case VK_FORMAT_R8G8B8A8_UINT: return "VK_FORMAT_R8G8B8A8_UINT";
        case VK_FORMAT_R8G8B8A8_SINT: return "VK_FORMAT_R8G8B8A8_SINT";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SNORM: return "VK_FORMAT_B8G8R8A8_SNORM";
        case VK_FORMAT_B8G8R8A8_UINT: return "VK_FORMAT_B8G8R8A8_UINT";
        case VK_FORMAT_B8G8R8A8_SINT: return "VK_FORMAT_B8G8R8A8_SINT";
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
        case VK_FORMAT_R16_UNORM: return "VK_FORMAT_R16_UNORM";
        case VK_FORMAT_R16_SNORM: return "VK_FORMAT_R16_SNORM";
        case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
        case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
        case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
        case VK_FORMAT_R16G16_UNORM: return "VK_FORMAT_R16G16_UNORM";
        case VK_FORMAT_R16G16_SNORM: return "VK_FORMAT_R16G16_SNORM";
        case VK_FORMAT_R16G16_UINT: return "VK_FORMAT_R16G16_UINT";
        case VK_FORMAT_R16G16_SINT: return "VK_FORMAT_R16G16_SINT";
        case VK_FORMAT_R16G16_SFLOAT: return "VK_FORMAT_R16G16_SFLOAT";
        case VK_FORMAT_R16G16B16A16_UNORM: return "VK_FORMAT_R16G16B16A16_UNORM";
        case VK_FORMAT_R16G16B16A16_SNORM: return "VK_FORMAT_R16G16B16A16_SNORM";
        case VK_FORMAT_R16G16B16A16_UINT: return "VK_FORMAT_R16G16B16A16_UINT";
        case VK_FORMAT_R16G16B16A16_SINT: return "VK_FORMAT_R16G16B16A16_SINT";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
        case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
        case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
        case VK_FORMAT_R32G32_UINT: return "VK_FORMAT_R32G32_UINT";
        case VK_FORMAT_R32G32_SINT: return "VK_FORMAT_R32G32_SINT";
        case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_UINT: return "VK_FORMAT_R32G32B32A32_UINT";
        case VK_FORMAT_R32G32B32A32_SINT: return "VK_FORMAT_R32G32B32A32_SINT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_R64_UINT: return "VK_FORMAT_R64_UINT";
        case VK_FORMAT_R64_SINT: return "VK_FORMAT_R64_SINT";
        case VK_FORMAT_R64_SFLOAT: return "VK_FORMAT_R64_SFLOAT";
        case VK_FORMAT_D16_UNORM: return "VK_FORMAT_D16_UNORM";
        case VK_FORMAT_X8_D24_UNORM_PACK32: return "VK_FORMAT_X8_D24_UNORM_PACK32";
        case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
        case VK_FORMAT_D16_UNORM_S8_UINT: return "VK_FORMAT_D16_UNORM_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
        default: return "Unknown VkFormat";
        }
    }

    void scene_graph_inspector_render_image_info(vsg::ImageInfo* imageInfo, std::size_t index, VSGContext vsgcontext)
    {
        ImGui::PushID(static_cast<int>(index));

        std::string label = "Image " + std::to_string(index);

        auto imageView = imageInfo ? imageInfo->imageView : nullptr;
        auto image = imageView ? imageView->image : nullptr;
        auto data = image ? image->data : nullptr;

        if (!scene_graph_inspector_section(label.c_str()))
        {
            ImGui::PopID();
            return;
        }

        if (ImGuiLTable::Begin(label.c_str()))
        {
            ImGuiLTable::Text("ImageInfo:", "%p", imageInfo);
            ImGuiLTable::Text("Sampler:", "%p", imageInfo ? imageInfo->sampler.get() : nullptr);
            ImGuiLTable::Text("ImageView:", "%p", imageView.get());
            ImGuiLTable::Text("Image:", "%p", image.get());
            ImGuiLTable::Text("Layout:", "%d", imageInfo ? static_cast<int>(imageInfo->imageLayout) : 0);

            if (data)
            {
                ImGuiLTable::TextUnformatted("Data type:", data->className());
                ImGuiLTable::Text("Format:", "%s (%d)",
                    scene_graph_inspector_vk_format(data->properties.format),
                    static_cast<int>(data->properties.format));
                ImGuiLTable::Text("Dimensions:", "%u x %u x %u", data->width(), data->height(), data->depth());
            }
            else if (image)
            {
                ImGuiLTable::Text("Extent:", "%u x %u x %u", image->extent.width, image->extent.height, image->extent.depth);
                ImGuiLTable::Text("Format:", "%s (%d)",
                    scene_graph_inspector_vk_format(image->format),
                    static_cast<int>(image->format));
            }

            ImGuiLTable::End();
        }

        auto preview = scene_graph_inspector_get_texture_preview(imageInfo, vsgcontext);
        auto size = scene_graph_inspector_preview_size(imageInfo);
        if (preview && size.x > 0.0f && size.y > 0.0f)
        {
            ImGui::Image(
                scene_graph_inspector_texture_handle(preview->holder->descriptorSet, preview->deviceID),
                size);
        }
        else
        {
            ImGui::TextUnformatted("No previewable image view.");
        }

        ImGui::PopID();
    }

    void scene_graph_inspector_render_descriptor_image(vsg::DescriptorImage* descriptor, VSGContext vsgcontext)
    {
        if (!descriptor)
            return;

        if (!scene_graph_inspector_section("Descriptor Image"))
            return;

        if (ImGuiLTable::Begin("DescriptorImage"))
        {
            ImGuiLTable::Text("Binding:", "%u", descriptor->dstBinding);
            ImGuiLTable::Text("Array element:", "%u", descriptor->dstArrayElement);
            ImGuiLTable::TextUnformatted("Descriptor type:", scene_graph_inspector_descriptor_type(descriptor->descriptorType));
            ImGuiLTable::Text("Images:", "%zu", descriptor->imageInfoList.size());
            ImGuiLTable::End();
        }

        for (std::size_t i = 0; i < descriptor->imageInfoList.size(); ++i)
        {
            scene_graph_inspector_render_image_info(descriptor->imageInfoList[i].get(), i, vsgcontext);
        }
    }

    void scene_graph_inspector_render_metadata(vsg::Object* object)
    {
        if (auto auxiliary = object ? object->getAuxiliary() : nullptr)
        {
            std::scoped_lock lock(auxiliary->getMutex());

            if (!auxiliary->userObjects.empty())
            {
                if (scene_graph_inspector_section("Metadata") &&
                    ImGui::BeginTable("Metadata", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
                {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    for (auto& [key, value] : auxiliary->userObjects)
                    {
                        auto display = scene_graph_inspector_format_object(value.get());
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(key.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped("%s", display.c_str());
                    }

                    ImGui::EndTable();
                }
            }
        }
    }

    void scene_graph_inspector_render_object_properties(vsg::Object* object, VSGContext vsgcontext)
    {
        if (!object)
        {
            ImGui::TextUnformatted("Select an object in the scene graph.");
            return;
        }

        ImGui::PushID(object);

        if (scene_graph_inspector_section("Object") && ImGuiLTable::Begin("SelectedObject"))
        {
            ImGuiLTable::TextUnformatted("Type:", object->className());
            ImGuiLTable::Text("Address:", "%p", object);
            ImGuiLTable::Text("References:", "%u", object->referenceCount());
            ImGuiLTable::Text("Object size:", "%zu bytes", object->sizeofObject());

            if (auto node = object->cast<vsg::Node>())
            {
                std::size_t count = 0;
                if (scene_graph_inspector_child_count(*node, count))
                    ImGuiLTable::Text("Children:", "%zu", count);
            }

            if (auto auxiliary = object->getAuxiliary())
                ImGuiLTable::Text("Metadata:", "%zu", auxiliary->userObjects.size());
            else
                ImGuiLTable::Text("Metadata:", "%d", 0);

            ImGuiLTable::End();
        }

        if (auto descriptor = object->cast<vsg::DescriptorImage>())
            scene_graph_inspector_render_descriptor_image(descriptor, vsgcontext);

        auto node = object->cast<vsg::Node>();
        if (!node)
        {
            scene_graph_inspector_render_metadata(object);
            ImGui::PopID();
            return;
        }

        if (auto group = node->cast<vsg::Switch>())
        {
            std::size_t active = 0;
            for (auto& child : group->children)
            {
                if (child.mask != vsg::MASK_OFF)
                    ++active;
            }

            if (scene_graph_inspector_section("Switch") && ImGuiLTable::Begin("Switch"))
            {
                ImGuiLTable::Text("Active children:", "%zu", active);
                ImGuiLTable::End();
            }
        }

        if (auto transform = node->cast<vsg::MatrixTransform>())
        {
            if (scene_graph_inspector_section("Matrix") &&
                ImGui::BeginTable("Matrix", 4, ImGuiTableFlags_SizingStretchProp))
            {
                for (int row = 0; row < 4; ++row)
                {
                    ImGui::TableNextRow();
                    for (int col = 0; col < 4; ++col)
                    {
                        ImGui::TableNextColumn();
                        ImGui::Text("%.3lf", transform->matrix(col, row));
                    }
                }
                ImGui::EndTable();
            }
        }

        if (auto group = node->cast<vsg::CullGroup>())
            scene_graph_inspector_render_sphere("Cull bound", group->bound);

        if (auto lod = node->cast<vsg::LOD>())
            scene_graph_inspector_render_sphere("LOD bound", lod->bound);

        if (auto lod = node->cast<vsg::PagedLOD>())
        {
            scene_graph_inspector_render_sphere("Paged LOD bound", lod->bound);

            if (scene_graph_inspector_section("Paged LOD") && ImGuiLTable::Begin("PagedLOD"))
            {
                ImGuiLTable::TextUnformatted("Filename:", lod->filename.string().c_str());
                ImGuiLTable::Text("Priority:", "%.6lf", lod->priority.load());
                ImGuiLTable::Text("Requests:", "%u", lod->requestCount.load());
                ImGuiLTable::TextUnformatted("Status:", scene_graph_inspector_request_status(lod->requestStatus.load()));
                ImGuiLTable::End();
            }
        }

        scene_graph_inspector_render_metadata(object);
        ImGui::PopID();
    }

    struct SceneGraphInspectorVisitor : public vsg::Inherit<vsg::Visitor, SceneGraphInspectorVisitor>
    {
        using vsg::Visitor::apply;

        SceneGraphInspectorVisitor(vsg::observer_ptr<vsg::Object>& in_selectedObject) :
            selectedObject(in_selectedObject)
        {
        }

        vsg::observer_ptr<vsg::Object>& selectedObject;
        int depth = 0;

        void apply(vsg::Object& object) override
        {
            render(object);
        }

        void apply(vsg::Node& node) override
        {
            render(node);
        }

        void apply(vsg::Group& group) override
        {
            render(group);
        }

        void apply(vsg::StateGroup& group) override
        {
            render(group);
        }

        void apply(vsg::QuadGroup& group) override
        {
            render(group);
        }

        void apply(vsg::Switch& group) override
        {
            render(group);
        }

        void apply(vsg::CullGroup& group) override
        {
            render(group);
        }

        void apply(vsg::LOD& lod) override
        {
            render(lod);
        }

        void apply(vsg::PagedLOD& lod) override
        {
            render(lod);
        }

        void apply(vsg::MatrixTransform& transform) override
        {
            render(transform);
        }

        void apply(vsg::BindDescriptorSet& bind) override
        {
            render(bind);
        }

        void apply(vsg::BindDescriptorSets& bind) override
        {
            render(bind);
        }

        void apply(vsg::DescriptorSet& descriptorSet) override
        {
            render(descriptorSet);
        }

        void apply(vsg::DescriptorImage& descriptor) override
        {
            render(descriptor);
        }

        void render(vsg::Object& object)
        {
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            bool root = depth == 0;

            if (root)
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            if (selectedObject == &object)
                flags |= ImGuiTreeNodeFlags_Selected;

            bool open = root ?
                ImGui::TreeNodeEx(&object, flags, "app.scene (%s)", object.className()) :
                ImGui::TreeNodeEx(&object, flags, "%s", object.className());

            if (ImGui::IsItemClicked())
                selectedObject = &object;

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
        static vsg::observer_ptr<vsg::Object> selectedObject;

        if (!selectedObject)
            selectedObject = app.scene;

        if (ImGui::BeginTable("SceneGraphInspector", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Scene graph", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 0.45f);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (ImGui::BeginChild("SceneGraphTree", ImVec2(0, 420), true))
            {
                SceneGraphInspectorVisitor inspector(selectedObject);
                app.scene->accept(inspector);
            }
            ImGui::EndChild();

            ImGui::TableNextColumn();
            if (ImGui::BeginChild("SceneGraphProperties", ImVec2(0, 420), true))
            {
                auto object = selectedObject.ref_ptr();
                scene_graph_inspector_render_object_properties(object.get(), app.vsgcontext);
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::TextUnformatted("No scene graph.");
    }
};
