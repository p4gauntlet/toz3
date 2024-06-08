#ifndef _P4PRUNER_OPTIONS_H_
#define _P4PRUNER_OPTIONS_H_

#include <string>

#include "frontends/common/options.h"
#include "frontends/common/parser_options.h"

namespace P4PRUNER {

class PrunerOptions : public CompilerOptions {
 public:
    PrunerOptions();
    // output file
    std::optional<std::string> config_file;
    std::optional<std::string> validation_bin;
    std::optional<std::string> compiler_bin = std::nullopt;
    std::string working_dir = "pruned";
    bool dry_run = false;
    bool do_rnd_prune = false;
    bool print_pruned = false;
    std::optional<std::string> seed;
    std::optional<std::string> bug_type = std::nullopt;
    std::optional<std::string> output_file = std::nullopt;
};

using P4PrunerContext = P4CContextWithOptions<PrunerOptions>;

}  // namespace P4PRUNER

#endif /* _P4PRUNER_OPTIONS_H_ */
