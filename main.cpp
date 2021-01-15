#include <fstream>

#include "frontends/common/applyOptionsPragmas.h"
#include "frontends/common/constantFolding.h"
#include "frontends/common/parseInput.h"
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/createBuiltins.h"
#include "frontends/p4/directCalls.h"
#include "frontends/p4/evaluator/evaluator.h"
#include "frontends/p4/frontend.h"
#include "frontends/p4/parseAnnotations.h"
#include "frontends/p4/specialize.h"
#include "frontends/p4/specializeGenericFunctions.h"
#include "frontends/p4/typeChecking/bindVariables.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/validateParsedProgram.h"

#include "frontends/p4/fromv1.0/v1model.h"
#include "frontends/p4/toP4/toP4.h"

#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/log.h"
#include "lib/nullstream.h"

#include "codegen.h"
#include "toz3Options.h"

int main(int argc, char *const argv[]) {
    setup_gc_logging();

    AutoCompileContext autoP4toZ3Context(new TOZ3_V2::P4toZ3Context);
    auto &options = TOZ3_V2::P4toZ3Context::get().options();
    // we only handle P4_16 right now
    options.langVersion = CompilerOptions::FrontendVersion::P4_16;
    options.compilerVersion = "p4toz3 test";

    if (options.process(argc, argv) != nullptr) {
        options.setInputFile();
    }
    if (::errorCount() > 0)
        return 1;

    auto hook = options.getDebugHook();

    const IR::P4Program *program = nullptr;

    program = P4::parseP4File(options);

    if (program != nullptr && ::errorCount() == 0) {
        z3::context ctx;
        try {
            P4::P4COptionPragmaParser optionsPragmaParser;
            program->apply(P4::ApplyOptionsPragmas(optionsPragmaParser));
            // convert the P4 program to Z3 Python
            TOZ3_V2::CodeGenToz3 *cgt3 = new TOZ3_V2::CodeGenToz3(&ctx);
            program->apply(*cgt3);
        } catch (const Util::P4CExceptionBase &bug) {
            std::cerr << bug.what() << std::endl;
            return 1;
        }
    }

    return ::errorCount() > 0;
}
