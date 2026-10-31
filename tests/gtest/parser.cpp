#include <gtest/gtest.h>

#include "frontends/common/parseInput.h"
#include "interpreter_state.h"
#include "lib/error.h"
#include "toz3/common/create_z3.h"

namespace P4::ToZ3 {
namespace {

constexpr unsigned BYTE_WIDTH = 8;
constexpr unsigned WORD_WIDTH = 32;
constexpr unsigned PAYLOAD_VALUE = 7;
constexpr unsigned VERIFY_ERROR = 5;
constexpr unsigned TIMEOUT_ERROR = 6;
constexpr unsigned PARSER_ROUNDS = 8;
constexpr unsigned STACK_CAPACITY = 40;
constexpr unsigned TARGET_COUNT = 5;

z3::expr summaryField(z3::context *ctx, const MainResult &summary, cstring block, cstring field) {
    for (const auto &entry : summary.at(block).first) {
        if (entry.first == field) {
            return entry.second.simplify();
        }
    }
    ADD_FAILURE() << "Missing output " << block << "." << field;
    return ctx->bool_val(false);
}

void expectOutput(const z3::expr &output, unsigned width, unsigned value) {
    EXPECT_EQ(output.get_sort().bv_size(), width);
    EXPECT_EQ(output.get_numeral_uint(), value);
}

class ParserTest : public ::testing::Test, protected InterpreterTestState {
    std::vector<std::pair<cstring, z3::expr>> result;

