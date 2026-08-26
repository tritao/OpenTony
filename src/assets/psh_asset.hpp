#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class PshFormatError final : public std::runtime_error {
public:
    explicit PshFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

struct PshPart {
    // The token between <base>PART_ and the second underscore. Retail's
    // appearance merge compares the trailing part label, not the costume/
    // model prefix (for example BURNQ_PELVIS and HAWK_PELVIS both expose
    // PELVIS here).
    std::string name;
    std::string model_name;
    std::uint32_t index{};
    std::size_t line_offset{};
};

struct PshPartMatch {
    std::uint32_t animation_index{};
    std::uint32_t model_index{};
    std::string name;
};

// Native value-owned form of the runtime PSH parser output. Retail keeps
// pointers into the loaded text buffer; owning each discovered part name here
// preserves the same count/order/name join without exposing dangling pointers.
class PshManifest final {
public:
    static PshManifest load(const std::string& path);
    static PshManifest parse(
        std::vector<std::byte> bytes,
        std::string source = {},
        std::string base_name = {});

    [[nodiscard]] const std::string& base_name() const noexcept { return base_name_; }
    [[nodiscard]] const std::vector<PshPart>& parts() const noexcept { return parts_; }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::string base_name_;
    std::vector<PshPart> parts_;
};

// Reproduce the name-based merge at 0x00480d90/0x00480cd0. The result is
// intentionally sparse: an animation part with no model counterpart is not
// fabricated into a positional match.
[[nodiscard]] std::vector<PshPartMatch> match_psh_parts(
    const PshManifest& animation,
    const PshManifest& model);

} // namespace opentony::assets
