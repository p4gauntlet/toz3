#include "packet.h"

#include "exceptions.h"
#include "state.h"
#include "type_inference.h"
#include "visitor_interpret.h"

namespace P4::ToZ3 {
namespace {

z3::expr packet_id(P4State *state) {
    return *state->get_var("$packet_id"_cs)->to<NumericVal>()->get_val();
}

z3::expr cursor(P4State *state) {
    return *state->get_var("$packet_cursor"_cs)->to<NumericVal>()->get_val();
}

z3::expr packet_length(P4State *state) {
    auto &ctx = *state->get_z3_ctx();
    return ctx.function("packet_length", ctx.string_sort(), ctx.bv_sort(32))(packet_id(state));
}

z3::expr packet_read(P4State *state, unsigned width, const z3::expr *variable_size = nullptr) {
    auto &ctx = *state->get_z3_ctx();
    if (width == 0) return ctx.int_val(0);
    // The read depends on packet position, not the temporary's name or the number of visits.
    // Aggregate lookahead and its bitvector lowering therefore share the same value.
    const auto name = "packet_read_" + std::to_string(width);
    if (variable_size) {
        return ctx.function((name + "_varbit").c_str(), ctx.string_sort(), ctx.int_sort(),
                            ctx.int_sort(),
                            ctx.bv_sort(width))(packet_id(state), cursor(state), *variable_size);
    }
    return ctx.function(name.c_str(), ctx.string_sort(), ctx.int_sort(), ctx.bv_sort(width))(
        packet_id(state), cursor(state));
}

void check_packet_read(Z3Visitor *visitor, const z3::expr &bits) {
    auto *state = visitor->get_state();
    // A failed packet operation has not assigned its out argument. Preserve that destination
    // while the parser exit machinery copies back the enclosing parsers' outputs.
    const auto outputs = state->get_copy_out_args();
    state->set_copy_out_args({});
    visitor->reject_parser(cursor(state) + bits > z3::bv2int(packet_length(state), false) * 8,
                           "PacketTooShort"_cs);
    state->set_copy_out_args(outputs);
}

z3::expr argument(P4State *state, const IR::Method *method, unsigned index) {
    z3::params normalization(*state->get_z3_ctx());
    normalization.set("mul2concat", true);
    // Normalize read sizes before merging paths so strength reduction builds the same cursor
    // and packet-read arguments on both sides of a comparison.
    return state->get_var(method->getParameters()->getParameter(index)->name)
        ->to<ValContainer>()
        ->get_val()
        ->simplify(normalization);
}

}  // namespace

P4Z3Instance *execute_packet_method(Z3Visitor *visitor, const IR::Method *method) {
    auto *state = visitor->get_state();
    auto &ctx = *state->get_z3_ctx();
    const auto *return_type = state->resolve_type(method->type->returnType);
    if (method->name == "length") {
        return allocate_instance<Z3Bitvector>(state, state, IR::Type_Bits::get(32),
                                              packet_length(state));
    }
    if (method->name == "advance") {
        const auto bits = z3::bv2int(argument(state, method, 0), false);
        check_packet_read(visitor, bits);
        if (!state->has_exited()) {
            state->update_var(
                "$packet_cursor"_cs,
                allocate_instance<Z3Int>(state, state, (cursor(state) + bits).simplify()));
        }
        return allocate_instance<VoidResult>(state);
    }
    if (method->name == "lookahead") {
        const auto width = serialized_size(*state, return_type, false).convert_to<unsigned>();
        check_packet_read(visitor, ctx.int_val(width));
        auto *result = state->gen_instance(cstring(UNDEF_LABEL), return_type);
        if (state->has_exited()) return result;
        const auto input = packet_read(state, width);
        if (auto *aggregate = result->to_mut<StructBase>()) {
            const auto valid = ctx.bool_val(true);
            aggregate->propagate_validity(&valid);
            aggregate->bind(&input, width);
            return aggregate;
        }
        return Z3Bitvector(state, IR::Type_Bits::get(width), input).cast_allocate(return_type);
    }
    if (method->name == "extract") {
        const auto *parameter = method->getParameters()->getParameter(0);
        const auto *header_type = state->resolve_type(parameter->type);
        auto bits = ctx.int_val(
            Util::toString(serialized_size(*state, header_type, false), 0, false).c_str());
        const bool variable = method->getParameters()->size() == 2;
        if (variable) bits = bits + z3::bv2int(argument(state, method, 1), false);
        check_packet_read(visitor, bits);
        if (state->has_exited()) return allocate_instance<VoidResult>(state);
        if (variable) {
            const auto maximum = serialized_size(*state, header_type, true);
            const auto outputs = state->get_copy_out_args();
            state->set_copy_out_args({});
            visitor->reject_parser(bits > ctx.int_val(Util::toString(maximum, 0, false).c_str()),
                                   "HeaderTooShort"_cs);
            state->set_copy_out_args(outputs);
        }
        if (state->has_exited()) return allocate_instance<VoidResult>(state);
        auto *result = state->gen_instance(cstring(UNDEF_LABEL), header_type)->to_mut<StructBase>();
        CHECK_NULL(result);
        const auto valid = ctx.bool_val(true);
        result->propagate_validity(&valid);
        if (result->get_width()) {
            const auto input = packet_read(state, result->get_width(), variable ? &bits : nullptr);
            result->bind(&input, result->get_width());
        }
        state->update_var(parameter->name, result);
        state->update_var(
            "$packet_cursor"_cs,
            allocate_instance<Z3Int>(state, state, (cursor(state) + bits).simplify()));
        return allocate_instance<VoidResult>(state);
    }
    throw UnsupportedFeatureError("Unsupported packet_in method " + method->name.name);
}

}  // namespace P4::ToZ3
