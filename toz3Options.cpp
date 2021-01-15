#include "toz3Options.h"

namespace P4TOZ3 {

toz3Options::toz3Options() {
    registerOption(
        "--output", "file",
        [this](const char *arg) {
            o_file = arg;
            return true;
        },
        "The translated Z3 file.");
}

} // namespace P4TOZ3
