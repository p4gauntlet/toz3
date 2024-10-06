#include "compiler_pruner.h"

#include <cstdlib>
#include <vector>

#include "extended_unused.h"
#include "frontends/common/constantFolding.h"
#include "frontends/common/parser_options.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/createBuiltins.h"
#include "frontends/p4/directCalls.h"
#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/p4/typeMap.h"
#include "ir/pass_manager.h"
#include "lib/error_reporter.h"
#include "replace_variables.h"
#include "toz3/pruner/src/pruner_util.h"

namespace P4::ToZ3::Pruner {

const IR::P4Program *apply_generic_passes(const IR::P4Program *program, PrunerConfig pruner_conf,
                                          bool *applied) {
    P4::ReferenceMap refMap;
    P4::TypeMap typeMap;
    const IR::P4Program *temp = nullptr;

    PassManager pass_manager({new P4::CreateBuiltins(), new P4::ResolveReferences(&refMap, true),
                              new P4::ConstantFolding(new P4::ConstantFoldingPolicy()),
                              new P4::InstantiateDirectCalls(),
                              new P4::TypeInference(&typeMap, false)});

    INFO("Applying Generic passes...");
    temp = program->apply(pass_manager);
    if (check_pruned_program(&program, temp, pruner_conf) == EXIT_SUCCESS) {
        *applied = true;
    }

    return program;
}

const IR::P4Program *apply_replace_vars(const IR::P4Program *program, PrunerConfig pruner_conf) {
    INFO("Replacing variables...");
    const IR::P4Program *temp = replace_variables(program, pruner_conf);
    check_pruned_program(&program, temp, pruner_conf);

    return program;
}

const IR::P4Program *apply_unused_decls(const IR::P4Program *program, PrunerConfig pruner_conf) {
    P4::ReferenceMap refMap;
    P4::TypeMap typeMap;
    const IR::P4Program *temp = nullptr;
    std::vector<struct_obj *> used_structs;
    PassManager pass_manager({new ExtendedUnusedDeclarations(&refMap, &used_structs)});
    temp = program->apply(pass_manager);
    check_pruned_program(&program, temp, pruner_conf);

    return program;
}

const IR::P4Program *apply_compiler_passes(const IR::P4Program *program, PrunerConfig pruner_conf) {
    // this disables warning temporarily to avoid spam
    auto prev_action = P4CContext::get().getDefaultWarningDiagnosticAction();
    auto action = DiagnosticAction::Ignore;
    P4CContext::get().setDefaultWarningDiagnosticAction(action);
    INFO("\nPruning with compiler passes")
    bool genericPassesApplied = false;
    // apply the compiler passes
    program = apply_generic_passes(program, pruner_conf, &genericPassesApplied);

    if (!genericPassesApplied) {
        INFO("Generic passes failed, aborting compiler pruner phase");
        return program;
    }
    program = apply_replace_vars(program, pruner_conf);
    program = apply_unused_decls(program, pruner_conf);

    // reset to previous warning
    P4CContext::get().setDefaultWarningDiagnosticAction(prev_action);

    return program;
}

}  // namespace P4::ToZ3::Pruner
