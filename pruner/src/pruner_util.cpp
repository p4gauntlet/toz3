#include "pruner_util.h"

#include <sys/stat.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/random/uniform_int_distribution.hpp>

#include "counter.h"
#include "frontends/p4/toP4/toP4.h"
#include "lib/error.h"
#include "toz3/pruner/src/constants.h"

namespace P4PRUNER {

int64_t PrunerRng::get_rnd_int(int64_t min, int64_t max) {
    boost::random::uniform_int_distribution<int64_t> distribution(min, max);
    return distribution(instance().rng);
}

double PrunerRng::get_rnd_pct() {
    // Do not use boosts uniform_real_distribution, instead round coarsely by
    // dividing by a 100. This is not as precise but avoids floating point
    // inconsistencies.
    boost::random::uniform_int_distribution<uint8_t> distribution(0, 100);  // NOLINT
    return distribution(instance().rng) / 100.0;
}

bool file_exists(cstring file_path) {
    struct stat buffer {};
    INFO("Checking if " << file_path << " exists.");
    return stat(file_path, &buffer) == 0;
}

void create_dir(cstring folder_path) {
    int ret = 0;
    cstring cmd = "mkdir -p ";
    cmd += folder_path;
    ret = system(cmd);
    if (ret != 0) {
        ::warning("Creating folder %s failed.", folder_path);
    }
}

void remove_file(cstring file_path) {
    int ret = 0;
    cstring cmd = "rm -rf ";
    cmd += file_path;
    ret = system(cmd);
    if (ret != 0) {
        ::warning("Removing file or folder %s failed.", file_path);
    }
}

cstring get_file_stem(cstring file_path) {
    cstring file_stem;
    cstring stripped_name = P4PRUNER::remove_extension(file_path);

    const char *pos = stripped_name.findlast('/');
    // check if there even is a parent directory
    if (pos == nullptr) {
        return stripped_name;
    }
    auto idx = static_cast<size_t>(pos - stripped_name);
    if (idx != std::string::npos) {
        file_stem = stripped_name.substr(idx + 1);
    } else {
        file_stem = stripped_name;
    }
    return file_stem;
}

cstring get_parent(cstring file_path) {
    cstring file_stem;
    cstring stripped_name = P4PRUNER::remove_extension(file_path);

    const char *pos = stripped_name.findlast('/');
    // check if there even is a parent directory
    if (pos == nullptr) {
        return stripped_name;
    }
    auto idx = static_cast<size_t>(pos - stripped_name);
    if (idx != std::string::npos) {
        file_stem = stripped_name.substr(0, idx);
    } else {
        file_stem = "";
    }
    return file_stem;
}

cstring remove_extension(cstring file_path) {
    // find the last dot
    const char *last_dot = file_path.findlast('.');
    // there is no dot in this string, just return the full name
    if (last_dot == nullptr) {
        return file_path;
    }
    // otherwise get the index, remove the dot
    auto idx = static_cast<size_t>(last_dot - file_path);
    return file_path.substr(0, idx);
}

ExitInfo get_exit_info(cstring name, P4PRUNER::PrunerConfig pruner_conf) {
    ExitInfo exit_info;
    INFO("Checking exit code.");

    if (pruner_conf.err_type == ErrorType::SemanticBug) {
        cstring command = pruner_conf.validation_bin;
        command += " -i ";
        command += name;
        // set the output dir
        command += " -o ";
        command += pruner_conf.working_dir;
        // suppress output
        command += " -ll CRITICAL ";
        command += " -p ";
        command += pruner_conf.compiler;
        if (pruner_conf.allow_undef) {
            command += " -u ";
        }
        auto exit_code = system(command.c_str());
        exit_info.exit_code = WEXITSTATUS(exit_code);
        exit_info.err_msg = cstring("");

    } else {
        exit_info = get_crash_exit_info(name, pruner_conf);
    }
    return exit_info;
}

std::pair<int, cstring> exec(cstring cmd) {
    constexpr int BUF_SIZE = 1000;
    constexpr int CHUNK_SIZE = 128;

    std::array<char, BUF_SIZE> buffer{};
    std::string output;
    auto *pipe = popen(cmd, "r");  // get rid of shared_ptr

    if (pipe == nullptr) {
        throw std::runtime_error("popen() failed!");
    }

    while (feof(pipe) == 0) {
        if (fgets(buffer.data(), CHUNK_SIZE, pipe) != nullptr) {
            output += buffer.data();
        }
    }

    auto err_code = pclose(pipe);
    return {err_code, output};
}

ExitInfo get_crash_exit_info(cstring name, P4PRUNER::PrunerConfig pruner_conf) {
    // The crash bugs variant of get_exit_code
    ExitInfo exit_info;
    cstring include_dir = get_parent(pruner_conf.compiler) + "/../../p4include";
    cstring command = "P4C_16_INCLUDE_PATH=" + include_dir + " ";
    command += pruner_conf.compiler;
    command += " --Wdisable ";
    command += name;
    // Apparently popen doesn't like stderr hence redirecting stderr to
    // stdout
    command += " 2>&1";
    auto result = exec(command);
    exit_info.exit_code = WEXITSTATUS(result.first);
    exit_info.err_msg = result.second;
    return exit_info;
}

ErrorType classify_bug(ExitInfo exit_info) {
    int exit_code = exit_info.exit_code;

    if (exit_code == EXIT_TEST_VALIDATION) {
        return ErrorType::SemanticBug;
    }
    if (exit_code == EXIT_TEST_SUCCESS) {
        return ErrorType::Success;
    }
    if (exit_code == EXIT_TEST_UNDEFINED) {
        return ErrorType::Undefined;
    }
    cstring comp = exit_info.err_msg.find("Compiler Bug");

    if (!comp.isNullOrEmpty()) {
        INFO("Crash bug");
        return ErrorType::CrashBug;
    }
    cstring err_msg = exit_info.err_msg.find("error");

    if (!err_msg.isNullOrEmpty()) {
        return ErrorType::Error;
    }
    return ErrorType::Unknown;
}

void emit_p4_program(const IR::P4Program *program, cstring prog_name) {
    auto *temp_f = new std::ofstream(prog_name);
    auto *temp_p4 = new P4::ToP4(temp_f, false);
    program->apply(*temp_p4);
    temp_f->close();
}

void print_p4_program(const IR::P4Program *program) {
    auto *print_p4 = new P4::ToP4(&std::cout, false);
    program->apply(*print_p4);
}

bool compare_files(const IR::P4Program *prog_before, const IR::P4Program *prog_after) {
    auto *before_stream = new std::stringstream;
    auto *after_stream = new std::stringstream;

    auto *before = new P4::ToP4(before_stream, false);
    prog_before->apply(*before);

    auto *after = new P4::ToP4(after_stream, false);
    prog_after->apply(*after);

    return before_stream->str() == after_stream->str();
}

double measure_size(const IR::P4Program *prog) {
    auto *prog_stream = new std::stringstream;
    auto *toP4 = new P4::ToP4(prog_stream, false);
    prog->apply(*toP4);
    return prog_stream->str().length();
}

uint64_t count_statements(const IR::P4Program *prog) {
    auto *counter = new Counter();
    prog->apply(*counter);
    return counter->statements;
}

double measure_pct(const IR::P4Program *prog_before, const IR::P4Program *prog_after) {
    double before_len = measure_size(prog_before);

    return (before_len - measure_size(prog_after)) * (100.0 / before_len);
}

int check_pruned_program(const IR::P4Program **orig_program, const IR::P4Program *pruned_program,
                         P4PRUNER::PrunerConfig pruner_conf) {
    cstring out_file = pruner_conf.working_dir + "/" + get_file_stem(pruner_conf.out_file_name);

    // append a .p4 suffix
    out_file += ".p4";
    emit_p4_program(pruned_program, out_file);
    if (compare_files(pruned_program, *orig_program)) {
        INFO("File has not changed. Skipping analysis.");
        return EXIT_SUCCESS;
    }
    ExitInfo exit_info = get_exit_info(out_file, pruner_conf);
    int exit_code = exit_info.exit_code;
    ErrorType err_type = classify_bug(exit_info);
    // if got the right exit code and the right error type
    // then modify the original program, if not
    // then choose a smaller bank of statements to remove now.
    if (exit_code != pruner_conf.exit_code || err_type != pruner_conf.err_type) {
        INFO("FAILED");
        return EXIT_FAILURE;
    }

    INFO("PASSED: Reduced by " << measure_pct(*orig_program, pruned_program) << " %")
    *orig_program = pruned_program;
    return EXIT_SUCCESS;
}
}  // namespace P4PRUNER
