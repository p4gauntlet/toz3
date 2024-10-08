#include "options.h"

namespace P4::ToZ3 {

CompareOptions::CompareOptions() {
    registerOption(
        "--allow-undefined", nullptr,
        [this](const char * /*val*/) {
            undefined_is_ok = true;
            return true;
        },
        "Toggle to tolerate undefined behavior in comparison.");
}
}  // namespace P4::ToZ3
