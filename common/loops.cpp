#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "exceptions.h"
#include "ir/ir.h"
#include "lib/iterator_range.h"
#include "type_inference.h"
#include "visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

constexpr unsigned MAX_LOOP_ITERATIONS = 1000;

class LoopReads : public Inspector {
    std::set<cstring> names;

 public:
    const auto &getNames() const { return names; }
    bool preorder(const IR::PathExpression *path) override {
        names.insert(path->toString());
        return false;
    }
    bool preorder(const IR::Member *member) override {
        names.insert(member->toString());
        return false;
    }
};

class LoopWrites : public Inspector {
    const P4State &state;
    std::set<const IR::Node *> expanding;
    std::set<cstring> names;
    bool has_calls = false;
    bool has_early_termination = false;

    const IR::Node *actionBody(const IR::MethodCallExpression *call) const {
        const auto *path = call->method->to<IR::PathExpression>();
        if (path == nullptr) {
            return nullptr;
        }
        const auto name = mangle_name(path->path->name, call->arguments->size());
        const auto *declaration = state.find_static_decl(name);
        if (declaration == nullptr) {
            return nullptr;
        }
        const auto *action = declaration->get_decl()->to<IR::P4Action>();
        return action != nullptr && action->getParameters()->empty() && call->arguments->empty()
                   ? action->body
                   : nullptr;
    }

 public:
    explicit LoopWrites(const P4State &state) : state(state) { visitDagOnce = false; }
    auto &getNames() { return names; }
    bool hasCalls() const { return has_calls; }
    bool hasEarlyTermination() const { return has_early_termination; }
    bool preorder(const IR::BreakStatement * /*unused*/) override {
        has_early_termination = true;
        return false;
    }
    bool preorder(const IR::ReturnStatement * /*unused*/) override {
        has_early_termination = true;
        return false;
    }
    bool preorder(const IR::ExitStatement * /*unused*/) override {
        has_early_termination = true;
        return false;
    }
    bool preorder(const IR::BaseAssignmentStatement *assignment) override {
        names.insert(assignment->left->toString());
        return true;
    }
    bool preorder(const IR::MethodCallExpression *call) override {
        const IR::Node *body = actionBody(call);
        if (const auto *member = call->method->to<IR::Member>()) {
            if (member->member == "isValid" && call->arguments->empty()) {
                try {
                    const auto receiver = expression_type(state, member->expr);
                    if (receiver->is<IR::Type_Header>() || receiver->is<IR::Type_HeaderUnion>()) {
                        // The built-in is pure, but evaluating its receiver can call functions.
                        return true;
                    }
                } catch (const UnsupportedFeatureError &) {
                    // An unknown receiver cannot establish that this is a pure built-in.
                }
            }
            const auto *path = member->expr->to<IR::PathExpression>();
            const auto *declaration =
                path != nullptr ? state.find_static_decl(path->path->name) : nullptr;
            const auto *table =
                declaration != nullptr ? declaration->to<P4TableInstance>() : nullptr;
            if (member->member == "apply" && table != nullptr && call->arguments->empty() &&
                table->table_props.immutable && table->table_props.keys.empty() &&
                table->table_props.entries.empty()) {
                body = table->table_props.default_action;
            }
        }
        if (body != nullptr && expanding.insert(body).second) {
            visit(body);
            expanding.erase(body);
            return false;
        }
        has_calls = true;
        return false;
    }
};

bool overlaps(cstring left, cstring right) {
    const auto a = left.string_view();
    const auto b = right.string_view();
    return a == b || a.starts_with(std::string(b) + ".") || b.starts_with(std::string(a) + ".") ||
           a.starts_with(std::string(b) + "[") || b.starts_with(std::string(a) + "[");
}

bool overlaps(const std::set<cstring> &reads, const std::set<cstring> &writes) {
    return std::any_of(reads.begin(), reads.end(), [&](const auto &read) {
        return std::any_of(writes.begin(), writes.end(),
                           [&](const auto &write) { return overlaps(read, write); });
    });
}

z3::expr executionCondition(const P4State &state) {
    auto condition = state.get_exit_cond();
    for (const auto &part : state.get_return_conds()) {
        condition = condition && part;
    }
    return condition.simplify();
}

}  // namespace