 protected:
    void evaluate(const std::string &source) {
        const auto program = parseP4String(R"(
            error { NoError, PacketTooShort, NoMatch, StackOutOfBounds, HeaderTooShort, Bad, ParserTimeout }
            extern packet_in {
                void extract<T>(out T hdr);
                T lookahead<T>();
                void advance(in bit<32> bits);
                bit<32> length();
            }
            extern void verify(in bool condition, in error err);
        )" + source);
        ASSERT_NE(program, nullptr);
        Z3Visitor declarations(&state, false);
        program->apply(declarations);
        Z3Visitor interpreter(&state);
        const auto summary = gen_state_from_instance(&interpreter, get_main_decl(&state));
        result = summary.at("p"_cs).first;
    }
    z3::expr value(cstring name) {
        for (const auto &entry : result) {
            if (entry.first == name) {
                return entry.second;
            }
        }
        ADD_FAILURE() << "Missing output " << name;
        return ctx.bool_val(false);
    }
    z3::expr length() {
        return ctx.function("packet_length", ctx.string_sort(),
                            ctx.bv_sort(WORD_WIDTH))(ctx.string_val("p"));
    }
    z3::expr read(unsigned width, unsigned offset) {
        return ctx.function(("packet_read_" + std::to_string(width)).c_str(), ctx.string_sort(),
                            ctx.int_sort(),
                            ctx.bv_sort(width))(ctx.string_val("p"), ctx.int_val(offset));
    }
    void equivalent(const z3::expr &actual, const z3::expr &expected, const z3::expr &condition) {
        z3::solver solver(ctx);
        solver.add(condition && actual != expected);
        EXPECT_EQ(solver.check(), z3::unsat);
    }
};

TEST(ArchitectureTypes, GenericPackagesPreserveControlAndParserOutputs) {
    AutoCompileContext context{new P4CContextWithOptions<CompilerOptions>};
    z3::context ctx;
    P4State state(&ctx);
    const auto program = parseP4String(R"(
        error { NoError, NoMatch }
        control C8(out bit<8> value) { apply { value = 8; } }
        control C16(out bit<16> value) { apply { value = 16; } }
        parser P(out bit<8> value) {
            state start { value = 24; transition accept; }
        }
        package Generic<Block>(Block block);
        Generic(C8()) inferred;
        Generic<C16>(C16()) explicit;
        C8() control_instance;
        Generic(control_instance) named;
        Generic(P()) parser_instance;
        package Wrapper<Package>(Package inner, @optional Package unused);
        Wrapper(inferred) nested;
        package Top(Generic<C8> a, Generic<C16> b, Generic<C8> c,
                    Generic<P> d, Wrapper<Generic<C8>> e);
        Top(inferred, explicit, named, parser_instance, nested) main;
    )");
    ASSERT_NE(program, nullptr);
    Z3Visitor declarations(&state, false);
    program->apply(declarations);
    Z3Visitor interpreter(&state);
    const auto summary = gen_state_from_instance(&interpreter, get_main_decl(&state));
    ASSERT_EQ(summary.size(), 5U);
    const auto value = [&](cstring block, cstring field) {
        return summaryField(&ctx, summary, block, field);
    };
    for (const auto block : {"ablock"_cs, "cblock"_cs, "einnerblock"_cs}) {
        expectOutput(value(block, "value"_cs), BYTE_WIDTH, BYTE_WIDTH);
    }
    EXPECT_EQ(value("bblock"_cs, "value"_cs).get_sort().bv_size(), 16U);
    EXPECT_EQ(value("bblock"_cs, "value"_cs).get_numeral_uint(), 16U);
    EXPECT_EQ(value("dblock"_cs, "value"_cs).get_numeral_uint(), 24U);
    EXPECT_TRUE(value("dblock"_cs, "$parser_accepted"_cs).is_true());
}
TEST_F(ParserTest, StackExtractionAdvancesAndRejectionPreservesExtractedHeaders) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[2] stack; }
        parser P(packet_in packet, out Headers headers) {
            state start { packet.extract(headers.stack.next); transition again; }
            state again { packet.extract(headers.stack.next); transition overflow; }
            state overflow { packet.extract(headers.stack.next); transition accept; }
        }
        parser Proto(packet_in packet, out Headers headers);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(2, 32));
    equivalent(value("headers.stack.0.x"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("headers.stack.1.x"_cs), read(BYTE_WIDTH, BYTE_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(2 * BYTE_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(false), ctx.bool_val(true));
    equivalent(value("$parser_error"_cs), ctx.bv_val(3, WORD_WIDTH), enough);
    equivalent(value("$parser_error"_cs), ctx.bv_val(1, WORD_WIDTH),
               length() == ctx.bv_val(0, WORD_WIDTH));
    equivalent(value("$packet_cursor"_cs), ctx.int_val(0), length() == ctx.bv_val(0, WORD_WIDTH));
}

TEST_F(ParserTest, VerifyRejectsImmediatelyWithTheSpecifiedError) {
    evaluate(R"(
        parser P(in bool condition, out bit<8> result) {
            state start { result = 1; verify(condition, error.Bad); result = 2; transition accept; }
        }
        parser Proto(in bool condition, out bit<8> result);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto condition = ctx.bool_const("p.condition");
    equivalent(value("result"_cs),
               z3::ite(condition, ctx.bv_val(2, BYTE_WIDTH), ctx.bv_val(1, BYTE_WIDTH)),
               ctx.bool_val(true));
    equivalent(value("$parser_accepted"_cs), condition, ctx.bool_val(true));
    equivalent(value("$parser_error"_cs),
               z3::ite(condition, ctx.bv_val(0, WORD_WIDTH), ctx.bv_val(VERIFY_ERROR, WORD_WIDTH)),
               ctx.bool_val(true));
}

TEST_F(ParserTest, SelectEvaluatesItsKeyOnceAndUsesTheFirstMatchingCase) {
    evaluate(R"(
        bit<8> key(inout bit<8> count) { count += 1; return count; }
        parser P(out bit<8> count, out bit<8> result) {
            state start { count = 0; transition select(key(count)) { 0 .. 2: first; 1: second; } }
            state first { result = 7; transition accept; }
            state second { result = 9; transition reject; }
        }
        parser Proto(out bit<8> count, out bit<8> result);
        package Top(Proto p);
        Top(P()) main;
    )");
    equivalent(value("count"_cs), ctx.bv_val(1, BYTE_WIDTH), ctx.bool_val(true));
    equivalent(value("result"_cs), ctx.bv_val(PAYLOAD_VALUE, BYTE_WIDTH), ctx.bool_val(true));
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(true), ctx.bool_val(true));
}

TEST_F(ParserTest, LookaheadAndExtractionShareDataWithoutAdvancingLookahead) {
    evaluate(R"(
        header H { bit<8> x; }
        parser P(packet_in packet, out H hdr, out bit<8> before, out bit<8> after) {
            state start {
                before = packet.lookahead<bit<8>>();
                hdr = packet.lookahead<H>();
                packet.extract(hdr);
                after = packet.lookahead<bit<8>>();
                transition accept;
            }
        }
        parser Proto(packet_in packet, out H hdr, out bit<8> before, out bit<8> after);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(2, 32));
    equivalent(value("before"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("hdr.x"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("after"_cs), read(BYTE_WIDTH, BYTE_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(BYTE_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(true), enough);
}

TEST_F(ParserTest, MergedLoopPathsPreserveValuesAndStackBounds) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[2] stack; }
        parser P(packet_in packet, out Headers headers, out bit<8> result) {
            state start { result = 0; transition choose; }
            state choose {
                transition select(packet.lookahead<bit<8>>()) { 0: zero; default: other; }
            }
            state zero {
                packet.extract(headers.stack.next);
                result += 1;
                transition choose;
            }
            state other {
                packet.extract(headers.stack.next);
                result += 2;
                transition choose;
            }
        }
        parser Proto(packet_in packet, out Headers headers, out bit<8> result);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(3, 32));
    const auto first = z3::ite(read(8, 0) == ctx.bv_val(0, 8), ctx.bv_val(1, 8), ctx.bv_val(2, 8));
    const auto second = z3::ite(read(8, 8) == ctx.bv_val(0, 8), ctx.bv_val(1, 8), ctx.bv_val(2, 8));
    equivalent(value("result"_cs), first + second, enough);
    equivalent(value("headers.stack.0.x"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("headers.stack.1.x"_cs), read(BYTE_WIDTH, BYTE_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(2 * BYTE_WIDTH), enough);
    equivalent(value("$parser_error"_cs), ctx.bv_val(3, WORD_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(false), enough);
}

TEST_F(ParserTest, ShortExtractionPreservesEarlierAssignments) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H first; H second; }
        parser P(packet_in packet, out Headers headers) {
            state start {
                packet.extract(headers.first);
                headers.first.x = 1;
                packet.extract(headers.second);
                headers.first.x = 2;
                transition accept;
            }
        }
        parser Proto(packet_in packet, out Headers headers);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto short_packet = length() == ctx.bv_val(1, 32);
    equivalent(value("headers.first.x"_cs), ctx.bv_val(1, BYTE_WIDTH), short_packet);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(BYTE_WIDTH), short_packet);
    equivalent(value("$parser_error"_cs), ctx.bv_val(1, WORD_WIDTH), short_packet);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(false), short_packet);
}

TEST_F(ParserTest, CyclesCanConsumeDifferentStacksUntilTheirCombinedCapacityIsExhausted) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[1] first; H[2] second; }
        parser P(packet_in packet, out Headers headers, out bit<8> count) {
            state start { count = 0; transition choose; }
            state choose {
                transition select(packet.lookahead<bit<8>>()) { 0: first; default: second; }
            }
            state first { packet.extract(headers.first.next); count += 1; transition choose; }
            state second { packet.extract(headers.second.next); count += 1; transition choose; }
        }
        parser Proto(packet_in packet, out Headers headers, out bit<8> count);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto alternating = z3::uge(length(), ctx.bv_val(4, 32)) &&
                             read(8, 0) == ctx.bv_val(0, 8) && read(8, 8) != ctx.bv_val(0, 8) &&
                             read(8, 16) != ctx.bv_val(0, 8);
    equivalent(value("count"_cs), ctx.bv_val(3, BYTE_WIDTH), alternating);
    equivalent(value("headers.first.0.x"_cs), read(BYTE_WIDTH, 0), alternating);
    equivalent(value("headers.second.0.x"_cs), read(BYTE_WIDTH, BYTE_WIDTH), alternating);
    equivalent(value("headers.second.1.x"_cs), read(BYTE_WIDTH, 2 * BYTE_WIDTH), alternating);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(3 * BYTE_WIDTH), alternating);
    equivalent(value("$parser_error"_cs), ctx.bv_val(3, WORD_WIDTH), alternating);
}

