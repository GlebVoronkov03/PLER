#pragma once
#include "pler/types.hpp"
#include <string>

namespace pler {

/** Load key: value YAML subset into options. Missing file is a no-op. */
bool load_options_yaml(const std::string& path, PlerOptions& opt);

/**
 * Search order: explicit path, PLER_CONFIG env, cwd/config/pler.yaml,
 * next to executable, repo-relative config/pler.yaml.
 */
std::string find_default_config_path();

/** Apply discovered config if present. */
void apply_default_config(PlerOptions& opt);

}  // namespace pler