bool Z3Visitor::try_additive_loop(const IR::ForStatement *loop, cstring index) {
    const auto *condition = loop->condition->to<IR::Lss>();
    if (condition == nullptr || !condition->left->is<IR::PathExpression>() ||
        condition->left->to<IR::PathExpression>()->path->name != index) {
        return false;
    }
    LoopReads boundReads;
    condition->right->apply(boundReads);
    for (const auto &name : boundReads.getNames()) {
        if (overlaps(name, index)) {
            return false;
        }
    }
    const auto *update = loop->updates.front()->to<IR::AssignmentStatement>();
    const auto *step = update != nullptr ? update->right->to<IR::Add>() : nullptr;
    const auto *stepValue = step != nullptr ? step->right->to<IR::Constant>() : nullptr;
    if (stepValue == nullptr || stepValue->value != 1 || step->left->toString() != index) {
        return false;
    }
    const IR::Statement *body = loop->body;
    if (const auto *block = body->to<IR::BlockStatement>()) {
        if (block->components.size() != 1) {
            return false;
        }
        body = block->components.front()->to<IR::Statement>();
        if (body == nullptr) {
            return false;
        }
    }
    const auto *assignment = body->to<IR::AssignmentStatement>();
    const auto *addition = assignment != nullptr ? assignment->right->to<IR::Add>() : nullptr;
    if (addition == nullptr || addition->left->toString() != assignment->left->toString() ||
        !addition->right->is<IR::Constant>()) {
        return false;
    }
    const IR::Expression *root = assignment->left;
    while (const auto *member = root->to<IR::Member>()) {
        root = member->expr;
    }
    if (!root->is<IR::PathExpression>()) {
        return false;
    }
    const auto *counter = state->get_var(index)->to<Z3Bitvector>();
    if (counter == nullptr || counter->bv_is_signed() || !counter->get_val()->is_bv()) {
        return false;
    }
    const auto start = *counter->get_val();
    visit(condition->right);
    const auto bound =
        pure_bv_cast(*state->get_expr_result<NumericVal>()->get_val(), start.get_sort());
    const auto count = z3::ite(z3::ult(start, bound), bound - start,
                               state->get_z3_ctx()->bv_val(0, start.get_sort().bv_size()))
                           .simplify();
    visit(assignment->left);
    const auto *accumulator = state->get_expr_result()->to<Z3Bitvector>();
    if (accumulator == nullptr || !accumulator->get_val()->is_bv()) {
        return false;
    }
    const auto before = *accumulator->get_val();
    visit(addition->right);
    const auto increment =
        pure_bv_cast(*state->get_expr_result<NumericVal>()->get_val(), before.get_sort());
    const auto result = (before + pure_bv_cast(count, before.get_sort()) * increment).simplify();
    const auto target = get_member_struct(state, this, assignment->left);
    state->set_var(target, allocate_instance<Z3Bitvector>(state, state, accumulator->get_p4_type(),
                                                          result, accumulator->bv_is_signed()));
    state->update_var(index, allocate_instance<Z3Bitvector>(
                                 state, state, counter->get_p4_type(),
                                 z3::ite(z3::ult(start, bound), bound, start).simplify()));
    return true;
}

namespace {
struct LoopAnalysis {
    cstring index;
    bool invariantCondition;
    bool canDetectCycle;
    bool unknownWrites;
    std::set<cstring> changed;
};

LoopAnalysis analyzeLoop(const P4State *state, const IR::ForStatement *loop) {
    cstring index;
    if (loop->updates.size() == 1) {
        if (const auto *assignment = loop->updates.front()->to<IR::BaseAssignmentStatement>()) {
            if (const auto *path = assignment->left->to<IR::PathExpression>()) {
                index = path->path->name;
            }
        }
    }
    LoopReads reads;
    loop->condition->apply(reads);
    LoopWrites writes(*state);
    loop->body->apply(writes);
    LoopWrites conditionWrites(*state);
    loop->condition->apply(conditionWrites);
    LoopWrites updateWrites(*state);
    LoopReads updateReads;
    for (const auto &update : loop->updates) {
        update->apply(updateWrites);
        update->apply(updateReads);
    }
    const bool knownEffects = !writes.hasCalls() && !writes.hasEarlyTermination() &&
                              !conditionWrites.hasCalls() && conditionWrites.getNames().empty() &&
                              !updateWrites.hasCalls();
    const bool bodyChangesCondition = overlaps(reads.getNames(), writes.getNames());
    const bool invariantCondition = knownEffects && !bodyChangesCondition &&
                                    !overlaps(reads.getNames(), updateWrites.getNames());
    bool canDetectCycle = knownEffects && !index.isNullOrEmpty() && !bodyChangesCondition &&
                          !overlaps(updateReads.getNames(), writes.getNames());
    for (const auto &write : writes.getNames()) {
        if (overlaps(write, index)) {
            canDetectCycle = false;
        }
    }
    std::set<cstring> changed;
    writes.getNames().insert(updateWrites.getNames().begin(), updateWrites.getNames().end());
    writes.getNames().insert(conditionWrites.getNames().begin(), conditionWrites.getNames().end());
    for (const auto &write : writes.getNames()) {
        const auto root = write.string_view().substr(0, write.string_view().find_first_of(".["));
        changed.insert(cstring(root));
    }
    if (!index.isNullOrEmpty()) {
        changed.insert(index);
    }
    return {index, invariantCondition, canDetectCycle,
            writes.hasCalls() || updateWrites.hasCalls() || conditionWrites.hasCalls(),
            std::move(changed)};
}

void excludeNontermination(P4State *state, z3::expr nonterminating) {
    for (const auto &part : state->get_forward_conds()) {
        nonterminating = nonterminating && part;
    }
    state->set_termination_condition(
        (state->get_termination_condition() && !nonterminating).simplify());
}

bool repeatedIndex(const P4State *state, cstring index, bool *canDetectCycle,
                   std::set<std::string> *seenIndexes) {
    if (!*canDetectCycle) {
        return false;
    }
    const auto *numeric = state->get_var(index)->to<NumericVal>();
    if (numeric == nullptr || !numeric->get_val()->simplify().is_numeral()) {
        *canDetectCycle = false;
        return false;
    }
    return !seenIndexes->insert(numeric->get_val()->simplify().to_string()).second;
}
}  // namespace