TEST_F(ParserTest, ResettingAStackDoesNotImposeItsCapacityAsALoopBound) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[1] stack; }
        parser P(packet_in packet, out Headers headers, out bit<8> count) {
            state start { count = 0; transition again; }
            state again {
                headers.stack.pop_front(1);
                packet.extract(headers.stack.next);
                count += 1;
                transition select(count) { 3: accept; default: again; }
            }
        }
        parser Proto(packet_in packet, out Headers headers, out bit<8> count);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(3, 32));
    equivalent(value("count"_cs), ctx.bv_val(3, BYTE_WIDTH), enough);
    equivalent(value("headers.stack.0.x"_cs), read(BYTE_WIDTH, 2 * BYTE_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(3 * BYTE_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(true), enough);
}

TEST_F(ParserTest, APathCanBypassStackExtractionAndStillTerminateByItsCounter) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[1] stack; }
        parser P(packet_in packet, out Headers headers, out bit<8> count) {
            state start { count = 0; transition choose; }
            state choose { transition select(count) { 0: extract; default: skip; } }
            state extract { packet.extract(headers.stack.next); count += 1; transition choose; }
            state skip {
                count += 1;
                transition select(count) { 5: accept; default: choose; }
            }
        }
        parser Proto(packet_in packet, out Headers headers, out bit<8> count);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(1, 32));
    equivalent(value("count"_cs), ctx.bv_val(TARGET_COUNT, BYTE_WIDTH), enough);
    equivalent(value("headers.stack.0.x"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(BYTE_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(true), enough);
}

TEST_F(ParserTest, SubparserAcceptanceResumesAndRejectionStopsTheCaller) {
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H first; H second; }
        parser Sub(packet_in b, out H hdr) {
            state start { b.extract(hdr); transition accept; }
        }
        parser P(packet_in packet, out Headers headers, out bit<8> result) {
            state start {
                result = 1;
                Sub.apply(packet, headers.first);
                result = 2;
                packet.extract(headers.second);
                result = 3;
                transition accept;
            }
        }
        parser Proto(packet_in packet, out Headers headers, out bit<8> result);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(2, 32));
    equivalent(value("headers.first.x"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("headers.second.x"_cs), read(BYTE_WIDTH, BYTE_WIDTH), enough);
    equivalent(value("result"_cs), ctx.bv_val(3, BYTE_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(2 * BYTE_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(true), enough);
    const auto short_packet = length() == ctx.bv_val(1, 32);
    equivalent(value("result"_cs), ctx.bv_val(2, BYTE_WIDTH), short_packet);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(BYTE_WIDTH), short_packet);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(false), short_packet);
    equivalent(value("result"_cs), ctx.bv_val(1, BYTE_WIDTH),
               length() == ctx.bv_val(0, WORD_WIDTH));
}

