#include "vgcpu/artifacts/naming.hpp"

#include <cctype>
#include <string>

namespace vgcpu::artifacts {

static std::string sanitize(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (std::isalnum(c)) {
            result.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '-' || c == '_') {
            result.push_back(c);
        } else {
            result.push_back('_');
        }
    }
    return result;
}

std::string generate_artifact_path(const std::string& scene_name, const std::string& backend_name,
                                   const std::string& suffix) {
    std::string clean_scene = sanitize(scene_name);
    std::string clean_backend = sanitize(backend_name);

    // Format: {scene}_{backend}{suffix}
    // Example: "tiger_main_skia.png"
    return clean_scene + "_" + clean_backend + suffix;
}

}  // namespace vgcpu::artifacts
