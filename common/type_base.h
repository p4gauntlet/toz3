#ifndef TOZ3_COMMON_TYPE_BASE_H_
#define TOZ3_COMMON_TYPE_BASE_H_

#include <cstdio>
#include <map>      // std::map
#include <stack>    // std::stack
#include <utility>  // std::pair
#include <vector>   // std::vector

#include "../contrib/z3/z3++.h"
#include "ir/ir.h"
#include "lib/cstring.h"
#include "util.h"

#define BOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT
#include <boost/range/adaptor/reversed.hpp>
#include <boost/variant.hpp>
#include <boost/variant/get.hpp>

namespace TOZ3 {

using namespace P4::literals;  // NOLINT

class Z3Int;
class Z3Bitvector;
class VoidResult;
class StructInstance;
class HeaderInstance;
class HeaderUnionInstance;
class EnumInstance;
class ErrorInstance;
class SerEnumInstance;
class ListInstance;
class ExternInstance;
class P4TableInstance;

using P4Z3Function = std::function<void(Visitor *, const IR::Vector<IR::Argument> *)>;
using FunOrMethod = boost::variant<P4Z3Function, const IR::Method *>;

struct Z3Slice {
    z3::expr hi;
    z3::expr lo;
};

inline std::ostream &operator<<(std::ostream &out, const TOZ3::Z3Slice &z3_slice) {
    out << "[" << z3_slice.hi << ":" << z3_slice.lo << "]";
    return out;
}

// Some static type definitions we can exploit when generating Z3 expressions
static const IR::Type_Boolean BOOL_TYPE{};
static const IR::Type_String STRING_TYPE{};
static const IR::Type_Void VOID_TYPE{};
static const IR::Type_InfInt INT_TYPE{};
static const IR::Type_Bits P4_STD_BIT_TYPE{32, false};

struct ParamInfo {
    const IR::ParameterList params;
    const IR::Vector<IR::Argument> arguments;
    const IR::TypeParameters type_params;
    const IR::Vector<IR::Type> type_args;
};

struct TableProperties {
    cstring table_name;
    std::vector<const IR::KeyElement *> keys;
    std::vector<const IR::MethodCallExpression *> actions;
    const IR::MethodCallExpression *default_action = nullptr;
    // TODO: Simplify
    std::vector<std::pair<const IR::ListExpression *, const IR::MethodCallExpression *>> entries;
    bool immutable;
};

class P4Z3Node {
 public:
    template <typename T>
    bool is() const {
        return to<T>() != nullptr;
    }
    template <typename T>
    const T *to() const {
        return dynamic_cast<const T *>(this);
    }
    template <typename T>
    T *to_mut() {
        return dynamic_cast<T *>(this);
    }

    virtual cstring get_static_type() const = 0;
    virtual cstring to_string() const = 0;
    friend inline std::ostream &operator<<(std::ostream &out, const TOZ3::P4Z3Node &type) {
        return out << type.to_string();
    }
};

using NameOrIndex = boost::variant<cstring, z3::expr>;
// These structures are used to cleanly resolve references for copy in and out
class MemberStruct {
 public:
    cstring main_member = nullptr;
    std::vector<NameOrIndex> mid_members;
    NameOrIndex target_member = nullptr;
    bool has_stack = false;
    bool is_flat = false;
    std::vector<Z3Slice> end_slices;

    cstring to_string() const {
        std::stringstream ret;
        ret << *this;
        return ret;
    }
    // TODO: Improve printing here.
    friend inline std::ostream &operator<<(std::ostream &out,
                                           const TOZ3::MemberStruct &member_struct) {
        if (member_struct.main_member != nullptr) {
            out << member_struct.main_member << ".";
        }
        for (const auto &mid_member : member_struct.mid_members) {
            out << mid_member << ".";
        }
        out << member_struct.target_member;
        for (const auto &end_slice : member_struct.end_slices) {
            out << end_slice << ".";
        }
        return out;
    }
};

using CopyArgs = std::vector<std::pair<MemberStruct, cstring>>;

struct ParserError : public std::exception {
    const char *what() const noexcept override { return "Parserexception"; }
};

class P4Z3Instance : public P4Z3Node {
 protected:
    const IR::Type *p4_type = nullptr;

 public:
    explicit P4Z3Instance(const IR::Type *p4_type) : p4_type(p4_type) {}
    ~P4Z3Instance() = default;

    const IR::Type *get_p4_type() const { return p4_type; }
    /****** UNARY OPERANDS ******/
    virtual P4Z3Instance *operator-() const {
        P4C_UNIMPLEMENTED("- not implemented for %s", to_string());
    }
    virtual P4Z3Instance *operator~() const {
        P4C_UNIMPLEMENTED("~ not implemented for %s", to_string());
    }
    virtual P4Z3Instance *operator!() const {
        P4C_UNIMPLEMENTED("! not implemented for %s", to_string());
    }
    /****** BINARY OPERANDS ******/
    virtual P4Z3Instance *operator*(const P4Z3Instance & /*var*/) const {
        P4C_UNIMPLEMENTED("* not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator/(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("/ not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator%(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("% not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator+(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("+ not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operatorAddSat(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("|+| not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator-(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("- not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operatorSubSat(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("|-| not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator>>(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED(">> not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator<<(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("<< not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator==(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("== not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator!=(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("!= not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator<(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("< not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator<=(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("<= not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator>(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("> not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator>=(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED(">= not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator&&(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("&& not implemented for %s.", get_static_type());
    }
    virtual z3::expr operator||(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("|| not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator&(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("& not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator|(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("| not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *operator^(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("^ not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *concat(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("concat not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *cast_allocate(const IR::Type * /*dest_sort*/) const {
        P4C_UNIMPLEMENTED("cast_allocate not implemented for %s.", get_static_type());
    }
    /****** TERNARY OPERANDS ******/
    virtual P4Z3Instance *slice(const z3::expr & /*hi*/, const z3::expr & /*lo*/) const {
        P4C_UNIMPLEMENTED("slice not implemented for %s.", get_static_type());
    }
    /****** CUSTOM FUNCTIONS ******/
    virtual void merge(const z3::expr & /*cond*/, const P4Z3Instance & /*then_var*/) {
        P4C_UNIMPLEMENTED("Complex expression merge not implemented for %s.", get_static_type());
    }
    virtual std::vector<std::pair<cstring, z3::expr>> get_z3_vars(
        cstring /*prefix*/ = ""_cs, const z3 ::expr * /*valid*/ = nullptr) const {
        P4C_UNIMPLEMENTED("get_z3_vars not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *copy() const {
        P4C_UNIMPLEMENTED("Copy not implemented for %s.", get_static_type());
    }
    virtual void set_undefined() {
        P4C_UNIMPLEMENTED("set_undefined not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *get_member(cstring /*member_name*/) const {
        P4C_UNIMPLEMENTED("get_member not implemented for %s.", get_static_type());
    }

    P4Z3Instance(const P4Z3Instance &other) { p4_type = other.p4_type; }
};

using VarMap = ordered_map<cstring, std::pair<P4Z3Instance *, const IR::Type *>>;
using MainResult =
    ordered_map<cstring, std::pair<std::vector<std::pair<cstring, z3::expr>>, const IR::Type *>>;

}  // namespace TOZ3

#endif  // TOZ3_COMMON_TYPE_BASE_H_
