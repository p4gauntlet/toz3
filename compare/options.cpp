#include "options.h"

namespace TOZ3_V2 {

CompareOptions::CompareOptions() {
    registerOption(
        "--compare-with", "file",
        [this](const char *arg) {
            compare_file = arg;
            return true;
        },
        "Compare the input to this file.");
}
} // namespace TOZ3_V2
