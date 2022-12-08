#include "options.h"

ValidateOptions::ValidateOptions() {
    registerOption(
        "--dump-dir", "folder",
        [this](const char* arg) {
            dump_dir = arg;
            return true;
        },
        "The output folder where all passes are dumped.\n");
    registerOption(
        "--compiler-bin", "file",
        [this](const char* arg) {
            compiler_bin = arg;
            return true;
        },
        "Specifies the binary to compile a p4 file.");
    registerOption(
        "--allow-undefined", nullptr,
        [this](const char* /*arg*/) {
            undefined_is_ok = true;
            return true;
        },
        "Toggle to tolerate undefined behavior in comparison.");
}
