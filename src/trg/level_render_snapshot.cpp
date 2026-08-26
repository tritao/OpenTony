#include "level_render_snapshot.hpp"

namespace opentony::trg {

LevelRenderSnapshot LevelRenderSnapshot::build(
    const LevelSceneRegistry& scene,
    const assets::PsxArchive& archive) {
    LevelRenderSnapshot result;
    result.entities_.reserve(scene.entities().size());

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
        };

        if (source.psx_object_index != CommandPointRuntime::npos) {
            if (source.psx_object_index >= archive.objects().size()) {
                throw assets::PsxFormatError(
                    "scene render entity references a missing PSX object");
            }
            if (source.model_index >= archive.models().size()) {
                throw assets::PsxFormatError(
                    "scene render entity references a missing PSX model");
            }
            const assets::PsxModel& model = archive.models()[source.model_index];
            entity.first_face = result.faces_.size();
            for (std::size_t face_index = 0;
                 face_index < model.faces.size();
                 ++face_index) {
                const assets::PsxFace& source_face = model.faces[face_index];
                LevelRenderFaceSnapshot face{
                    source.entity,
                    source.psx_object_index,
                    source.model_index,
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
        }
        result.entities_.push_back(entity);
    }
    return result;
}

} // namespace opentony::trg
