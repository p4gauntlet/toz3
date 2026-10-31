#include <z3++.h>

#include <algorithm>
#include <cstdio>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "exceptions.h"
#include "ir/id.h"
#include "ir/indexed_vector.h"
#include "ir/ir.h"
#include "ir/vector.h"
#include "lib/cstring.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/iterator_range.h"
#include "lib/ordered_map.h"
#include "toz3/common/state.h"
#include "toz3/common/type_base.h"
#include "toz3/common/type_complex.h"
#include "type_inference.h"
#include "util.h"
#include "visitor_interpret.h"

namespace P4::ToZ3 {

void Z3Visitor::reject_parser(const z3::expr &condition, cstring error) {
    const auto *code = state->get_var("error"_cs)->get_member(error)->to<NumericVal>();
    CHECK_NULL(code);
    reject_parser(condition, *code->get_val());
}

void Z3Visitor::reject_parser(const z3::expr &condition, const z3::expr &error) {
    const auto reject = condition.simplify();
    if (reject.is_false()) {
        return;
    }
    const auto *previous = state->get_var("$parser_error"_cs)->copy();
    state->update_var("$parser_error"_cs,
                      allocate_instance<Z3Bitvector>(
                          state, state, &P4_STD_BIT_TYPE,
                          pure_bv_cast(error, state->get_z3_ctx()->bv_sort(INT_WIDTH))));
    state->push_forward_cond(reject);
    const auto saved = in_parser;
    in_parser = true;
    preorder(new IR::ExitStatement());
    in_parser = saved;
    state->pop_forward_cond();
    state->update_var("$parser_error"_cs, previous->copy());
    state->set_exit(reject.is_true());
}

void Z3Visitor::visit_parser_state(cstring name) {
    if (name == IR::ParserState::accept) {
        auto condition = state->get_exit_cond();
        for (const auto &guard : state->get_forward_conds()) {
            condition = condition && guard;
        }
        for (const auto &guard : state->get_return_conds()) {
            condition = condition && guard;
        }
        state->push_return_state(condition.simplify(), state->clone_vars());
        // Accept is terminal for this parser path. Its complement must not constrain other
        // worklist jobs, which already carry their own reachability conditions.
        state->set_returned(true);
    } else if (name == IR::ParserState::reject) {
        reject_parser(state->get_z3_ctx()->bool_val(true));
    } else if (name == "$no_match"_cs) {
        reject_parser(state->get_z3_ctx()->bool_val(true), "NoMatch"_cs);
    } else if (state->get_static_decl(name)->get_decl()->is<IR::ParserState>()) {
        if (parser_transition) {
            parser_transition(name);
        } else {
            run_parser(name);
        }
    } else {
        visit(state->get_static_decl(name)->get_decl());
    }
}

z3::expr handle_select_cond(Z3Visitor *visitor, const StructBase *select_list,
                            const IR::ListExpression *list_expr);

std::vector<P4Z3Instance *> get_vec_from_map(const StructBase *input_struct) {
    std::vector<P4Z3Instance *> target_list;
    for (const auto &member : *input_struct->get_member_map()) {
        auto *member_instance = member.second;
        target_list.push_back(member_instance);
    }
    return target_list;
}

z3::expr check_cond(Z3Visitor *visitor, const P4Z3Instance *select_eval,
                    const IR::Expression *match_key) {
    auto *state = visitor->get_state();
    if (const auto *range = match_key->to<IR::Range>()) {
        // TODO: A hack to deal with mismatch between lists and ranges
        if (const auto *li = select_eval->to<ListInstance>()) {
            select_eval = li->get_member_map()->begin()->second;
        }
        visitor->visit(range->left);
        const auto *min = state->copy_expr_result();
        visitor->visit(range->right);
        const auto *max = state->get_expr_result();
        return *min <= *select_eval && *select_eval <= *max;
    }
    if (const auto *mask_expr = match_key->to<IR::Mask>()) {
        // TODO: A hack to deal with mismatch between lists and masks
        if (const auto *li = select_eval->to<ListInstance>()) {
            select_eval = li->get_member_map()->begin()->second;
        }
        visitor->visit(mask_expr->left);
        const auto *val = state->copy_expr_result();
        visitor->visit(mask_expr->right);
        const auto *mask = state->get_expr_result();
        return *(*select_eval & *mask) == *(*val & *mask);
    }
    if (const auto *match_list_expr = match_key->to<IR::ListExpression>()) {
        const auto *struct_select_eval = select_eval->to<StructBase>();
        return handle_select_cond(visitor, struct_select_eval, match_list_expr);
    }
    if (match_key->is<IR::DefaultExpression>()) {
        return state->get_z3_ctx()->bool_val(true);
    }
    visitor->visit(match_key);
    return *select_eval == *state->get_expr_result();
}

z3::expr handle_select_cond(Z3Visitor *visitor, const StructBase *select_list,
                            const IR::ListExpression *list_expr) {
    auto *state = visitor->get_state();
    z3::expr match_cond = state->get_z3_ctx()->bool_val(true);
    const auto select_members = get_vec_from_map(select_list);

    for (size_t idx = 0; idx < list_expr->size(); ++idx) {
        const auto match_key = list_expr->components.at(idx);
        const auto *select_eval = select_members.at(idx);
        match_cond = match_cond && check_cond(visitor, select_eval, match_key);
    }
    return match_cond;
}

std::vector<std::pair<z3::expr, cstring>> gather_select_conds(Z3Visitor *visitor,
                                                              const IR::SelectExpression *se) {
    z3::expr matches = visitor->get_state()->get_z3_ctx()->bool_val(false);
    std::vector<std::pair<z3::expr, cstring>> select_vector;
    bool has_default = false;
    visitor->visit(se->select);
    const auto *list_instance = visitor->get_state()->copy_expr_result<ListInstance>();
    for (const auto &select_case : se->selectCases) {
        auto state_name = select_case->state->path->name.name;
        if (select_case->keyset->is<IR::DefaultExpression>()) {
            select_vector.emplace_back(!matches, state_name);
            has_default = true;
            break;
        }
        const auto cond = check_cond(visitor, list_instance, select_case->keyset);
        select_vector.emplace_back(!matches && cond, state_name);
        matches = matches || cond;
    }
    // We have to insert a reject if the default expression is missing
    if (!has_default) {
        select_vector.emplace_back(!matches, "$no_match"_cs);
    }
    return select_vector;
}

namespace {

constexpr unsigned MAX_UNPROVEN_PARSER_ROUNDS = 8;
constexpr unsigned PARSER_REACHABILITY_WORK_LIMIT = 100000;

std::vector<cstring> parser_successors(const IR::ParserState *state) {
    const auto transition = state->selectExpression;
    if (transition == nullptr) {
        return {};
    }
    if (const auto *path = transition->to<IR::PathExpression>()) {
        return {path->path->name};
    }
    std::vector<cstring> result;
    if (const auto *select = transition->to<IR::SelectExpression>()) {
        for (const auto &branch : select->selectCases) {
            result.push_back(branch->state->path->name);
        }
    }
    return result;
}

// A successful extract(stack.next) consumes one remaining stack slot. This is a
// ranking function only if every cycle extracts a next element and no statement
// can replace a stack or decrease its index. Unknown calls invalidate the proof.
class ParserStackProgress : public Inspector {
    P4State *state;
    const VarMap variables;
    std::set<cstring> scalar_locals;
    bool preserves_indices = true;