TEST_F(ParserTest, ProvenStackProgressRunsPastTheFallbackLimitWithNamedExtraction) {
    const auto warnings = warningCount();
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[40] stack; }
        parser Init(out bit<8> count) { state start { count = 0; transition accept; } }
        parser P(packet_in packet, out Headers headers, out bit<8> count) {
            Init() initialize;
            state start { initialize.apply(count); transition again; }
            state again {
                headers.stack.next.setInvalid();
                packet.extract(hdr = headers.stack.next);
                count += 1;
                transition again;
            }
        }
        parser Proto(packet_in packet, out Headers headers, out bit<8> count);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(40, 32));
    equivalent(value("count"_cs), ctx.bv_val(STACK_CAPACITY, BYTE_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(STACK_CAPACITY * BYTE_WIDTH), enough);
    equivalent(value("$parser_error"_cs), ctx.bv_val(3, WORD_WIDTH), enough);
    EXPECT_EQ(warningCount(), warnings);
}

TEST_F(ParserTest, RepeatedExtractionWarnsAndPreservesTimeoutAndShortPacketStates) {
    const auto warnings = warningCount();
    evaluate(R"(
        header H { bit<8> x; }
        parser P(packet_in packet, out H hdr) {
            state start { packet.extract(hdr); transition start; }
        }
        parser Proto(packet_in packet, out H hdr);
        package Top(Proto p);
        Top(P()) main;
    )");
    EXPECT_EQ(warningCount(), warnings + 1);
    const auto enough = z3::uge(length(), ctx.bv_val(8, 32));
    equivalent(value("$parser_error"_cs), ctx.bv_val(TIMEOUT_ERROR, WORD_WIDTH), enough);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(false), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(PARSER_ROUNDS * BYTE_WIDTH), enough);
    equivalent(value("hdr.x"_cs), read(BYTE_WIDTH, (PARSER_ROUNDS - 1) * BYTE_WIDTH), enough);
    const auto short_packet = length() == ctx.bv_val(2, 32);
    equivalent(value("$parser_error"_cs), ctx.bv_val(1, WORD_WIDTH), short_packet);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(2 * BYTE_WIDTH), short_packet);
    equivalent(value("hdr.x"_cs), read(BYTE_WIDTH, BYTE_WIDTH), short_packet);
}

