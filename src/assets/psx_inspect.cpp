#include "psx_asset.hpp"
#include "psx_collision.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: opentony_psx_inspect FILE.psx\n";
        return 2;
    }
    try {
        const opentony::assets::PsxArchive archive =
            opentony::assets::PsxArchive::load(argv[1]);
        std::size_t faces = 0;
        for (const opentony::assets::PsxModel& model : archive.models()) {
            faces += model.faces.size();
        }
        const opentony::assets::PsxCollisionWorld collision =
            opentony::assets::PsxCollisionWorld::build(archive);
        std::cout << "version=" << archive.version()
                  << " objects=" << archive.objects().size()
                  << " models=" << archive.models().size()
                  << " model_names=" << archive.model_names().size()
                  << " faces=" << faces
                  << " tags=" << archive.tags().size()
                  << " blockmaps=" << archive.blockmaps().size()
                  << " collision_objects=" << collision.referenced_object_count()
                  << " collision_faces=" << collision.face_count()
                  << " textures=" << archive.textures().size()
                  << " palettes4=" << archive.palettes4().size()
                  << " palettes8=" << archive.palettes8().size() << '\n';
    } catch (const opentony::assets::PsxFormatError& error) {
        std::cerr << "PSX error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