    const P4Z3Instance *resolve(const IR::Expression *expression) const {
        if (const auto *path = expression->to<IR::PathExpression>()) {
            const auto entry = variables.find(path->path->name);
            return entry == variables.end() ? nullptr : entry->second.first;
        }
        if (const auto *member = expression->to<IR::Member>()) {
            const auto *parent = resolve(member->expr);
            // For effect analysis, every element has the same type. Accessing next/last
            // may reject, but a method on that header cannot reset the stack's index.
            if (const auto *stack = parent != nullptr ? parent->to<StackInstance>() : nullptr) {
                if ((member->member == "next" || member->member == "last") &&
                    stack->get_int_size() != 0) {
                    return stack->get_member("0"_cs);
                }
            }
            const auto *aggregate = parent != nullptr ? parent->to<StructBase>() : nullptr;
            if (aggregate == nullptr) {
                return nullptr;
            }
            const auto &members = *aggregate->get_member_map();
            const auto entry = members.find(member->member.name);
            return entry == members.end() ? nullptr : entry->second;
        }
        return nullptr;
    }

    static bool contains_stack(const P4Z3Instance *value) {
        if (value->is<StackInstance>()) {
            return true;
        }
        if (const auto *aggregate = value->to<StructBase>()) {
            const auto &members = *aggregate->get_member_map();
            return std::any_of(members.begin(), members.end(),
                               [](const auto &member) { return contains_stack(member.second); });
        }
        return false;
    }

