#include "rename_variables.h"

#include <map>
#include <set>
#include <sstream>

#include "frontends/common/parser_options.h"
#include "frontends/common/resolveReferences/resolveReferences.h"

namespace P4::ToZ3::Pruner {
namespace {

using VariableNames = std::map<const IR::IDeclaration *, cstring>;

class ChooseVariableNames : public Inspector {
    const P4::ReferenceMap &references;
    P4::MinimalNameGenerator generator;

    cstring prefix(const IR::Type *type) const {
        std::set<const IR::Type *> visited;
        while (type != nullptr && visited.insert(type).second) {
            if (const auto *name = type->to<IR::Type_Name>()) {
                const auto *declaration = references.getDeclaration(name->path);
                type = declaration == nullptr ? nullptr : declaration->to<IR::Type>();
            } else if (const auto *alias = type->to<IR::Type_Typedef>()) {
                type = alias->type;
            } else if (type->is<IR::Type_Boolean>()) {
                return "flag"_cs;
            } else if (type->is<IR::Type_Header>() || type->is<IR::Type_HeaderUnion>()) {
                return "hdr"_cs;
            } else if (type->is<IR::Type_Array>()) {
                return "hdrs"_cs;
            } else if (type->is<IR::Type_Struct>()) {
                return "data"_cs;
            } else {
                break;
            }
        }
        return "value"_cs;
    }

    void choose(const IR::Declaration *declaration, cstring base) {
        if (isSystemFile(declaration->srcInfo.getSourceFile()) ||
            declaration->name.name.startsWith("__") ||
            declaration->hasAnnotation(IR::Annotation::nameAnnotation)) {
            return;
        }
        names.emplace(declaration, generator.newName(base.string_view()));
    }

 public:
    VariableNames names;
    ChooseVariableNames(const IR::P4Program *program, const P4::ReferenceMap &references)
        : references(references), generator(program) {}

    void postorder(const IR::Declaration_Variable *variable) override {
        cstring base = prefix(variable->type);
        if (const auto *loop = getParent<IR::ForStatement>()) {
            if (loop->init.getDeclaration(variable->name.name) == variable) base = "index"_cs;
        } else if (const auto *loop = getParent<IR::ForInStatement>()) {
            if (loop->decl == variable) base = "item"_cs;
        }
        choose(variable, base);
    }
    void postorder(const IR::Declaration_Constant *constant) override {
        choose(constant, "constant"_cs);
    }
};

class RenameVariables : public Transform {
    const P4::ReferenceMap &references;
    const VariableNames &names;

    void rename(IR::Declaration *declaration) const {
        const auto found = names.find(getOriginal<IR::IDeclaration>());
        if (found != names.end()) {
            declaration->name = IR::ID(declaration->name.srcInfo, found->second);
        }
    }

 public:
    RenameVariables(const P4::ReferenceMap &references, const VariableNames &names)
        : references(references), names(names) {}
    const IR::Node *postorder(IR::Declaration_Variable *variable) override {
        rename(variable);
        return variable;
    }
    const IR::Node *postorder(IR::Declaration_Constant *constant) override {
        rename(constant);
        return constant;
    }
    const IR::Node *postorder(IR::PathExpression *expression) override {
        const auto *original = getOriginal<IR::PathExpression>();
        const auto *declaration = references.getDeclaration(original->path);
        const auto found = names.find(declaration);
        if (found != names.end()) {
            auto *path = expression->path->clone();
            path->name = IR::ID(path->name.srcInfo, found->second);
            expression->path = path;
        }
        return expression;
    }
};

}  // namespace

const IR::P4Program *rename_variables(const IR::P4Program *program,
                                    const PrunerConfig &pruner_conf) {
    P4CContextWithOptions<CompilerOptions> context;
    AutoCompileContext scope(&context);
    std::ostringstream diagnostics;
    context.errorReporter().setOutputStream(&diagnostics);
    context.setDefaultWarningDiagnosticAction(DiagnosticAction::Ignore);
    try {
        P4::ReferenceMap references;
        P4::ResolveReferences resolve(&references);
        program->apply(resolve);
        if (::P4::errorCount() != 0) return program;
        ChooseVariableNames choose(program, references);
        program->apply(choose);
        if (choose.names.empty()) return program;
        RenameVariables rename(references, choose.names);
        const auto *candidate = program->apply(rename);
        check_pruned_program(&program, candidate, pruner_conf);
    } catch (const Util::P4CExceptionBase &) {
        // Unresolved programs can still be useful compiler-crash reproducers.
    }
    return program;
}

}  // namespace P4::ToZ3::Pruner
