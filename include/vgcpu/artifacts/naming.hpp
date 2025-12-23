#pragma once
#include <string>

namespace vgcpu::artifacts {

/**
 * @brief Generates a standardized path for an artifact.
 */
std::string generate_artifact_path(const std::string& scene_name, const std::string& backend_name,
                                   const std::string& suffix = ".png");

}  // namespace vgcpu::artifacts