bool Z3Visitor::preorder(const IR::ForStatement *loop) {
    state->push_scope();
    for (const auto &initial : loop->init) {
        visit(initial);
    }
    loops.emplace_back(state->get_z3_ctx());
    auto analysis = analyzeLoop(state, loop);
    if (analysis.invariantCondition) {
        visit(loop->condition);
        excludeNontermination(
            state, *state->get_expr_result<NumericVal>()->get_val() && executionCondition(*state));
        loops.pop_back();
        state->pop_lexical_scope();
        return false;
    }
    if (analysis.canDetectCycle && try_additive_loop(loop, analysis.index)) {
        loops.pop_back();
        state->pop_lexical_scope();
        return false;
    }
    const auto snapshot = [&] {
        return analysis.unknownWrites ? state->clone_vars() : state->clone_vars(analysis.changed);
    };
    std::set<std::string> seenIndexes;
    std::vector<std::pair<z3::expr, VarMap>> completed;
    auto active = state->get_z3_ctx()->bool_val(true);
    unsigned iterations = 0;
    while (true) {
        active = (active && executionCondition(*state)).simplify();
        if (active.is_false()) {
            break;
        }
        visit(loop->condition);
        const auto condition = state->get_expr_result<NumericVal>()->get_val()->simplify();
        const auto finished = (active && !condition).simplify();
        if (!finished.is_false()) {
            completed.emplace_back(finished, snapshot());
        }
        active = (active && condition).simplify();
        if (active.is_false()) {
            break;
        }
        if (repeatedIndex(state, analysis.index, &analysis.canDetectCycle, &seenIndexes)) {
            // The condition's other inputs are unchanged by the body. Repeating
            // the counter therefore proves that the remaining paths never finish.
            excludeNontermination(state, active);
            break;
        }
        if (++iterations > MAX_LOOP_ITERATIONS) {
            throw UnsupportedFeatureError("Loop exceeds the supported interpretation limit");
        }
        loops.back().stopped = false;
        loops.back().break_condition = state->get_z3_ctx()->bool_val(false);
        loops.back().continue_condition = state->get_z3_ctx()->bool_val(false);
        state->push_forward_cond(active);
        visit(loop->body);
        state->pop_forward_cond();
        const auto broken = loops.back().break_condition.simplify();
        if (!broken.is_false()) {
            completed.emplace_back(broken, snapshot());
        }
        active = (active && !broken && executionCondition(*state)).simplify();
        if (active.is_false() || state->has_returned() || state->has_exited()) {
            break;
        }
        loops.back().stopped = false;
        state->push_forward_cond(active);
        for (const auto &update : loop->updates) {
            visit(update);
        }
        state->pop_forward_cond();
    }
    loops.pop_back();
    for (const auto &entry : Util::iterator_range(completed).reverse()) {
        state->merge_vars(entry.first, entry.second);
    }
    state->pop_lexical_scope();
    return false;
}

void Z3Visitor::runCollectionLoop(const IR::ForInStatement *loop) {
    visit(loop->collection);
    const auto *collection = state->copy_expr_result()->to<StructBase>();
    if (collection == nullptr) {
        throw UnsupportedFeatureError("Unsupported for-in collection");
    }
    state->push_scope();
    if (loop->decl != nullptr) {
        visit(loop->decl);
    }
    loops.emplace_back(state->get_z3_ctx());
    for (const auto &entry : *collection->get_member_map()) {
        const auto active =
            (!loops.back().break_condition && executionCondition(*state)).simplify();
        if (active.is_false()) {
            break;
        }
        const auto skipped = state->clone_vars();
        const auto target = get_member_struct(state, this, loop->ref);
        state->set_var(target, entry.second->copy());
        loops.back().stopped = false;
        loops.back().continue_condition = state->get_z3_ctx()->bool_val(false);
        state->push_forward_cond(active);
        visit(loop->body);
        state->pop_forward_cond();
        state->merge_vars(!active, skipped);
        if (state->has_returned() || state->has_exited()) {
            break;
        }
    }
    loops.pop_back();
    state->pop_lexical_scope();
}

