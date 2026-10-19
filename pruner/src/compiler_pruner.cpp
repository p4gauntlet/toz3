#include "compiler_pruner.h"

#include <cstdlib>
#include <map>
#include <sstream>
#include <vector>

#include "extended_unused.h"
#include "frontends/common/constantFolding.h"
#include "frontends/common/parser_options.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/createBuiltins.h"
#include "frontends/p4/directCalls.h"
#include "frontends/p4/parameterSubstitution.h"
#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/unusedDeclarations.h"
#include "ir/pass_manager.h"
#include "lib/error_reporter.h"
#include "replace_variables.h"
#include "toz3/pruner/src/pruner_util.h"

namespace P4::ToZ3::Pruner {

namespace {

struct UnusedParameter {
    const IR::IDeclaration *owner;
    const IR::Parameter *parameter;
};

class FindUnusedParameters : public Inspector {
    const P4::UsedDeclSet &used;

    void collect(const IR::IDeclaration *owner, const IR::ParameterList *parameters) {
        for (const auto *parameter : parameters->parameters) {
            if (!used.isUsed(parameter)) candidates.push_back({owner, parameter});
        }
    }

 public:
    std::vector<UnusedParameter> candidates;
    explicit FindUnusedParameters(const P4::UsedDeclSet &used) : used(used) {}
    void postorder(const IR::P4Action *action) override { collect(action, action->parameters); }
    void postorder(const IR::Function *function) override {
        // Extern implementations must retain their declared interface.
        if (!isInContext<IR::Declaration_Instance>()) {
            collect(function, function->type->parameters);
        }
    }
};

class FindParameterArguments : public Inspector, ResolutionContext {
    UnusedParameter candidate;

 public:
    std::map<const IR::MethodCallExpression *, size_t> arguments;
    explicit FindParameterArguments(UnusedParameter candidate) : candidate(candidate) {}
    void postorder(const IR::MethodCallExpression *call) override {
        const auto *path = call->method->to<IR::PathExpression>();
        if (path == nullptr) return;
        const auto *owner = getDeclaration(path->path, true);
        if (owner != candidate.owner) return;
        const auto *parameters = owner->is<IR::P4Action>()
                                     ? owner->to<IR::P4Action>()->parameters
                                     : owner->to<IR::Function>()->type->parameters;
        P4::ParameterSubstitution substitution;
        substitution.populate(parameters, call->arguments);
        const auto *argument = substitution.lookup(candidate.parameter);
        if (argument == nullptr) return;  // Unbound control-plane or optional parameter.
        for (size_t index = 0; index < call->arguments->size(); ++index) {
            if (call->arguments->at(index) == argument) arguments.emplace(call, index);
        }
    }
};

class RemoveParameter : public Transform {
    const IR::Parameter *parameter;
    const std::map<const IR::MethodCallExpression *, size_t> &arguments;

 public:
    RemoveParameter(const IR::Parameter *parameter,
                    const std::map<const IR::MethodCallExpression *, size_t> &arguments)
        : parameter(parameter), arguments(arguments) {}
    const IR::Node *postorder(IR::Parameter *node) override {
        return getOriginal<IR::Parameter>() == parameter ? nullptr : node;
    }
    const IR::Node *postorder(IR::MethodCallExpression *call) override {
        const auto found = arguments.find(getOriginal<IR::MethodCallExpression>());
        if (found == arguments.end()) return call;
        auto *remaining = new IR::Vector<IR::Argument>();
        for (size_t index = 0; index < call->arguments->size(); ++index) {
            if (index != found->second) remaining->push_back(call->arguments->at(index));
        }
        call->arguments = remaining;
        return call;
    }
};

const IR::P4Program *infer_parameter_types(const IR::P4Program *program, P4::TypeMap *typeMap) {
    // A rejected candidate must not leave errors in the reduction's context.
    P4CContextWithOptions<CompilerOptions> context;
    AutoCompileContext scope(&context);
    std::ostringstream diagnostics;
    context.errorReporter().setOutputStream(&diagnostics);
    context.setDefaultWarningDiagnosticAction(DiagnosticAction::Ignore);
    try {
        P4::TypeInference inference(typeMap, false);
        const auto *result = program->apply(inference);
        return ::P4::errorCount() == 0 ? result : nullptr;
    } catch (const Util::P4CExceptionBase &) {
        return nullptr;
    }
}

}  // namespace

const IR::P4Program *prune_parameters(const IR::P4Program *program,
                                    const PrunerConfig &pruner_conf) {
    while (true) {
        P4::TypeMap typeMap;
        const auto *typed = infer_parameter_types(program, &typeMap);
        if (typed == nullptr) return program;
        P4::UsedDeclSet used;
        P4::CollectUsedDeclarations collect(used);
        typed->apply(collect);
        FindUnusedParameters unused(used);
        typed->apply(unused);
        bool changed = false;
        for (const auto &candidate : unused.candidates) {
            FindParameterArguments calls(candidate);
            typed->apply(calls);
            RemoveParameter remove(candidate.parameter, calls.arguments);
            const auto *reduced = typed->apply(remove);
            P4::TypeMap reducedTypes;
            reduced = infer_parameter_types(reduced, &reducedTypes);
            if (reduced != nullptr &&
                check_pruned_program(&program, reduced, pruner_conf) == EXIT_SUCCESS) {
                changed = true;
                break;  // Recompute references after each accepted signature change.
            }
        }
        if (!changed) return program;
    }
}

const IR::P4Program *apply_generic_passes(const IR::P4Program *program,
                                          const PrunerConfig &pruner_conf, bool *applied) {
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

const IR::P4Program *apply_replace_vars(const IR::P4Program *program,
                                        const PrunerConfig &pruner_conf) {
    INFO("Replacing variables...");
    const IR::P4Program *temp = replace_variables(program, pruner_conf);
    check_pruned_program(&program, temp, pruner_conf);

    return program;
}

const IR::P4Program *apply_unused_decls(const IR::P4Program *program,
                                        const PrunerConfig &pruner_conf) {
    const IR::P4Program *temp = nullptr;
    PassManager pass_manager({new ExtendedUnusedDeclarations()});
    temp = program->apply(pass_manager);
    if (temp != nullptr) {
        check_pruned_program(&program, temp, pruner_conf);
    }

    return program;
}

const IR::P4Program *apply_compiler_passes(const IR::P4Program *program,
                                           const PrunerConfig &pruner_conf) {
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
    program = prune_parameters(program, pruner_conf);
    program = apply_unused_decls(program, pruner_conf);

    // reset to previous warning
    P4CContext::get().setDefaultWarningDiagnosticAction(prev_action);

    return program;
}

}  // namespace P4::ToZ3::Pruner
