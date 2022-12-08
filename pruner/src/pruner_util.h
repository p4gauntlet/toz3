#ifndef _PRUNER_UTIL_H_
#define _PRUNER_UTIL_H_
#include <cstdint>
#include <iostream>

#include <boost/random/mersenne_twister.hpp>

#include "ir/ir.h"
#include "lib/cstring.h"

#define INFO(x) std::cout << x << std::endl;

namespace P4PRUNER {

class PrunerRng {
 private:
    boost::random::mt19937 rng;

 public:
    static PrunerRng& instance() {
        static PrunerRng instance;

        return instance;
    }
    static void seed(int s) { instance().rng.seed(s); }
    static int64_t get_rnd_int(int64_t min, int64_t max);
    static double get_rnd_pct();
};

enum class ErrorType : uint32_t {
    SemanticBug = 0,
    CrashBug = 1,
    Error = 2,
    Unknown = 3,
    Success = 4,
    Undefined = 5
};

struct ExitInfo {
    int exit_code = 0;
    cstring err_msg;
    ExitInfo() : err_msg(nullptr) {}
};

struct PrunerConfig {
    int exit_code = 0;
    cstring validation_bin;
    cstring prog_before;
    cstring prog_post;
    cstring compiler;
    cstring working_dir;
    cstring out_file_name;
    cstring err_string;
    bool allow_undef = false;
    ErrorType err_type = ErrorType::Unknown;
    PrunerConfig()
        : validation_bin(nullptr),
          prog_before{nullptr},
          prog_post(nullptr),
          compiler(nullptr),
          working_dir(nullptr),
          out_file_name(nullptr),
          err_string(nullptr) {}
};

bool file_exists(cstring file_path);
void create_dir(cstring folder_path);
void remove_file(cstring file_path);
cstring remove_extension(cstring file_path);
cstring get_file_stem(cstring file_path);
cstring get_parent(cstring file_path);

ExitInfo get_exit_info(cstring name, P4PRUNER::PrunerConfig pruner_conf);
ExitInfo get_crash_exit_info(cstring name, P4PRUNER::PrunerConfig pruner_conf);

ErrorType classify_bug(ExitInfo exit_info);
int get_exit_code(cstring name, P4PRUNER::PrunerConfig pruner_conf);

void emit_p4_program(const IR::P4Program* program, cstring prog_name);
void print_p4_program(const IR::P4Program* program);

bool compare_files(const IR::P4Program* prog_before, const IR::P4Program* prog_after);

double measure_pct(const IR::P4Program* prog_before, const IR::P4Program* prog_after);

double measure_size(const IR::P4Program* prog);

uint64_t count_statements(const IR::P4Program* prog);

int check_pruned_program(const IR::P4Program** orig_program, const IR::P4Program* pruned_program,
                         P4PRUNER::PrunerConfig pruner_conf);
}  // namespace P4PRUNER

#endif /* _PRUNER_UTIL_H_ */
