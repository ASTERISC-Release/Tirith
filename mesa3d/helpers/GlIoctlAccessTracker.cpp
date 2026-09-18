#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <queue>

#include "llvm/Support/CommandLine.h"

using namespace llvm;

// Global command-line option
static cl::opt<std::string> StartFnNameOpt(
    "gl-start-fn", cl::desc("Name of the starting GL function"),
    cl::init("save_Vertex2f"));

namespace {

// Utility: Check if function directly calls another function
bool callsFunction(Function &F, Function *targetFn) {
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (auto *call = dyn_cast<CallBase>(&I)) {
                if (Function *callee = call->getCalledFunction()) {
                    if (callee == targetFn)
                        return true;
                }
            }
        }
    }
    return false;
}

struct GlAccessTrackerPass : PassInfoMixin<GlAccessTrackerPass> {
    std::string StartFnName;


    GlAccessTrackerPass() : StartFnName(StartFnNameOpt) {}

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        Function *startFn = M.getFunction(StartFnName);
        if (!startFn) {
            errs() << "Error: start function '" << StartFnName << "' not found!\n";
            return PreservedAnalyses::all();
        }

        Function *intelIoctlFn = M.getFunction("ioctl");
        if (!intelIoctlFn) {
            errs() << "Warning: ioctl function not found!\n";
        }

        StructType *glCtxTy = StructType::getTypeByName(M.getContext(), "struct.gl_context");
        if (!glCtxTy) {
            errs() << "Warning: struct.gl_context not found!\n";
        }

        std::set<Function *> visited;
        std::queue<std::pair<Function *, int>> worklist;

        visited.insert(startFn);
        worklist.emplace(startFn, 0);

        while (!worklist.empty()) {
            auto [F, depth] = worklist.front();
            worklist.pop();

            // errs() << "Depth " << depth << ": " << F->getName() << "\n";

            if (intelIoctlFn && callsFunction(*F, intelIoctlFn)) {
                errs() << "  ==> calls ioctl!\n";
            }

            for (auto &BB : *F) {
                for (auto &I : BB) {
                    if (auto *call = dyn_cast<CallBase>(&I)) {
                        Function *callee = call->getCalledFunction();
                        if (callee && !callee->isDeclaration() && visited.insert(callee).second) {
                            worklist.emplace(callee, depth + 1);
                        }
                    }
                }
            }
        }

        return PreservedAnalyses::all();
    }
};

} // namespace

// Plugin registration

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "GlAccessTracker", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "gl-access-tracker") {
                        MPM.addPass(GlAccessTrackerPass());
                        return true;
                    }
                    return false;
                });
        }};
}