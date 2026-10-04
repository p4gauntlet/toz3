#ifndef TOZ3_VALIDATE_PASSES_H_
#define TOZ3_VALIDATE_PASSES_H_

#include <filesystem>
#include <vector>

namespace P4::ToZ3 {

std::vector<std::filesystem::path> generatePassList(const std::filesystem::path &p4_file,
                                                    const std::filesystem::path &dump_dir,
                                                    const std::filesystem::path &compiler_bin);

}  // namespace P4::ToZ3

#endif  // TOZ3_VALIDATE_PASSES_H_
