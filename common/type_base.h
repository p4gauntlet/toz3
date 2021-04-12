#ifndef TOZ3_V2_COMMON_TYPE_BASE_H_
#define TOZ3_V2_COMMON_TYPE_BASE_H_

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

namespace TOZ3_V2 {

class Z3Int;
class Z3Bitvector;
class VoidResult;
class StructInstance;
class HeaderInstance;
class EnumInstance;
class ErrorInstance;
class ListInstance;
class ExternInstance;
class P4TableInstance;
class FunctionWrapper;
class P4Declaration;

using Z3Result = boost::variant<boost::recursive_wrapper<Z3Int>,
                                boost::recursive_wrapper<Z3Bitvector>,
                                boost::recursive_wrapper<VoidResult>,
                                boost::recursive_wrapper<StructInstance>,
                                boost::recursive_wrapper<HeaderInstance>,
                                boost::recursive_wrapper<EnumInstance>,
                                boost::recursive_wrapper<ErrorInstance>,
                                boost::recursive_wrapper<ListInstance>,
                                boost::recursive_wrapper<P4TableInstance>,
                                boost::recursive_wrapper<FunctionWrapper>,
                                boost::recursive_wrapper<P4Declaration>,
                                boost::recursive_wrapper<ExternInstance>>;
using Z3P4FunctionCall =
    std::function<void(Visitor *, const IR::Vector<IR::Argument> *)>;

struct Z3Slice {
    z3::expr hi;
    z3::expr lo;
};

using NameOrIndex = boost::variant<cstring, z3::expr>;
struct MemberStruct {
    cstring main_member = nullptr;
    std::vector<NameOrIndex> mid_members;
    NameOrIndex target_member = nullptr;
    bool has_stack = false;
    bool is_flat = false;
    std::vector<Z3Slice> end_slices;
};
using CopyArgs = std::vector<std::pair<MemberStruct, cstring>>;

struct ParamInfo {
    const IR::ParameterList params;
    const IR::Vector<IR::Argument> arguments;
    const IR::TypeParameters type_params;
    const IR::Vector<IR::Type> type_args;
};

class P4Z3Object {
 public:
    P4Z3Object() = default;
    template <typename T> bool is() const { return to<T>() != nullptr; }
    template <typename T> const T *to() const {
        return dynamic_cast<const T *>(this);
    }
    template <typename T> T *to_mut() { return dynamic_cast<T *>(this); }
    virtual ~P4Z3Object() = default;

    virtual cstring get_static_type() const = 0;
    virtual cstring to_string() const = 0;
    friend inline std::ostream &operator<<(std::ostream &out,
                                           const TOZ3_V2::P4Z3Object &type) {
        return out << type.to_string();
    }
};

class P4Z3Instance : public P4Z3Object {
 public:
    /****** UNARY OPERANDS ******/
    virtual Z3Result operator-() const {
        P4C_UNIMPLEMENTED("- not implemented for %s", to_string());
    }
    virtual Z3Result operator~() const {
        P4C_UNIMPLEMENTED("~ not implemented for %s", to_string());
    }
    virtual Z3Result operator!() const {
        P4C_UNIMPLEMENTED("! not implemented for %s", to_string());
    }
    /****** BINARY OPERANDS ******/
    virtual Z3Result operator*(const P4Z3Instance & /*var*/) const {
        P4C_UNIMPLEMENTED("* not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator/(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("/ not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator%(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("% not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator+(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("+ not implemented for %s.", get_static_type());
    }
    virtual Z3Result operatorAddSat(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("|+| not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator-(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("- not implemented for %s.", get_static_type());
    }
    virtual Z3Result operatorSubSat(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("|-| not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator>>(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED(">> not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator<<(const P4Z3Instance & /*other*/) const {
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
    virtual Z3Result operator&(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("& not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator|(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("| not implemented for %s.", get_static_type());
    }
    virtual Z3Result operator^(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("^ not implemented for %s.", get_static_type());
    }
    virtual Z3Result concat(const P4Z3Instance & /*other*/) const {
        P4C_UNIMPLEMENTED("concat not implemented for %s.", get_static_type());
    }
    virtual Z3Result cast(z3::sort & /*other*/) const {
        P4C_UNIMPLEMENTED("cast not implemented for %s.", get_static_type());
    }
    virtual Z3Result cast(const IR::Type * /*dest_sort*/) const {
        P4C_UNIMPLEMENTED("cast not implemented for %s.", get_static_type());
    }
    virtual P4Z3Instance *cast_allocate(const IR::Type * /*dest_sort*/) const {
        P4C_UNIMPLEMENTED("cast_allocate not implemented for %s.",
                          get_static_type());
    }
    /****** TERNARY OPERANDS ******/
    virtual Z3Result slice(const P4Z3Instance & /*hi*/,
                           const P4Z3Instance & /*lo*/) const {
        P4C_UNIMPLEMENTED("slice not implemented for %s.", get_static_type());
    }
    /****** CUSTOM FUNCTIONS ******/
    virtual void merge(const z3::expr & /*cond*/,
                       const P4Z3Instance & /*then_var*/) {
        P4C_UNIMPLEMENTED("Complex expression merge not implemented for %s.",
                          get_static_type());
    }
    virtual std::vector<std::pair<cstring, z3::expr>>
    get_z3_vars(cstring /*prefix*/ = "",
                const z3 ::expr * /*valid*/ = nullptr) const {
        P4C_UNIMPLEMENTED("get_z3_vars not implemented for %s.",
                          get_static_type());
    }
    virtual P4Z3Instance *copy() const {
        P4C_UNIMPLEMENTED("Copy not implemented for %s.", get_static_type());
    }
    virtual void set_undefined() {
        P4C_UNIMPLEMENTED("set_undefined not implemented for %s.",
                          get_static_type());
    }
    virtual P4Z3Instance *get_member(cstring /*member_name*/) const {
        P4C_UNIMPLEMENTED("get_member not implemented for %s.",
                          get_static_type());
    }
    virtual P4Z3Instance *get_function(cstring /*function_name*/) const {
        P4C_UNIMPLEMENTED("get_function not implemented for %s.",
                          get_static_type());
    }
};

}  // namespace TOZ3_V2

#endif  // TOZ3_V2_COMMON_TYPE_BASE_H_
