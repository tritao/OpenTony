#include "psx_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::string upper(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return value;
}

} // namespace

std::string PsxAssetCatalog::key_for(std::string_view name) {
    const std::filesystem::path path{std::string(name)};
    std::string key = path.stem().string();
    if (key.empty()) {
        key = std::string(name);
    }
    return upper(std::move(key));
}

PsxAssetCatalog PsxAssetCatalog::scan(const std::string& root) {
    PsxAssetCatalog catalog;
    const std::filesystem::path directory(root);
    if (!std::filesystem::is_directory(directory)) {
        throw PsxFormatError("PSX asset root is not a directory: " + root);
    }
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (const std::filesystem::directory_entry& entry
         : std::filesystem::recursive_directory_iterator(directory, options)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (upper(entry.path().extension().string()) != ".PSX") {
            continue;
        }
        const std::string key = key_for(entry.path().filename().string());
        const std::string path = entry.path().string();
        const auto [found, inserted] = catalog.paths_.emplace(key, path);
        if (!inserted && found->second != path) {
            throw PsxFormatError("duplicate PSX asset stem: " + key);
        }
    }
    return catalog;
}

bool PsxAssetCatalog::contains(std::string_view name) const {
    return paths_.find(key_for(name)) != paths_.end();
}

const std::string* PsxAssetCatalog::path_for(std::string_view name) const {
    const auto found = paths_.find(key_for(name));
    return found == paths_.end() ? nullptr : &found->second;
}

const PsxArchive& PsxAssetCatalog::load(std::string_view name) const {
    const std::string key = key_for(name);
    const auto cached = archives_.find(key);
    if (cached != archives_.end()) {
        return *cached->second;
    }
    const auto path = paths_.find(key);
    if (path == paths_.end()) {
        throw PsxFormatError("PSX asset is not indexed: " + key);
    }
    auto archive = std::make_shared<PsxArchive>(PsxArchive::load(path->second));
    const PsxArchive& result = *archive;
    archives_.emplace(key, std::move(archive));
    return result;
}

} // namespace opentony::assets