namespace {
big_int loopLimit(const z3::expr &value, unsigned width, bool isSigned, bool upper) {
    std::string numeral;
    if (value.is_numeral(numeral)) {
        return big_int(numeral);
    }
    if (width == 0) {
        throw UnsupportedFeatureError(upper ? "For-in upper bound has no finite range"
                                            : "For-in lower bound has no finite range");
    }
    if (upper) {
        return (big_int(1) << (isSigned ? width - 1 : width)) - 1;
    }
    return isSigned ? -(big_int(1) << (width - 1)) : big_int(0);
}
}  // namespace

bool Z3Visitor::preorder(const IR::ForInStatement *loop) {
    const auto *range = loop->collection->to<IR::Range>();
    if (range == nullptr) {
        runCollectionLoop(loop);
        return false;
    }
    visit(range->left);
    auto lo = *state->get_expr_result<NumericVal>()->get_val();
    const auto *lowerBits = state->get_expr_result()->to<Z3Bitvector>();
    const auto lowerSigned = lowerBits != nullptr && lowerBits->bv_is_signed();
    const auto lowerWidth = lo.is_bv() ? lo.get_sort().bv_size() : 0;
    visit(range->right);
    auto hi = *state->get_expr_result<NumericVal>()->get_val();
    const auto *upperBits = state->get_expr_result()->to<Z3Bitvector>();
    const auto upperSigned = upperBits != nullptr && upperBits->bv_is_signed();
    const auto upperWidth = hi.is_bv() ? hi.get_sort().bv_size() : 0;
    if (lo.is_bv()) {
        lo = z3::bv2int(lo, lowerSigned);
    }
    if (hi.is_bv()) {
        hi = z3::bv2int(hi, upperSigned);
    }
    lo = lo.simplify();
    hi = hi.simplify();
    const auto lower = loopLimit(lo, lowerWidth, lowerSigned, false);
    const auto upper = loopLimit(hi, upperWidth, upperSigned, true);
    if (upper - lower >= MAX_LOOP_ITERATIONS) {
        throw UnsupportedFeatureError("For-in range exceeds 1000 iterations");
    }
    state->push_scope();
    if (loop->decl != nullptr) {
        visit(loop->decl);
    }
    loops.emplace_back(state->get_z3_ctx());
    std::vector<std::pair<z3::expr, VarMap>> completed;
    for (big_int value = lower; value <= upper; ++value) {
        const auto current = state->get_z3_ctx()->int_val(Util::toString(value, 0, false).c_str());
        const auto active = (!loops.back().break_condition && executionCondition(*state) &&
                             lo <= current && current <= hi)
                                .simplify();
        if (active.is_false()) {
            if (loops.back().break_condition.is_true() || executionCondition(*state).is_false()) {
                break;
            }
            continue;
        }
        const auto skipped = state->clone_vars();
        const IR::Constant constant(value);
        state->set_var(this, loop->ref, &constant);
        loops.back().stopped = false;
        loops.back().continue_condition = state->get_z3_ctx()->bool_val(false);
        const auto previousBreak = loops.back().break_condition;
        state->push_forward_cond(active);
        visit(loop->body);
        state->pop_forward_cond();
        state->merge_vars(!active, skipped);
        const auto newlyBroken = (loops.back().break_condition && !previousBreak).simplify();
        if (!newlyBroken.is_false()) {
            completed.emplace_back(newlyBroken, state->clone_vars());
        }
        if (loops.back().break_condition.is_true() || state->has_returned() ||
            state->has_exited()) {
            break;
        }
    }
    loops.pop_back();
    for (const auto &entry : Util::iterator_range(completed).reverse()) {
        state->merge_vars(entry.first, entry.second);
    }
    state->pop_lexical_scope();
    return false;
}

bool Z3Visitor::preorder(const IR::BreakStatement * /*unused*/) {
    BUG_CHECK(!loops.empty(), "Break outside loop");
    auto condition = executionCondition(*state);
    for (const auto &part : state->get_forward_conds()) {
        condition = condition && part;
    }
    loops.back().break_condition = (loops.back().break_condition || condition).simplify();
    loops.back().stopped = true;
    return false;
}

bool Z3Visitor::preorder(const IR::ContinueStatement * /*unused*/) {
    BUG_CHECK(!loops.empty(), "Continue outside loop");
    auto condition = executionCondition(*state);
    for (const auto &part : state->get_forward_conds()) {
        condition = condition && part;
    }
    loops.back().continue_condition = (loops.back().continue_condition || condition).simplify();
    loops.back().stopped = true;
    return false;
}

}  // namespace P4::ToZ3
