#include <set>
#include <string>
#include <vector>

#include "exceptions.h"
#include "ir/ir.h"
#include "visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

class LoopReads : public Inspector {
 public:
    std::set<cstring> names;
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
 public:
    std::set<cstring> names;
    bool has_calls = false;
    bool has_early_termination = false;
    bool preorder(const IR::BreakStatement *) override {
        has_early_termination = true;
        return false;
    }
    bool preorder(const IR::ReturnStatement *) override {
        has_early_termination = true;
        return false;
    }
    bool preorder(const IR::ExitStatement *) override {
        has_early_termination = true;
        return false;
    }
    bool preorder(const IR::BaseAssignmentStatement *assignment) override {
        names.insert(assignment->left->toString());
        return true;
    }
    bool preorder(const IR::MethodCallExpression *call) override {
        if (const auto *member = call->method->to<IR::Member>()) {
            if (member->member == "isValid") return false;
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

z3::expr executionCondition(const P4State &state) {
    auto condition = state.get_exit_cond();
    for (const auto &part : state.get_return_conds()) condition = condition && part;
    return condition.simplify();
}

}  // namespace

bool Z3Visitor::try_additive_loop(const IR::ForStatement *loop, cstring index) {
    const auto *condition = loop->condition->to<IR::Lss>();
    if (!condition || !condition->left->is<IR::PathExpression>() ||
        condition->left->to<IR::PathExpression>()->path->name != index)
        return false;
    LoopReads boundReads;
    condition->right->apply(boundReads);
    for (const auto &name : boundReads.names) {
        if (overlaps(name, index)) return false;
    }
    const auto *update = loop->updates.front()->to<IR::AssignmentStatement>();
    const auto *step = update ? update->right->to<IR::Add>() : nullptr;
    const auto *stepValue = step ? step->right->to<IR::Constant>() : nullptr;
    if (!stepValue || stepValue->value != 1 || step->left->toString() != index) return false;
    const IR::Statement *body = loop->body;
    if (const auto *block = body->to<IR::BlockStatement>()) {
        if (block->components.size() != 1) return false;
        body = block->components.front()->to<IR::Statement>();
        if (!body) return false;
    }
    const auto *assignment = body->to<IR::AssignmentStatement>();
    const auto *addition = assignment ? assignment->right->to<IR::Add>() : nullptr;
    if (!addition || addition->left->toString() != assignment->left->toString() ||
        !addition->right->is<IR::Constant>())
        return false;
    const IR::Expression *root = assignment->left;
    while (const auto *member = root->to<IR::Member>()) root = member->expr;
    if (!root->is<IR::PathExpression>()) return false;
    const auto *counter = state->get_var(index)->to<Z3Bitvector>();
    if (!counter || counter->bv_is_signed() || !counter->get_val()->is_bv()) return false;
    const auto start = *counter->get_val();
    visit(condition->right);
    const auto bound =
        pure_bv_cast(*state->get_expr_result<NumericVal>()->get_val(), start.get_sort());
    const auto count = z3::ite(z3::ult(start, bound), bound - start,
                               state->get_z3_ctx()->bv_val(0, start.get_sort().bv_size()))
                           .simplify();
    visit(assignment->left);
    const auto *accumulator = state->get_expr_result()->to<Z3Bitvector>();
    if (!accumulator || !accumulator->get_val()->is_bv()) return false;
    const auto before = *accumulator->get_val();
    visit(addition->right);
    const auto increment =
        pure_bv_cast(*state->get_expr_result<NumericVal>()->get_val(), before.get_sort());
    const auto result = (before + pure_bv_cast(count, before.get_sort()) * increment).simplify();
    const auto target = get_member_struct(state, this, assignment->left);
    state->set_var(target, new Z3Bitvector(state, accumulator->get_p4_type(), result,
                                           accumulator->bv_is_signed()));
    state->update_var(index,
                      new Z3Bitvector(state, counter->get_p4_type(),
                                      z3::ite(z3::ult(start, bound), bound, start).simplify()));
    return true;
}

bool Z3Visitor::preorder(const IR::ForStatement *loop) {
    state->push_scope();
    for (const auto *initial : loop->init) visit(initial);
    loops.emplace_back(state->get_z3_ctx());
    cstring index;
    if (loop->updates.size() == 1) {
        if (const auto *assignment = loop->updates.front()->to<IR::BaseAssignmentStatement>()) {
            if (const auto *path = assignment->left->to<IR::PathExpression>())
                index = path->path->name;
        }
    }
    LoopReads reads;
    loop->condition->apply(reads);
    LoopWrites writes;
    loop->body->apply(writes);
    LoopWrites conditionWrites;
    loop->condition->apply(conditionWrites);
    LoopWrites updateWrites;
    LoopReads updateReads;
    for (const auto *update : loop->updates) {
        update->apply(updateWrites);
        update->apply(updateReads);
    }
    bool canDetectCycle = !index.isNullOrEmpty() && !writes.has_calls &&
                          !writes.has_early_termination && !conditionWrites.has_calls &&
                          conditionWrites.names.empty() && !updateWrites.has_calls;
    for (const auto &write : writes.names) {
        for (const auto &read : reads.names) {
            if (overlaps(write, read)) canDetectCycle = false;
        }
        for (const auto &read : updateReads.names) {
            if (overlaps(write, read)) canDetectCycle = false;
        }
        if (overlaps(write, index)) canDetectCycle = false;
    }
    if (canDetectCycle && try_additive_loop(loop, index)) {
        loops.pop_back();
        state->pop_lexical_scope();
        return false;
    }
    std::set<cstring> changed;
    writes.names.insert(updateWrites.names.begin(), updateWrites.names.end());
    writes.names.insert(conditionWrites.names.begin(), conditionWrites.names.end());
    for (const auto &write : writes.names) {
        const auto root = write.string_view().substr(0, write.string_view().find_first_of(".["));
        changed.insert(cstring(root));
    }
    if (!index.isNullOrEmpty()) changed.insert(index);
    const auto snapshot = [&] {
        return writes.has_calls || updateWrites.has_calls || conditionWrites.has_calls
                   ? state->clone_vars()
                   : state->clone_vars(changed);
    };
    std::set<std::string> seenIndexes;
    std::vector<std::pair<z3::expr, VarMap>> completed;
    auto active = state->get_z3_ctx()->bool_val(true);
    unsigned iterations = 0;
    while (true) {
        active = (active && executionCondition(*state)).simplify();
        if (active.is_false()) break;
        visit(loop->condition);
        const auto condition = state->get_expr_result<NumericVal>()->get_val()->simplify();
        const auto finished = (active && !condition).simplify();
        if (!finished.is_false()) completed.emplace_back(finished, snapshot());
        active = (active && condition).simplify();
        if (active.is_false()) break;
        if (canDetectCycle) {
            const auto *numeric = state->get_var(index)->to<NumericVal>();
            if (!numeric || !numeric->get_val()->simplify().is_numeral()) {
                canDetectCycle = false;
            } else if (!seenIndexes.insert(numeric->get_val()->simplify().to_string()).second) {
                // The condition's other inputs are unchanged by the body. Repeating
                // the counter therefore proves that the remaining paths never finish.
                auto nonterminating = active;
                for (const auto &part : state->get_forward_conds()) {
                    nonterminating = nonterminating && part;
                }
                state->set_termination_condition(
                    (state->get_termination_condition() && !nonterminating).simplify());
                break;
            }
        }
        if (++iterations > 1000) {
            throw UnsupportedFeatureError("Loop exceeds the supported interpretation limit");
        }
        loops.back().stopped = false;
        loops.back().break_condition = state->get_z3_ctx()->bool_val(false);
        loops.back().continue_condition = state->get_z3_ctx()->bool_val(false);
        state->push_forward_cond(active);
        visit(loop->body);
        state->pop_forward_cond();
        const auto broken = loops.back().break_condition.simplify();
        if (!broken.is_false()) completed.emplace_back(broken, snapshot());
        active = (active && !broken && executionCondition(*state)).simplify();
        if (active.is_false() || state->has_returned() || state->has_exited()) break;
        loops.back().stopped = false;
        state->push_forward_cond(active);
        for (const auto *update : loop->updates) visit(update);
        state->pop_forward_cond();
    }
    loops.pop_back();
    for (auto it = completed.rbegin(); it != completed.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
    state->pop_lexical_scope();
    return false;
}

bool Z3Visitor::preorder(const IR::ForInStatement *loop) {
    const auto *range = loop->collection->to<IR::Range>();
    if (!range) throw UnsupportedFeatureError("For-in collection is not a constant range");
    visit(range->left);
    auto lo = *state->get_expr_result<NumericVal>()->get_val();
    const auto *lowerBits = state->get_expr_result()->to<Z3Bitvector>();
    const auto lowerSigned = lowerBits && lowerBits->bv_is_signed();
    visit(range->right);
    auto hi = *state->get_expr_result<NumericVal>()->get_val();
    const auto *upperBits = state->get_expr_result()->to<Z3Bitvector>();
    const auto upperSigned = upperBits && upperBits->bv_is_signed();
    if (lo.is_bv()) lo = z3::bv2int(lo, lowerSigned);
    if (hi.is_bv()) hi = z3::bv2int(hi, upperSigned);
    lo = lo.simplify();
    hi = hi.simplify();
    if (!lo.is_numeral() || !hi.is_numeral()) {
        throw UnsupportedFeatureError("For-in range bounds must be constant");
    }
    std::string lowerString, upperString;
    lo.is_numeral(lowerString);
    hi.is_numeral(upperString);
    const big_int lower(lowerString), upper(upperString);
    if (upper - lower >= 1000)
        throw UnsupportedFeatureError("For-in range exceeds 1000 iterations");
    state->push_scope();
    if (loop->decl) visit(loop->decl);
    loops.emplace_back(state->get_z3_ctx());
    std::vector<std::pair<z3::expr, VarMap>> completed;
    for (big_int value = lower; value <= upper; ++value) {
        const auto active =
            (!loops.back().break_condition && executionCondition(*state)).simplify();
        if (active.is_false()) break;
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
        if (!newlyBroken.is_false()) completed.emplace_back(newlyBroken, state->clone_vars());
        if (loops.back().break_condition.is_true() || state->has_returned() || state->has_exited())
            break;
    }
    loops.pop_back();
    for (auto it = completed.rbegin(); it != completed.rend(); ++it) {
        state->merge_vars(it->first, it->second);
    }
    state->pop_lexical_scope();
    return false;
}

bool Z3Visitor::preorder(const IR::BreakStatement *) {
    BUG_CHECK(!loops.empty(), "Break outside loop");
    auto condition = executionCondition(*state);
    for (const auto &part : state->get_forward_conds()) condition = condition && part;
    loops.back().break_condition = (loops.back().break_condition || condition).simplify();
    loops.back().stopped = true;
    return false;
}

bool Z3Visitor::preorder(const IR::ContinueStatement *) {
    BUG_CHECK(!loops.empty(), "Continue outside loop");
    auto condition = executionCondition(*state);
    for (const auto &part : state->get_forward_conds()) condition = condition && part;
    loops.back().continue_condition = (loops.back().continue_condition || condition).simplify();
    loops.back().stopped = true;
    return false;
}

}  // namespace P4::ToZ3
