#ifndef TOZ3_PRUNER_SRC_RENAME_VARIABLES_H_
#define TOZ3_PRUNER_SRC_RENAME_VARIABLES_H_

#include "ir/ir.h"
#include "pruner_util.h"

namespace P4::ToZ3::Pruner {

const IR::P4Program *rename_variables(const IR::P4Program *program,
                                    const PrunerConfig &pruner_conf);

}  // namespace P4::ToZ3::Pruner

#endif  // TOZ3_PRUNER_SRC_RENAME_VARIABLES_H_
