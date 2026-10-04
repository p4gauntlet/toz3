#include "passes.h"

#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>

#include <iomanip>
#include <sstream>
#include <string>

#include "toz3/common/exceptions.h"
#include "toz3/common/util.h"

namespace P4::ToZ3 {
namespace {

// popen invokes a shell, so each path must remain a single literal argument.
std::string quotePath(const std::filesystem::path &path) {
    return absl::StrCat("'", absl::StrReplaceAll(path.string(), {{"'", "'\\''"}}), "'");
}

}  // namespace

std::vector<std::filesystem::path> generatePassList(const std::filesystem::path &p4_file,
                                                    const std::filesystem::path &dump_dir,
                                                    const std::filesystem::path &compiler_bin) {
    const auto baseCmd =
        absl::StrCat(quotePath(compiler_bin), " --top4 FrontEnd,MidEnd,PassManager --dump ",
                     quotePath(dump_dir));
    // Capture the compiler's status directly; a shell pipeline would report only
    // the status of its final filter and could hide compilation failures.
    const auto passListCmd = absl::StrCat(baseCmd, " --Wdisable -v ", quotePath(p4_file), " 2>&1");
    std::stringstream output;
    if (exec(passListCmd.c_str(), output) != 0) {
        throw CompilerExecutionError(absl::StrCat("Failed to list compiler passes. Command:\n",
                                                  passListCmd, "\n", output.str()));
    }

    std::vector<std::filesystem::path> passList;
    std::string line;
    const std::string marker = "Writing program to ";
    while (std::getline(output, line)) {
        const auto pos = line.find(marker);
        if (pos == std::string::npos ||
            (!absl::StrContains(line, "FrontEnd") && !absl::StrContains(line, "MidEnd") &&
             !absl::StrContains(line, "PassManager"))) {
            continue;
        }
        // Filesystem paths are logged with std::quoted, including escaped quotes
        // and backslashes. Decode them before using them as filenames.
        std::istringstream pathStream(line.substr(pos + marker.size()));
        std::string pass;
        if (!(pathStream >> std::quoted(pass)) || pass.empty()) {
            throw CompilerExecutionError(absl::StrCat("Malformed pass filename: ", line));
        }
        passList.emplace_back(pass);
    }

    if (passList.size() < 2) {
        return passList;
    }
    // Write the actual programs without -v, which adds diagnostics to the dumps.
    const auto dumpCmd = absl::StrCat(baseCmd, " ", quotePath(p4_file), " 2>&1");
    std::stringstream dumpOutput;
    if (exec(dumpCmd.c_str(), dumpOutput) != 0) {
        throw CompilerExecutionError(absl::StrCat("Failed to dump compiler passes. Command:\n",
                                                  dumpCmd, "\n", dumpOutput.str()));
    }

    std::vector<std::filesystem::path> prunedPassList{passList.front()};
    for (std::size_t idx = 1; idx < passList.size(); ++idx) {
        const auto &passAfter = passList[idx];
        if (compare_files(prunedPassList.back(), passAfter)) {
            std::filesystem::remove(passAfter);
        } else {
            prunedPassList.push_back(passAfter);
        }
    }
    return prunedPassList;
}

}  // namespace P4::ToZ3