    void check_write(const IR::Expression *left) {
        if (const auto *value = resolve(left)) {
            preserves_indices &= !contains_stack(value);
        } else if (const auto *path = left->to<IR::PathExpression>()) {
            preserves_indices &= scalar_locals.contains(path->path->name);
        } else {
            // Dynamic indexing and stack pseudo-members need a separate alias proof.
            preserves_indices = false;
        }
    }

    bool preorder(const IR::Declaration_Variable *declaration) override {
        // Resolving a shadowed name using the entry state could identify the wrong stack.
        if (variables.count(declaration->name.name) != 0U) {
            preserves_indices = false;
        }
        if (declaration->type->is<IR::Type_Bits>() || declaration->type->is<IR::Type_Boolean>()) {
            scalar_locals.insert(declaration->name.name);
        }
        return true;
    }
    bool preorder(const IR::BaseAssignmentStatement *assignment) override {
        check_write(assignment->left);
        return true;
    }
    bool hasStackFreeArguments(const IR::MethodCallExpression *call) const {
        return std::all_of(
            call->arguments->begin(), call->arguments->end(), [&](const auto &argument) {
                if (const auto *value = resolve(argument->expression)) {
                    return !contains_stack(value);
                }
                if (const auto *path = argument->expression->template to<IR::PathExpression>()) {
                    return scalar_locals.contains(path->path->name);
                }
                return argument->expression->template is<IR::Literal>();
            });
    }

    bool preorder(const IR::MethodCallExpression *call) override {
        if (const auto *member = packet_method(call)) {
            const auto count = call->arguments->size();
            preserves_indices &= (member->member == "extract" && (count == 1 || count == 2)) ||
                                 (member->member == "lookahead" && count == 0) ||
                                 (member->member == "length" && count == 0) ||
                                 (member->member == "advance" && count == 1);
        } else if (const auto *path = call->method->to<IR::PathExpression>()) {
            // A user function named verify could mutate its arguments. Only the
            // extern declaration is handled by the interpreter's built-in model.
            const auto *declaration = path->path->name == "verify"
                                          ? state->find_static_decl(mangle_name(
                                                path->path->name, call->arguments->size()))
                                          : nullptr;
            const auto *method =
                declaration != nullptr ? declaration->get_decl()->to<IR::Method>() : nullptr;
            preserves_indices &=
                method != nullptr && method->getParameters()->size() == 2 &&
                method->getParameters()->getParameter(0)->direction == IR::Direction::In &&
                method->getParameters()->getParameter(1)->direction == IR::Direction::In;
        } else if (const auto *member = call->method->to<IR::Member>()) {
            const auto *receiver = resolve(member->expr);
            const auto name = member->member;
            const bool header_method =
                receiver != nullptr &&
                (receiver->is<HeaderInstance>() || receiver->is<HeaderUnionInstance>()) &&
                call->arguments->empty() &&
                (name == "isValid" || name == "setValid" || name == "setInvalid");
            const auto *count = call->arguments->size() == 1
                                    ? call->arguments->at(0)->expression->to<IR::Constant>()
                                    : nullptr;
            const bool push = receiver != nullptr && receiver->is<StackInstance>() &&
                              name == "push_front" && count != nullptr && count->value >= 0;
            // A subparser can mutate its caller's stacks only through its arguments.
            // Calls on scalar/header arguments must not disqualify an unrelated stack loop.
            const bool subparser = receiver != nullptr && receiver->is<ControlInstance>() &&
                                   name == "apply" && receiver->get_p4_type()->is<IR::P4Parser>() &&
                                   hasStackFreeArguments(call);
            // pop_front and indirect stack writes still need a separate proof.
            preserves_indices &= header_method || push || subparser;
        } else {
            preserves_indices = false;
        }
        return true;
    }

