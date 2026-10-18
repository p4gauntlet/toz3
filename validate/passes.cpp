#include "passes.h"

#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>

#include <iomanip>
#include <iostream>
#include <set>
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

// P4C logs passes as Manager_number_Name. Select by Manager_number_ so the
// human-readable pass name never needs regex escaping.
std::string passPrefix(const std::string &name) {
    const auto managerEnd = name.find('_');
    const auto manager = name.substr(0, managerEnd);
    if (manager != "FrontEnd" && manager != "MidEnd" && manager != "PassManager") {
        return {};
    }
    if (managerEnd == std::string::npos) {
        return {};
    }
    const auto numberEnd = name.find('_', managerEnd + 1);
    if (numberEnd == std::string::npos || numberEnd + 1 == name.size()) {
        return {};
    }
    const auto number = name.substr(managerEnd + 1, numberEnd - managerEnd - 1);
    if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos) {
        return {};
    }
    return name.substr(0, numberEnd + 1);
}

std::set<std::string> selectCheckpoints(const std::vector<std::string> &passes) {
    std::set<std::string> selected{passPrefix(passes.front()), passPrefix(passes.back())};
    std::string frontend;
    std::string midend;
    for (const auto &pass : passes) {
        if (pass.starts_with("FrontEnd_")) {
            frontend = pass;
        }
        if (pass.starts_with("MidEnd_")) {
            midend = pass;
        }
    }
    if (!frontend.empty()) {
        selected.insert(passPrefix(frontend));
    }
    if (!midend.empty()) {
        selected.insert(passPrefix(midend));
    }
    return selected;
}

std::string checkpointFilter(const std::set<std::string> &selected) {
    std::string filter;
    for (const auto &prefix : selected) {
        if (!filter.empty()) {
            filter += ",";
        }
        filter += "^" + prefix;
    }
    return filter;
}
}  // namespace

std::vector<std::filesystem::path> generatePassList(const std::filesystem::path &p4_file,
                                                    const std::filesystem::path &dump_dir,
                                                    const std::filesystem::path &compiler_bin,
                                                    PassSelection selection) {
    // Capture the compiler's status directly; a shell pipeline would report only
    // the status of its final filter and could hide compilation failures.
    // Discover the actual pipeline without writing verbose (show-IR) dumps.
    const auto passListCmd =
        absl::StrCat(quotePath(compiler_bin), " --Wdisable -v ", quotePath(p4_file), " 2>&1");
    std::stringstream output;
    if (exec(passListCmd.c_str(), output) != 0) {
        throw CompilerExecutionError(absl::StrCat("Failed to list compiler passes. Command:\n",
                                                  passListCmd, "\n", output.str()));
    }

    std::vector<std::string> passes;
    std::string line;
    while (std::getline(output, line)) {
        if (!passPrefix(line).empty()) {
            passes.push_back(line);
        }
    }
    if (passes.empty()) {
        return {};
    }

    const auto selected = selection == PassSelection::Checkpoints ? selectCheckpoints(passes)
                                                                  : std::set<std::string>{};
    const auto filter = selection == PassSelection::Checkpoints
                            ? checkpointFilter(selected)
                            : "^FrontEnd_,^MidEnd_,^PassManager_";
    std::filesystem::create_directories(dump_dir);
    // Write the actual programs without -v, which adds diagnostics to the dumps.
    const auto dumpCmd =
        absl::StrCat(quotePath(compiler_bin), " --top4 ", quotePath(filter), " --dump ",
                     quotePath(dump_dir), " ", quotePath(p4_file), " 2>&1");
    std::stringstream dumpOutput;
    if (exec(dumpCmd.c_str(), dumpOutput) != 0) {
        throw CompilerExecutionError(absl::StrCat("Failed to dump compiler passes. Command:\n",
                                                  dumpCmd, "\n", dumpOutput.str()));
    }

    std::vector<std::filesystem::path> prunedPassList;
    unsigned sequence = 0;
    for (const auto &pass : passes) {
        if (selection == PassSelection::Checkpoints && !selected.contains(passPrefix(pass))) {
            continue;
        }
        // P4C numbers only the dumps selected by --top4, in execution order.
        std::ostringstream filename;
        filename << p4_file.stem().string() << '-' << std::setfill('0') << std::setw(4)
                 << ++sequence << '-' << pass << p4_file.extension().string();
        const auto path = dump_dir / filename.str();
        if (!std::filesystem::is_regular_file(path)) {
            throw CompilerExecutionError("Missing compiler dump: " + path.string());
        }
        if (!prunedPassList.empty() && compare_files(prunedPassList.back(), path)) {
            std::filesystem::remove(path);
        } else {
            prunedPassList.push_back(path);
        }
    }
    return prunedPassList;
}

int validatePasses(
    const std::filesystem::path &p4_file, const std::filesystem::path &dump_dir,
    const std::filesystem::path &compiler_bin,
    const std::function<int(const std::vector<std::filesystem::path> &, bool)> &compare,
    bool all_passes) {
    if (!all_passes) {
        const auto checkpoints = generatePassList(p4_file, dump_dir / "checkpoints", compiler_bin,
                                                  PassSelection::Checkpoints);
        if (checkpoints.size() < 2) {
            return EXIT_SKIPPED;
        }
        const auto result = compare(checkpoints, false);
        if (result != EXIT_VIOLATION) {
            return result;
        }
        std::cerr
            << "Phase-boundary mismatch; checking all compiler passes to locate the change.\n";
    }
    const auto passes = generatePassList(p4_file, dump_dir / "passes", compiler_bin);
    // The detailed check is authoritative: it evaluates undefined behavior at
    // each individual transition.
    if (passes.size() < 2) {
        return EXIT_SKIPPED;
    }
    return compare(passes, true);
}

}  // namespace P4::ToZ3
