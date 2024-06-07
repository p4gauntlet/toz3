
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>  // IWYU pragma: keep
#include <iostream>
#include <random>

#include <boost/lexical_cast.hpp>

#include "boolean_pruner.h"
#include "compiler_pruner.h"
#include "contrib/json.h"
#include "expression_pruner.h"
#include "frontends/common/options.h"
#include "frontends/common/parseInput.h"
#include "frontends/common/parser_options.h"
#include "ir/ir.h"
#include "lib/compile_context.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "pruner_options.h"
#include "pruner_util.h"
#include "statement_pruner.h"

const IR::P4Program *prune(const IR::P4Program *program, P4PRUNER::PrunerConfig pruner_conf,
                           uint64_t prog_size) {
    program = P4PRUNER::prune_statements(program, pruner_conf, prog_size);
    program = P4PRUNER::prune_expressions(program, pruner_conf);
    program = P4PRUNER::prune_bool_expressions(program, pruner_conf);
    program = P4PRUNER::apply_compiler_passes(program, pruner_conf);
    return program;
}

void process_seed(const P4PRUNER::PrunerOptions &options) {
    uint64_t seed = 0;
    if (options.seed.has_value()) {
        std::cerr << "Using provided seed.\n";
        try {
            seed = boost::lexical_cast<uint64_t>(options.seed.value());
        } catch (boost::bad_lexical_cast &) {
            ::error("invalid seed %s", options.seed.value());
            exit(EXIT_FAILURE);
        }
    } else {
        // No seed provided, we generate our own.
        std::cerr << "Using generated seed.\n";
        std::random_device r;
        seed = r();
    }
    std::cerr << "Seed:" << seed << "\n";
    P4PRUNER::PrunerRng::seed(seed);
}

P4PRUNER::PrunerConfig get_config_from_json(const std::filesystem::path &json_path,
                                            const std::filesystem::path &working_dir,
                                            const std::filesystem::path &input_file,
                                            const P4PRUNER::PrunerOptions &options) {
    if (!std::filesystem::exists(json_path)) {
        ::error("Config file %s does not exist! Exiting.", json_path);
        exit(EXIT_FAILURE);
    }
    P4PRUNER::PrunerConfig pruner_conf;
    nlohmann::json config_json;

    std::ifstream config_file(json_path);
    config_file >> config_json;

    pruner_conf.exit_code = config_json.at("exit_code");

    pruner_conf.compiler = std::filesystem::path(config_json.at("compiler"));
    pruner_conf.validation_bin = config_json.at("validation_bin");
    pruner_conf.prog_before = config_json.at("prog_before");
    pruner_conf.prog_post = config_json.at("prog_after");
    pruner_conf.working_dir = working_dir;
    pruner_conf.allow_undef = config_json.at("allow_undef");
    pruner_conf.err_string = config_json.at("err_string");

    if (options.output_file.has_value()) {
        INFO("Using provided output name : " << options.output_file.value());
        pruner_conf.out_file_name = options.output_file.value();
    } else {
        pruner_conf.out_file_name = input_file;
        pruner_conf.out_file_name =
            pruner_conf.out_file_name.replace_extension().string() + "_stripped.p4";
    }

    return pruner_conf;
}

P4PRUNER::PrunerConfig get_conf_from_script(const P4PRUNER::PrunerOptions &options) {
    INFO("Grabbing config by using the validation script.");
    if (!std::filesystem::exists(options.validation_bin.value())) {
        ::error("Path to validator binary %s invalid! Exiting.", options.validation_bin.value());
        exit(EXIT_FAILURE);
    }
    if (!std::filesystem::exists(options.compiler_bin.value())) {
        ::error("Path to compiler binary %s invalid! Exiting.", options.compiler_bin.value());
        exit(EXIT_FAILURE);
    }
    // assemble the command to run the validation script to get a config file
    std::string command = "python3 ";
    command += options.validation_bin.value();
    // the input
    command += " -i ";
    command += options.file;
    // the compiler binary
    command += " -p ";
    command += options.compiler_bin.value();
    // suppress output
    command += " -ll CRITICAL ";
    // change the output dir
    command += " -o ";
    command += options.working_dir;
    // also toggle config recording
    command += " -d ";
    int ret = system(command.c_str());
    if (ret == 0) {
        ::warning("There was no error. Pruning will yield bogus results.");
    }

    std::string file_stem = std::filesystem::path(options.file.c_str()).stem().string();
    // assemble the path for the config file
    // this is not great but we have no other way right now
    std::string conf_file = realpath(options.working_dir.c_str(), nullptr);
    conf_file += "/";
    conf_file += file_stem;
    conf_file += "/";
    conf_file += file_stem;
    conf_file += "_info.json";
    return get_config_from_json(conf_file, options.working_dir, options.file.c_str(), options);
}

