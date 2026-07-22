#ifndef GUI_APP_STARTUP_ENVIRONMENT_H
#define GUI_APP_STARTUP_ENVIRONMENT_H

#include <map>
#include <string>

namespace gui::startup_environment {

enum class Platform {
    Linux,
    MacOS,
    Other,
};

using Environment = std::map<std::string, std::string>;

Platform current_platform() noexcept;

Environment compute_default_updates(
    Platform platform,
    const Environment& current_environment);

void apply_defaults_for_current_platform();

} // namespace gui::startup_environment

#endif // GUI_APP_STARTUP_ENVIRONMENT_H
