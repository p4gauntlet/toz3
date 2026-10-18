#ifndef TOZ3_COMMON_EXCEPTIONS_H_
#define TOZ3_COMMON_EXCEPTIONS_H_

#include <stdexcept>
#include <string>

#include "ir/ir.h"
#include "lib/cstring.h"

namespace P4::ToZ3 {

class GauntletException : public std::runtime_error {
 public:
    explicit GauntletException(const std::string &message) : std::runtime_error(message) {}
};

class UnsupportedFeatureError : public GauntletException {
 public:
    UnsupportedFeatureError(cstring feature, const IR::Node *node)
        : GauntletException("Unsupported feature: " + feature.string() + " in node " +
                            node->node_type_name()) {}
    explicit UnsupportedFeatureError(const std::string &message) : GauntletException(message) {}
};

class InternalError : public GauntletException {
 public:
    explicit InternalError(const std::string &message)
        : GauntletException("Internal error: " + message) {}
};

class CompilerExecutionError : public GauntletException {
 public:
    explicit CompilerExecutionError(const std::string &message)
        : GauntletException("Compiler execution failed: " + message) {}
};

class Z3Error : public GauntletException {
 public:
    explicit Z3Error(const std::string &message)
        : GauntletException("Z3 solver error: " + message) {}
};

}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_EXCEPTIONS_H_
