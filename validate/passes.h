#ifndef TOZ3_VALIDATE_PASSES_H_
#define TOZ3_VALIDATE_PASSES_H_

#include <filesystem>
#include <functional>
#include <vector>

namespace P4::ToZ3 {

enum class PassSelection { Checkpoints, All };

std::vector<std::filesystem::path> generatePassList(const std::filesystem::path &p4_file,
                                                    const std::filesystem::path &dump_dir,
                                                    const std::filesystem::path &compiler_bin,
                                                    PassSelection selection = PassSelection::All);

// Check phase boundaries first; use individual passes to diagnose a failed coarse check.
int validatePasses(
    const std::filesystem::path &p4_file, const std::filesystem::path &dump_dir,
    const std::filesystem::path &compiler_bin,
    const std::function<int(const std::vector<std::filesystem::path> &, bool)> &compare,
    bool all_passes = false);

}  // namespace P4::ToZ3

#endif  // TOZ3_VALIDATE_PASSES_H_
