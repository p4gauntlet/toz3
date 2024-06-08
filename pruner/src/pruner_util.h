#ifndef _PRUNER_UTIL_H_
#define _PRUNER_UTIL_H_
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <boost/random/mersenne_twister.hpp>

#include "ir/ir.h"
#include "lib/cstring.h"

#define INFO(x) std::cout << x << std::endl;

namespace P4PRUNER {

class PrunerRng {
 private:
    boost::random::mt19937 rng;

 public:
    static PrunerRng &instance() {
        static PrunerRng instance;

        return instance;
    }
    static void seed(int s) { instance().rng.seed(s); }
    static int64_t get_rnd_int(int64_t min, int64_t max);
    static double get_rnd_pct();
};

enum class ErrorType : uint32_t {
    Unknown = 0,
    SemanticBug = 1,
    CrashBug = 2,
    Error = 3,
    Success = 4,
    Undefined = 5
};

struct ExitInfo {
    int exit_code = 0;
    std::string err_msg;
    ExitInfo() {}
};

struct PrunerConfig {
    int exit_code = 0;
    std::optional<std::filesystem::path> validation_bin;
    std::string prog_before;
    std::string prog_post;
    std::filesystem::path compiler;
    std::filesystem::path working_dir;
    std::filesystem::path out_file_name;
    std::string err_string;
    bool allow_undef = false;
    ErrorType err_type = ErrorType::Unknown;
    PrunerConfig() {}
};

ExitInfo get_exit_info(const std::filesystem::path &file,
                       const P4PRUNER::PrunerConfig &pruner_conf);
ExitInfo get_crash_exit_info(const std::filesystem::path &file,
                             const P4PRUNER::PrunerConfig &pruner_conf);

ErrorType classify_bug(ExitInfo exit_info);
int get_exit_code(const std::filesystem::path &file, const P4PRUNER::PrunerConfig &pruner_conf);

void emit_p4_program(const IR::P4Program *program, const std::filesystem::path &prog_name);
void print_p4_program(const IR::P4Program *program);

bool compare_files(const IR::P4Program *prog_before, const IR::P4Program *prog_after);

double measure_pct(const IR::P4Program *prog_before, const IR::P4Program *prog_after);

double measure_size(const IR::P4Program *prog);

uint64_t count_statements(const IR::P4Program *prog);

int check_pruned_program(const IR::P4Program **orig_program, const IR::P4Program *pruned_program,
                         const P4PRUNER::PrunerConfig &pruner_conf);
}  // namespace P4PRUNER

#endif /* _PRUNER_UTIL_H_ */