 public:
    bool preservesIndices() const { return preserves_indices; }
    explicit ParserStackProgress(P4State *state) : state(state), variables(state->get_vars()) {
        visitDagOnce = false;
    }
    const IR::Member *packet_method(const IR::MethodCallExpression *call) const {
        const auto *member = call->method->to<IR::Member>();
        const auto *receiver = member != nullptr ? resolve(member->expr) : nullptr;
        const auto *type = receiver != nullptr && receiver->is<ExternInstance>()
                               ? receiver->get_p4_type()->to<IR::Type_Extern>()
                               : nullptr;
        return type != nullptr && type->name == "packet_in" ? member : nullptr;
    }

 private:
    const IR::Expression *extractionTarget(const IR::MethodCallExpression *call,
                                           const IR::Member *method) const {
        if (!call->arguments->at(0)->name) {
            return call->arguments->at(0)->expression;
        }
        const IR::Expression *argument = nullptr;
        const auto *packet = resolve(method->expr)->get_p4_type()->checkedTo<IR::Type_Extern>();
        for (const auto &declaration : packet->methods) {
            if (declaration->name != "extract" ||
                !arguments_match(*declaration->getParameters(), *call->arguments)) {
                continue;
            }
            const auto name = declaration->getParameters()->getParameter(0)->name;
            for (const auto &supplied : *call->arguments) {
                if (supplied->name == name) {
                    argument = supplied->expression;
                }
            }
        }
        return argument;
    }

