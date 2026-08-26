#include "level_render_snapshot.hpp"

#include <algorithm>

namespace opentony::trg {

namespace {

void append_model_faces(
    std::vector<LevelRenderFaceSnapshot>& faces,
    LevelRenderEntitySnapshot& entity,
    const assets::PsxRuntimeEnvironment& runtime,
    std::size_t model_index,
    std::size_t object_index) {
    const assets::PsxArchive& archive = runtime.source_archive();
    if (model_index >= archive.models().size()) {
        throw assets::PsxFormatError(
            "render entity references a missing PSX model");
    }

    const assets::PsxModel& model = archive.models()[model_index];
    entity.first_face = faces.size();
    for (std::size_t face_index = 0;
         face_index < model.faces.size();
         ++face_index) {
        const assets::PsxFace& source_face = model.faces[face_index];
        LevelRenderFaceSnapshot face{
            entity.entity,
            object_index,
            model_index,
            face_index,
            source_face.flags,
            source_face.surface_flags,
            source_face.raw_collision_word,
            source_face.texture_index,
            CommandPointRuntime::npos,
            0,
            0,
            0,
            false,
            source_face.has_texture,
            static_cast<std::uint8_t>(
                (source_face.flags & 0x0010U) != 0 ? 3 : 4),
            source_face.uv_count,
        };
        for (std::size_t corner = 0;
             corner < source_face.vertex_indices.size();
             ++corner) {
            const std::size_t vertex_index = source_face.vertex_indices[corner];
            if (vertex_index >= model.vertices.size()) {
                throw assets::PsxFormatError(
                    "render face references a missing vertex");
            }
            face.local_vertices[corner] = model.vertices[vertex_index];
            face.uv[corner] = source_face.uv[corner];
        }
        if (source_face.has_texture) {
            const auto material_index =
                runtime.materials().material_index_for_texture(
                    source_face.texture_index);
            if (!material_index.has_value()) {
                throw assets::PsxFormatError(
                    "render face references a missing runtime material");
            }
            face.runtime_material_index = *material_index;
            face.material_checksum = runtime.materials()
                .record(*material_index)
                .checksum();
            const auto dimensions = runtime.texture_dimensions_for_material(
                *material_index,
                face.material_checksum);
            if (dimensions.has_value()) {
                face.texture_width = dimensions->at(0);
                face.texture_height = dimensions->at(1);
                face.has_texture_dimensions = true;
            }
        }
        if (source_face.normal_index >= model.normals.size()) {
            throw assets::PsxFormatError(
                "render face references a missing normal");
        }
        face.normal = model.normals[source_face.normal_index];
        faces.push_back(face);
    }
    entity.face_count = faces.size() - entity.first_face;
}

const assets::PsxRuntimeEnvironment* runtime_for_powerup(
    const PowerupRuntimeRecord& powerup,
    const assets::PsxRuntimeEnvironment* items_runtime,
    const assets::PsxRuntimeEnvironment* medals_runtime) noexcept {
    if (powerup.resource() == "items") {
        return items_runtime;
    }
    if (powerup.resource() == "skmedals") {
        return medals_runtime;
    }
    return nullptr;
}

} // namespace

LevelRenderSnapshot LevelRenderSnapshot::build(
    const LevelSceneRegistry& scene,
    const assets::PsxArchive& archive) {
    const assets::PsxRuntimeEnvironment runtime =
        assets::PsxRuntimeEnvironment::build(archive);
    return build(scene, runtime);
}

LevelRenderSnapshot LevelRenderSnapshot::build(
    const LevelSceneRegistry& scene,
    const assets::PsxArchive& archive,
    const assets::PsxAssetCatalog* catalog) {
    const assets::PsxRuntimeEnvironment runtime =
        assets::PsxRuntimeEnvironment::build(archive);
    LevelRenderSnapshot result = build(scene, runtime);
    if (catalog == nullptr) {
        return result;
    }
    const std::size_t count = std::min(
        scene.entities().size(), result.entities_.size());
    for (std::size_t index = 0; index < count; ++index) {
        const LevelSceneEntity& source = scene.entities()[index];
        if (source.kind != LevelSceneEntityKind::Pickup
            || !source.pickup_model_resolved
            || source.pickup_resource.empty()) {
            continue;
        }
        LevelRenderEntitySnapshot& entity = result.entities_[index];
        const assets::PsxArchive& pickup_archive =
            catalog->load(source.pickup_resource);
        const assets::PsxRuntimeEnvironment pickup_runtime =
            assets::PsxRuntimeEnvironment::build(pickup_archive);
        entity.model_resource = source.pickup_resource;
        entity.resource_model_index = source.pickup_model_index;
        entity.model_index = source.pickup_model_index;
        entity.model_name = source.pickup_model_checksum;
        append_model_faces(
            result.faces_, entity, pickup_runtime,
            source.pickup_model_index, CommandPointRuntime::npos);
    }
    return result;
}

LevelRenderSnapshot LevelRenderSnapshot::build(
    const LevelSceneRegistry& scene,
    const assets::PsxRuntimeEnvironment& runtime) {
    return build(scene, runtime, nullptr, nullptr, nullptr);
}

LevelRenderSnapshot LevelRenderSnapshot::build(
    const LevelSceneRegistry& scene,
    const assets::PsxRuntimeEnvironment& runtime,
    const PowerupRuntimeList* powerups,
    const assets::PsxRuntimeEnvironment* items_runtime,
    const assets::PsxRuntimeEnvironment* medals_runtime) {
    const assets::PsxArchive& archive = runtime.source_archive();
    LevelRenderSnapshot result;
    result.entities_.reserve(
        scene.entities().size() + (powerups == nullptr ? 0 : powerups->records().size()));

    const auto append_faces = [&result](
        LevelRenderEntitySnapshot& entity,
        const assets::PsxArchive& source_archive,
        std::size_t model_index) {
        if (model_index >= source_archive.models().size()) {
            throw assets::PsxFormatError(
                "scene render entity references a missing PSX model");
        }
        const assets::PsxModel& model = source_archive.models()[model_index];
        entity.first_face = result.faces_.size();
        for (std::size_t face_index = 0;
             face_index < model.faces.size();
             ++face_index) {
            const assets::PsxFace& source_face = model.faces[face_index];
            LevelRenderFaceSnapshot face{
                entity.entity,
                entity.psx_object_index,
                model_index,
                face_index,
                source_face.flags,
                source_face.surface_flags,
                source_face.raw_collision_word,
                source_face.texture_index,
                source_face.has_texture,
                static_cast<std::uint8_t>(
                    (source_face.flags & 0x0010U) != 0 ? 3 : 4),
                source_face.uv_count,
            };
            for (std::size_t corner = 0;
                 corner < source_face.vertex_indices.size();
                 ++corner) {
                const std::size_t vertex_index = source_face.vertex_indices[corner];
                if (vertex_index >= model.vertices.size()) {
                    throw assets::PsxFormatError(
                        "scene render face references a missing vertex");
                }
                face.local_vertices[corner] = model.vertices[vertex_index];
                face.uv[corner] = source_face.uv[corner];
            }
            if (source_face.normal_index >= model.normals.size()) {
                throw assets::PsxFormatError(
                    "scene render face references a missing normal");
            }
            face.normal = model.normals[source_face.normal_index];
            result.faces_.push_back(face);
        }
        entity.face_count = result.faces_.size() - entity.first_face;
    };

    for (const LevelSceneEntity& source : scene.entities()) {
        LevelRenderEntitySnapshot entity{
            source.entity,
            source.kind,
            source.source_node,
            source.psx_object_index,
            source.model_index,
            source.model_name,
            source.position,
            source.orientation,
            source.has_orientation,
            source.asset_flags,
            source.gameplay_flags,
            source.active,
            source.suspended,
            source.alive,
            source.killed,
            source.visible_commanded,
            source.pickup_visual_state_d1,
            source.pickup_motion_state_d2,
            source.pickup_motion_substate_d3,
            source.pickup_motion_words_14_18,
            source.pickup_motion_words_70_74,
            source.has_pickup_motion_inputs,
            source.pickup_timer_f0,
            source.pickup_phase_ea,
            source.pickup_phase_ec,
            source.pickup_global_fade_flags,
            source.has_pickup_lifecycle_inputs,
            source.pickup_glow_present,
            source.pickup_update_calls,
            CommandPointRuntime::npos,
            0,
            {},
            CommandPointRuntime::npos,
        };

        if (source.psx_object_index != CommandPointRuntime::npos) {
            if (source.psx_object_index >= archive.objects().size()) {
                throw assets::PsxFormatError(
                    "scene render entity references a missing PSX object");
            }
            append_model_faces(
                result.faces_,
                entity,
                runtime,
                source.model_index,
                source.psx_object_index);
        } else if (powerups != nullptr
                   && source.kind == LevelSceneEntityKind::Pickup
                   && source.source_node != CommandPointRuntime::npos) {
            const PowerupRuntimeRecord* powerup =
                powerups->record_for_node(source.source_node);
            const assets::PsxRuntimeEnvironment* powerup_runtime =
                powerup == nullptr
                ? nullptr
                : runtime_for_powerup(
                    *powerup, items_runtime, medals_runtime);
            if (powerup != nullptr) {
                entity.model_name = powerup->model_name_checksum();
                entity.object_position = powerup->position();
                if (powerup->model_index().has_value()) {
                    entity.model_index = *powerup->model_index();
                }
            }
            if (powerup_runtime != nullptr && entity.model_index != CommandPointRuntime::npos) {
                append_model_faces(
                    result.faces_,
                    entity,
                    *powerup_runtime,
                    entity.model_index,
                    CommandPointRuntime::npos);
            }
        }
        result.entities_.push_back(entity);
    }
    return result;
}

} // namespace opentony::trg