TEST_F(ParserTest, ResettableStacksDoNotReceiveAProvenCapacityBound) {
    const auto warnings = warningCount();
    evaluate(R"(
        header H { bit<8> x; }
        struct Headers { H[1] stack; }
        parser Reset(inout Headers headers) {
            state start { headers.stack.pop_front(1); transition accept; }
        }
        parser P(packet_in packet, out Headers headers) {
            Reset() reset;
            state start {
                reset.apply(headers);
                packet.extract(headers.stack.next);
                transition start;
            }
        }
        parser Proto(packet_in packet, out Headers headers);
        package Top(Proto p);
        Top(P()) main;
    )");
    EXPECT_EQ(warningCount(), warnings + 1);
    const auto enough = z3::uge(length(), ctx.bv_val(8, 32));
    equivalent(value("$parser_error"_cs), ctx.bv_val(TIMEOUT_ERROR, WORD_WIDTH), enough);
    equivalent(value("$packet_cursor"_cs), ctx.int_val(PARSER_ROUNDS * BYTE_WIDTH), enough);
    equivalent(value("headers.stack.0.x"_cs), read(BYTE_WIDTH, (PARSER_ROUNDS - 1) * BYTE_WIDTH),
               enough);
}

TEST_F(ParserTest, TimingOutOnePathPreservesOtherAcceptedPaths) {
    evaluate(R"(
        parser P(in bool finish, out bit<8> count) {
            state start { count = 0; transition again; }
            state again {
                count += 1;
                transition select(finish) { true: accept; default: again; }
            }
        }
        parser Proto(in bool finish, out bit<8> count);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto finish = ctx.bool_const("p.finish");
    equivalent(value("count"_cs), ctx.bv_val(1, BYTE_WIDTH), finish);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(true), finish);
    equivalent(value("$parser_error"_cs), ctx.bv_val(0, WORD_WIDTH), finish);
    equivalent(value("count"_cs), ctx.bv_val(PARSER_ROUNDS, BYTE_WIDTH), !finish);
    equivalent(value("$parser_accepted"_cs), ctx.bool_val(false), !finish);
    equivalent(value("$parser_error"_cs), ctx.bv_val(TIMEOUT_ERROR, WORD_WIDTH), !finish);
}

TEST_F(ParserTest, UnionExtractionSelectsAMemberAndShortPacketsPreserveThePreviousMember) {
    evaluate(R"(
        header H { bit<8> x; }
        header_union U { H a; H b; }
        parser P(packet_in packet, out U u) {
            state start {
                u.a = { 7 };
                packet.extract(u.b);
                transition accept;
            }
        }
        parser Proto(packet_in packet, out U u);
        package Top(Proto p);
        Top(P()) main;
    )");
    const auto enough = z3::uge(length(), ctx.bv_val(1, 32));
    equivalent(value("u.a.$valid"_cs), ctx.bool_val(false), enough);
    equivalent(value("u.b.$valid"_cs), ctx.bool_val(true), enough);
    equivalent(value("u.b.x"_cs), read(BYTE_WIDTH, 0), enough);
    equivalent(value("u.a.$valid"_cs), ctx.bool_val(true), !enough);
    equivalent(value("u.b.$valid"_cs), ctx.bool_val(false), !enough);
    equivalent(value("u.a.x"_cs), ctx.bv_val(PAYLOAD_VALUE, BYTE_WIDTH), !enough);
}

}  // namespace
}  // namespace P4::ToZ3