 public:
    const StackInstance *advanced_stack(const IR::StatOrDecl *component) const {
        if (const auto *block = component->to<IR::BlockStatement>()) {
            for (const auto &nested : block->components) {
                if (const auto *stack = advanced_stack(nested)) {
                    return stack;
                }
            }
            return nullptr;
        }
        const auto *statement = component->to<IR::MethodCallStatement>();
        if (statement == nullptr) {
            return nullptr;
        }
        const auto call = statement->methodCall;
        const auto *method = packet_method(call);
        if (method == nullptr || method->member != "extract" || call->arguments->empty()) {
            return nullptr;
        }
        // Only an unconditional statement proves progress on every continuing path.
        const auto *argument = extractionTarget(call, method);
        if (argument == nullptr) {
            return nullptr;
        }
        while (const auto *member = argument->to<IR::Member>()) {
            if (member->member == "next") {
                const auto *parent = resolve(member->expr);
                return parent != nullptr ? parent->to<StackInstance>() : nullptr;
            }
            argument = member->expr;
        }
        return nullptr;
    }
};

std::set<const StackInstance *> advancedStacks(const IR::ParserState *parserState,
                                               const ParserStackProgress &effects) {
    std::set<const StackInstance *> stacks;
    for (const auto &component : parserState->components) {
        if (const auto *stack = effects.advanced_stack(component)) {
            stacks.insert(stack);
        }
    }
    return stacks;
}

std::optional<size_t> parser_stack_bound(P4State *state, const std::vector<cstring> &order,
                                         const std::map<cstring, unsigned> &ranks) {
    ParserStackProgress effects(state);
    std::set<const StackInstance *> stacks;
    std::set<cstring> progress_states;
    std::map<cstring, std::vector<cstring>> successors;
    for (const auto name : order) {
        const auto *parser_state =
            state->get_static_decl(name)->get_decl()->checkedTo<IR::ParserState>();
        parser_state->apply(effects);
        const auto advanced = advancedStacks(parser_state, effects);
        stacks.insert(advanced.begin(), advanced.end());
        if (!advanced.empty()) {
            progress_states.insert(name);
        }
        successors[name] = parser_successors(parser_state);
    }
    if (!effects.preservesIndices() || stacks.empty()) {
        return std::nullopt;
    }
    // Removing progress states must leave an acyclic graph. Counting an extract
    // somewhere in a strongly connected component would miss cycles that bypass it.
    std::set<cstring> active;
    std::set<cstring> visited;
    std::map<cstring, size_t> back_edges;
    std::function<bool(cstring)> acyclic = [&](cstring name) {
        if (progress_states.contains(name) || visited.contains(name)) {
            return true;
        }
        if (!active.insert(name).second) {
            return false;
        }
        for (const auto next : successors[name]) {
            if (!acyclic(next)) {
                return false;
            }
            const auto target = ranks.find(next);
            const auto edge = target != ranks.end() && target->second <= ranks.at(name) ? 1 : 0;
            back_edges[name] = std::max(back_edges[name], back_edges[next] + edge);
        }
        active.erase(name);
        visited.insert(name);
        return true;
    };
    for (const auto name : order) {
        if (!acyclic(name)) {
            return std::nullopt;
        }
    }
    size_t capacity = 0;
    for (const auto *stack : stacks) {
        capacity += stack->get_int_size();
    }
    size_t gap = 0;
    for (const auto &entry : back_edges) {
        gap = std::max(gap, entry.second);
    }
    // A path has at most capacity progress states. Each can leave through a back
    // edge, and each of the capacity + 1 intervals between them can cross at most
    // gap other back edges. Include the initial round and final rejecting attempt.
    return (capacity + 1) * (gap + 1);
}

z3::expr simplify_on_parser_path(P4State *state, const z3::expr &expression) {
    auto &ctx = *state->get_z3_ctx();
    z3::expr_vector sources(ctx);
    z3::expr_vector values(ctx);
    std::set<unsigned> seen;
    std::function<void(const z3::expr &)> remember = [&](const z3::expr &condition) {
        if (!seen.insert(condition.id()).second) {
            return;
        }
        if (condition.is_and()) {
            for (unsigned i = 0; i < condition.num_args(); ++i) {
                remember(condition.arg(i));
            }
        } else if (condition.is_eq() && condition.arg(1).is_numeral()) {
            sources.push_back(condition.arg(0));
            values.push_back(condition.arg(1));
        } else {
            sources.push_back(condition.is_not() ? condition.arg(0) : condition);
            values.push_back(ctx.bool_val(!condition.is_not()));
        }
    };
    for (const auto &condition : state->get_forward_conds()) {
        remember(condition);
    }
    auto result = expression;
    return result.substitute(sources, values).simplify();
}
}  // namespace

void process_select_cases(Z3Visitor *visitor,
                          const std::vector<std::pair<z3::expr, cstring>> &select_vector) {
    auto *state = visitor->get_state();
    bool has_exited = true;
    bool has_returned = true;
    std::vector<std::pair<z3::expr, VarMap>> case_states;
    for (const auto &select : select_vector) {
        const auto cond = simplify_on_parser_path(state, select.first);
        if (cond.is_false()) {
            continue;
        }
        auto path_name = select.second;
        auto old_vars = state->clone_vars();
        state->push_forward_cond(cond);
        visitor->visit_parser_state(path_name);
        state->pop_forward_cond();
        auto call_has_exited = state->has_exited();
        auto stmt_has_returned = state->has_returned();
        if (!(call_has_exited || stmt_has_returned)) {
            case_states.emplace_back(cond, state->get_vars());
        }
        has_exited = has_exited && call_has_exited;
        has_returned = has_returned && stmt_has_returned;
        state->set_exit(false);
        state->set_returned(false);
        state->restore_vars(old_vars);
    }
    state->set_exit(has_exited);
    state->set_returned(has_returned);
    for (const auto &entry : Util::iterator_range(case_states).reverse()) {
        state->merge_vars(entry.first, entry.second);
    }
}

namespace {
std::vector<cstring> parserOrder(const P4State *state, cstring start) {
    // Reverse DFS postorder orders forward edges. Back edges begin another unfolding round,
    // so every acyclic join receives all its incoming states before its body is evaluated.
    std::set<cstring> seen;
    std::vector<cstring> order;
    std::function<void(cstring)> discover = [&](cstring name) {
        if (name == IR::ParserState::accept || name == IR::ParserState::reject ||
            !seen.insert(name).second) {
            return;
        }
        const auto *parser_state =
            state->get_static_decl(name)->get_decl()->checkedTo<IR::ParserState>();
        for (const auto next : parser_successors(parser_state)) {
            discover(next);
        }
        order.push_back(name);
    };
    discover(start);
    return order;
}
struct ParserJob {
    cstring name;
    z3::expr condition;
    VarMap variables;
};
using ParserJobs = std::map<unsigned, ParserJob>;

void enqueueParserTransition(P4State *state, cstring target,
                             const std::map<cstring, unsigned> &ranks, unsigned currentRank,
                             ParserJobs *current, ParserJobs *next) {
    auto condition = state->get_exit_cond();
    for (const auto &guard : state->get_forward_conds()) {
        condition = condition && guard;
    }
    condition = condition.simplify();
    if (condition.is_false()) {
        return;
    }
    const auto rank = ranks.at(target);
    auto variables = state->clone_vars();
    auto &pending = rank > currentRank ? *current : *next;
    const auto [entry, inserted] = pending.emplace(rank, ParserJob{target, condition, variables});
    if (!inserted) {
        // Merge at joins before visiting the next layer; retain shared expression DAGs.
        for (auto &variable : entry->second.variables) {
            variable.second.first->merge(condition, *variables.at(variable.first).first);
        }
        entry->second.condition = (entry->second.condition || condition).simplify();
    }
}

void dischargeParserJobs(z3::solver *reachability, const ParserJobs &jobs) {
    auto pending = reachability->ctx().bool_val(false);
    for (const auto &job : jobs) {
        pending = pending || job.second.condition;
    }
    reachability->push();
    reachability->add(pending);
    const auto outcome = reachability->check();
    reachability->pop();
    if (outcome == z3::sat) {
        throw InternalError("Parser reached a path beyond its proved stack bound");
    }
    if (outcome == z3::unknown) {
        throw UnsupportedFeatureError("Parser stack bound could not discharge remaining paths");
    }
}
}  // namespace

void Z3Visitor::run_parser(cstring start) {
    const auto order = parserOrder(state, start);
    std::map<cstring, unsigned> ranks;
    for (const auto &name : Util::iterator_range(order).reverse()) {
        ranks.emplace(name, ranks.size());
    }
    const auto stack_bound = parser_stack_bound(state, order, ranks);
    ParserJobs current;
    ParserJobs next;
    current.emplace(ranks.at(start),
                    ParserJob{start, state->get_z3_ctx()->bool_val(true), state->clone_vars()});
    const auto original = state->clone_vars();
    const auto enclosing_continuation = state->get_exit_cond();
    auto continuation = enclosing_continuation;
    unsigned current_rank = 0;
    z3::solver reachability(*state->get_z3_ctx());
    z3::params limits(*state->get_z3_ctx());
    // A work limit keeps pruning decisions reproducible across compiler passes.
    limits.set("rlimit", PARSER_REACHABILITY_WORK_LIMIT);
    reachability.set(limits);
    const auto saved_transition = std::move(parser_transition);
    parser_transition = [&](cstring target) {
        enqueueParserTransition(state, target, ranks, current_rank, &current, &next);
    };
    const auto initial_returns = state->get_return_states().size();
    try {
        for (unsigned depth = 0; !current.empty(); ++depth) {
            if (stack_bound && depth >= *stack_bound) {
                // Conditional merges can leave syntactically nonconstant but
                // unreachable jobs. Discharge them once at the proved bound,
                // rather than running a solver query on every back edge.
                dischargeParserJobs(&reachability, current);
                break;
            }
            const bool timed_out = !stack_bound && depth >= MAX_UNPROVEN_PARSER_ROUNDS;
            if (timed_out) {
                warning(
                    "%1%: parser cycle has no proven stack-progress bound; stopping after %2% "
                    "unfolding rounds and modeling remaining paths as error.ParserTimeout.",
                    state->get_static_decl(current.begin()->second.name)->get_decl(),
                    MAX_UNPROVEN_PARSER_ROUNDS);
            }
            for (const auto &job : current) {
                current_rank = job.first;
                state->restore_vars(job.second.variables);
                // Rejections in other jobs cannot affect this job's disjoint path. Keep their
                // snapshots, but accumulate the enclosing continuation separately.
                state->set_exit_cond(enclosing_continuation);
                state->set_exit(false);
                state->set_returned(false);
                state->push_forward_cond(job.second.condition);
                if (timed_out) {
                    // The spec permits a parser execution budget. Preserve the state at
                    // exhaustion and reject only this path, just like any other parser error.
                    reject_parser(state->get_z3_ctx()->bool_val(true), "ParserTimeout"_cs);
                } else {
                    preorder(state->get_static_decl(job.second.name)
                                 ->get_decl()
                                 ->checkedTo<IR::ParserState>());
                }
                state->pop_forward_cond();
                continuation = continuation && state->get_exit_cond();
            }
            if (timed_out) {
                break;
            }
            current = std::move(next);
            next.clear();
        }
    } catch (...) {
        parser_transition = saved_transition;
        throw;
    }
    parser_transition = saved_transition;
    state->set_exit_cond(continuation.simplify());
    state->restore_vars(original);
    state->set_exit(state->get_return_states().size() == initial_returns);
    state->set_returned(!state->has_exited());
}

bool Z3Visitor::preorder(const IR::ParserState *ps) {
    state->push_scope();
    for (const auto &component : ps->components) {
        visit(component);
        if (state->has_exited() || state->has_returned()) {
            break;
        }
    }
    if (state->has_exited() || state->has_returned()) {
        state->pop_scope();
        return false;
    }
    if (ps->selectExpression == nullptr) {
        state->pop_scope();
        visit_parser_state(IR::ParserState::reject);
    } else if (const auto *path = ps->selectExpression->to<IR::PathExpression>()) {
        state->pop_scope();
        visit_parser_state(path->path->name);
    } else if (const auto *se = ps->selectExpression->to<IR::SelectExpression>()) {
        const auto select_vector = gather_select_conds(this, se);
        state->pop_scope();
        if (!state->has_exited()) {
            process_select_cases(this, select_vector);
        }
    } else {
        P4C_UNIMPLEMENTED("SelectExpression of type %s not implemented.",
                          ps->selectExpression->node_type_name());
    }
    return false;
}

}  // namespace P4::ToZ3
