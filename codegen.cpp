#include <utility>

#include "codegen.h"
#include "complex_type.h"


namespace TOZ3_V2 {

Visitor::profile_t CodeGenToz3::init_apply(const IR::Node *node) {
    return Inspector::init_apply(node);
}

void CodeGenToz3::end_apply(const IR::Node *) {}

bool CodeGenToz3::preorder(const IR::P4Program *p) {
    // Start to visit the actual AST objects
    for (auto o : p->objects) {
        visit(o);
    }
    return false;
}

void CodeGenToz3::fill_with_z3_sorts(std::vector<const IR::Node *> *sorts,
                                     const IR::Type *t) {
    t = state->resolve_type(t);
    sorts->push_back(t);
}

bool CodeGenToz3::preorder(const IR::Type_StructLike *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool CodeGenToz3::preorder(const IR::Type_Stack *) { return false; }
bool CodeGenToz3::preorder(const IR::Type_Enum *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool CodeGenToz3::preorder(const IR::Type_Error *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool CodeGenToz3::preorder(const IR::Type_Extern *t) {
    state->type_map[t->name.name] = t;
    return false;
}

bool CodeGenToz3::preorder(const IR::P4Control *c) {
    auto ctrl_name = c->name.name;
    std::cout << "TYPE: " << ctrl_name << "\n";

    auto scope = new P4Scope();
    state->add_scope(scope);
    std::vector<cstring> state_names;
    std::vector<std::pair<cstring, z3::ast>> state_vars;
    // INITIALIZE
    for (auto param : *c->getApplyParameters()) {
        auto par_type = state->resolve_type(param->type);
        P4Z3Type var = state->gen_instance(param->name.name, par_type);
        state->insert_var(param->name.name, var);
        state_names.push_back(param->name.name);
    }
    // DO SOMETHING

    // COLLECT
    for (auto state_name : state_names) {
        P4Scope *scope;
        auto member = state->find_var(state_name, &scope);
        if (z3::ast *z3_var = boost::get<z3::ast>(&member)) {
            state_vars.push_back({state_name, *z3_var});
        } else if (auto z3_var = check_complex<StructInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                              z3_sub_vars.end());
        } else if (auto z3_var = check_complex<ErrorInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                              z3_sub_vars.end());
        } else if (auto z3_var = check_complex<EnumInstance>(member)) {
            auto z3_sub_vars = z3_var->get_z3_vars();
            state_vars.insert(state_vars.end(), z3_sub_vars.begin(),
                              z3_sub_vars.end());
        } else if (auto z3_var = check_complex<ExternInstance>(member)) {
            printf("Skipping extern...\n");
        } else {
            BUG("Var is neither type z3::ast nor P4ComplexInstance!");
        }
    }
    for (auto tuple : state_vars) {
        auto name = tuple.first;
        auto var = tuple.second;
        std::cout << name << ": " << var << "\n";
    }
    return false;
}

} // namespace TOZ3_V2
