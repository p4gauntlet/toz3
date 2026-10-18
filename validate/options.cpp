#include "options.h"

namespace P4::ToZ3 {

ValidateOptions::ValidateOptions() {
    registerOption(
        "--all-passes", nullptr,
        [this](const char *) {
            all_passes = true;
            return true;
        },
        "Check every compiler pass directly instead of checking phase boundaries first.");
    registerOption(
        "--dump-dir", "folder",
        [this](const char *arg) {
            dump_dir = cstring(arg);
            return true;
        },
        "The output folder where all passes are dumped.\n");
    registerOption(
        "--compiler-bin", "file",
        [this](const char *arg) {
            compiler_bin = cstring(arg);
            return true;
        },
        "Specifies the binary to compile a p4 file.");
    registerOption(
        "--allow-undefined", nullptr,
        [this](const char * /*arg*/) {
            undefined_is_ok = true;
            return true;
        },
        "Toggle to tolerate undefined behavior in comparison.");
}

}  // namespace P4::ToZ3
