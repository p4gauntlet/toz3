#ifndef _COMPILER_PRUNER_H
#define _COMPILER_PRUNER_H

#include "ir/ir.h"
#include "pruner_util.h"

namespace P4::ToZ3::Pruner {

const IR::P4Program *apply_compiler_passes(const IR::P4Program *program, PrunerConfig pruner_conf);

}  // namespace P4::ToZ3::Pruner

#endif /* _COMPILER_PRUNER_H */
