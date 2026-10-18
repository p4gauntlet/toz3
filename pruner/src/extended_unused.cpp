#include "extended_unused.h"

namespace P4::ToZ3::Pruner {

const IR::Node *PruneUnused::preorder(IR::Type_StructLike *type) {
    if (!used.isUsed(getOriginal<IR::IDeclaration>())) {
        prune();
        return nullptr;
    }
    return type;
}

const IR::Node *PruneUnused::preorder(IR::StructField *field) {
    const auto *parent = getParent<IR::Type_StructLike>();
    const auto fields = usedFields.find(parent->name.name);
    return fields != usedFields.end() && fields->second.count(field->name.name) != 0U ? field
                                                                                      : nullptr;
}

const IR::Node *PruneUnused::preorder(IR::Type_Extern *type) {
    // Member calls refer to the extern instance, not to individual method
    // declarations. Keep the signatures of a live extern intact.
    prune();
    return used.isUsed(getOriginal<IR::IDeclaration>()) ? type : nullptr;
}

Visitor::profile_t ListStructs::init_apply(const IR::Node *node) {
    auto result = Inspector::init_apply(node);
    usedFields->clear();
    return result;
}

void ListStructs::retainFields(const IR::Type *type) {
    if (type == nullptr) return;
    if (const auto *specialized = type->to<IR::Type_SpecializedCanonical>()) {
        retainFields(specialized->substituted);
    } else if (const auto *aggregate = type->to<IR::Type_StructLike>()) {
        for (const auto *field : aggregate->fields) {
            (*usedFields)[aggregate->name.name].insert(field->name.name);
            retainFields(field->type);
        }
    } else if (const auto *array = type->to<IR::Type_Array>()) {
        retainFields(array->elementType);
    } else if (const auto *tuple = type->to<IR::Type_BaseList>()) {
        for (const auto *component : tuple->components) retainFields(component);
    }
}

bool ListStructs::preorder(const IR::Expression *expression) {
    if (const auto *member = expression->to<IR::Member>()) {
        const auto *baseType = typeMap.getType(member->expr, true);
        if (const auto *specialized = baseType->to<IR::Type_SpecializedCanonical>()) {
            baseType = specialized->substituted;
        }
        if (const auto *aggregate = baseType->to<IR::Type_StructLike>()) {
            if (aggregate->getField(member->member.name) != nullptr) {
                (*usedFields)[aggregate->name.name].insert(member->member.name);
            }
        }
    }

    // A receiver used only to select a field does not consume the whole value.
    // This handles nested members, locals, constants, parameters and stacks
    // uniformly, using inferred types rather than the declaration's spelling.
    if (const auto *member = getParent<IR::Member>()) {
        if (member->expr == expression) return true;
    }
    if (const auto *index = getParent<IR::ArrayIndex>()) {
        if (index->left == expression) return true;
    }

    // Whole-value uses require the complete layout, including implicit tuple
    // initializers that TypeInference converts to typed StructExpressions.
    // Retaining recursively also covers calls, returns, copies and comparisons.
    retainFields(typeMap.getType(expression, true));
    return true;
}

}  // namespace P4::ToZ3::Pruner