P4PRUNER::PrunerConfig get_conf_from_compiler(const P4PRUNER::PrunerOptions &options) {
    INFO("Grabbing config by using the compiler binary.");
    if (!std::filesystem::exists(options.compiler_bin.value())) {
        ::error("Path to compiler binary %s invalid! Exiting.", options.compiler_bin.value());
        exit(EXIT_FAILURE);
    }
    P4PRUNER::PrunerConfig pruner_conf;

    // we do not have munch info, so leave most of this empty, we don't need it
    pruner_conf.compiler = options.compiler_bin.value();
    pruner_conf.working_dir = options.working_dir;
    pruner_conf.allow_undef = true;
    if (options.output_file.has_value()) {
        pruner_conf.out_file_name = options.output_file.value();
    } else {
        pruner_conf.out_file_name =
            std::filesystem::path(options.file.c_str()).replace_extension().string() +
            "_stripped.p4";
    }
    // auto-fill the exit info from running the compiler on it
    auto exit_info = P4PRUNER::get_exit_info(options.file.c_str(), pruner_conf);

    pruner_conf.exit_code = exit_info.exit_code;
    pruner_conf.err_string = exit_info.err_msg;
    return pruner_conf;
}

int main(int argc, char *const argv[]) {
    AutoCompileContext autoP4toZ3Context(new P4PRUNER::P4PrunerContext);
    auto &options = P4PRUNER::P4PrunerContext::get().options();
    options.langVersion = CompilerOptions::FrontendVersion::P4_16;
    const IR::P4Program *program = nullptr;
    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (::errorCount() > 0) {
        exit(EXIT_FAILURE);
    }

    if (options.file == nullptr) {
        options.usage();
        exit(EXIT_FAILURE);
    }

    if (std::filesystem::exists(options.working_dir)) {
        ::error(
            "Working directory already exists. To be safe, please choose a "
            "non-existent directory.");
        return EXIT_FAILURE;
    }

    P4PRUNER::ErrorType error_type;

    if (!options.bug_type.has_value() && !options.config_file.has_value()) {
        ::error("Please provide a valid bug type or a config file");
        options.usage();
        return EXIT_FAILURE;
    }
    P4PRUNER::PrunerConfig pruner_conf;
    if (options.config_file.has_value()) {
        pruner_conf = get_config_from_json(options.config_file.value(), options.working_dir,
                                           options.file.c_str(), options);
    } else {
        if (options.bug_type.value() == "CRASH") {
            INFO("Ignoring validation bin for crash bug");
            error_type = P4PRUNER::ErrorType::CrashBug;
        } else if (options.bug_type.value() == "VALIDATION") {
            error_type = P4PRUNER::ErrorType::SemanticBug;
        } else {
            ::error(
                "Please enter a valid bug type. VALIDATION for validation or "
                "CRASH for crash bug");
            exit(EXIT_FAILURE);
        }
        if (error_type == P4PRUNER::ErrorType::SemanticBug) {
            if (!(options.validation_bin.has_value() && options.compiler_bin.has_value())) {
                ::error(
                    "Need to provide both a validation binary and a compiler "
                    "binary to prune a validation bug");
                options.usage();
                return EXIT_FAILURE;
            }

            // we are not provided with a config but validation and compiler bin
            // we should infer the expected output from these first
            pruner_conf = get_conf_from_script(options);
        } else if (error_type == P4PRUNER::ErrorType::CrashBug) {
            if (!options.compiler_bin.has_value()) {
                ::error("Need to provide a compiler binary to prune a crash bug");
                options.usage();
                return EXIT_FAILURE;
            }

            // we are not provided with a config but a compiler bin
            // we should infer the expected output from the compiler bin first
            pruner_conf = get_conf_from_compiler(options);
        }
    }

    // create the working dir
    std::filesystem::create_directories(pruner_conf.working_dir);

    // at this point, all properties should be known
    P4PRUNER::ExitInfo exit_info;
    exit_info.exit_code = pruner_conf.exit_code;
    exit_info.err_msg = pruner_conf.err_string;
    // this should probably become part of the initial setup later
    pruner_conf.err_type = P4PRUNER::classify_bug(exit_info);

    try {
        // if a seed was provided, use it
        // otherwise generate a random seed and set it
        process_seed(options);
        // parse the input P4 program<
        program = P4::parseP4File(options);

        if (program != nullptr && ::errorCount() == 0) {
            const auto *original = program;
            double prog_size = P4PRUNER::count_statements(original);
            INFO("Size of the program :" << prog_size << " statements");

            program = prune(program, pruner_conf, prog_size);
            if (options.print_pruned) {
                P4PRUNER::print_p4_program(program);
            }
            // sometimes we do not want to emit the final file
            if (!options.dry_run) {
                P4PRUNER::emit_p4_program(program, pruner_conf.out_file_name);
            }
            INFO("Total reduction percentage = " << P4PRUNER::measure_pct(original, program)
                                                 << " %");
        }
        INFO("Done.");
    } catch (const std::exception &bug) {
        std::cerr << bug.what() << std::endl;
    }
    INFO("Removing ephemeral working directory.");
    // need to wait a little because of race conditions
    std::filesystem::remove_all(pruner_conf.working_dir);
    return static_cast<int>(::errorCount() > 0);
}
