#include "pre_catalog.hpp"

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

std::string PreAssetCatalog::key_for(std::string_view name) {
    const std::filesystem::path path{std::string(name)};
    std::string key = path.stem().string();
    if (key.empty()) {
        key = std::string(name);
    }
    return upper(std::move(key));
}

PreAssetCatalog PreAssetCatalog::scan(const std::string& root) {
    PreAssetCatalog catalog;
    const std::filesystem::path directory(root);
    if (!std::filesystem::is_directory(directory)) {
        throw PreFormatError("PRE asset root is not a directory: " + root);
    }
    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (const std::filesystem::directory_entry& entry
         : std::filesystem::recursive_directory_iterator(directory, options)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string extension = entry.path().extension().string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::toupper(character));
            });
        if (extension != ".PRE") {
            continue;
        }
        const std::string key = key_for(entry.path().filename().string());
        const std::string path = entry.path().string();
        const auto [found, inserted] = catalog.paths_.emplace(key, path);
        if (!inserted && found->second != path) {
            throw PreFormatError("duplicate PRE asset stem: " + key);
        }
    }
    return catalog;
}

bool PreAssetCatalog::contains(std::string_view name) const {
    return paths_.find(key_for(name)) != paths_.end();
}

const std::string* PreAssetCatalog::path_for(std::string_view name) const {
    const auto found = paths_.find(key_for(name));
    return found == paths_.end() ? nullptr : &found->second;
}

const PreArchive& PreAssetCatalog::load(std::string_view name) const {
    const std::string key = key_for(name);
    const auto cached = archives_.find(key);
    if (cached != archives_.end()) {
        return *cached->second;
    }
    const auto path = paths_.find(key);
    if (path == paths_.end()) {
        throw PreFormatError("PRE asset is not indexed: " + key);
    }
    auto archive = std::make_shared<PreArchive>(PreArchive::load(path->second));
    const PreArchive& result = *archive;
    archives_.emplace(key, std::move(archive));
    return result;
}

} // namespace opentony::assets
