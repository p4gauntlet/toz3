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
    auto scope = new P4Scope();
    state->add_scope(scope);
    for (auto param : *c->getApplyParameters()) {
        auto par_type = state->resolve_type(param->type);
        boost::any var = state->gen_instance(param->name.name, par_type);
        state->insert_var(param->name.name, var);
        if (par_type->is<IR::Type_StructLike>()) {
            StructInstance *si = boost::any_cast<StructInstance>(&var);
            auto vars = si->get_z3_vars();
            std::cout << vars << "\n";
        }
    }
    return false;
}

} // namespace TOZ3_V2
