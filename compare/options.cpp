#include "options.h"

namespace TOZ3 {

CompareOptions::CompareOptions() {
    registerOption(
        "--allow-undefined", nullptr,
        [this](const char *) {
            undefined_is_ok = true;
            return true;
        },
        "Toggle to tolerate undefined behavior in comparison.");
}
}  // namespace TOZ3
