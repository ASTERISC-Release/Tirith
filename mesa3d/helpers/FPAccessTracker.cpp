#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

namespace {
class FuncPtrAccessTracker {
  std::set<Function *> Visited;

public:
  void analyzeFunction(Function *F) {
    if (!F || F->isDeclaration() || Visited.count(F))
      return;

    Visited.insert(F);

    for (Instruction &I : instructions(F)) {
      // Indirect calls (possible via function pointers)
      if (CallBase *CB = dyn_cast<CallBase>(&I)) {
        if (CB->isIndirectCall()) {
          Value *Called = CB->getCalledOperand()->stripPointerCasts();

          // We're looking for function pointers loaded from a struct
          if (auto *LI = dyn_cast<LoadInst>(Called)) {
            Value *Ptr = LI->getPointerOperand()->stripPointerCasts();
            if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
              if (auto *STy = dyn_cast<StructType>(GEP->getSourceElementType())) {
                if (GEP->getNumIndices() >= 2) {
                  auto IdxIt = GEP->idx_begin();
                  ++IdxIt; // skip first (object), second is field
                  if (auto *CI = dyn_cast<ConstantInt>(*IdxIt)) {
                    unsigned fieldIdx = CI->getZExtValue();
                    errs() << "[INDIRECT] " << F->getName()
                           << " accesses struct: " << STy->getName()
                           << " field #" << fieldIdx << "\n";
                  }
                }
              }
            }
          }
        } else {
          // Direct call, recurse
          Function *Callee = CB->getCalledFunction();
          if (Callee)
            analyzeFunction(Callee);
        }
      }
    }
  }
};

struct FuncPtrAccessAnalysis : public PassInfoMixin<FuncPtrAccessAnalysis> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    if (Function *StartFn = M.getFunction("_mesa_DrawElements")) {
      FuncPtrAccessTracker Tracker;
      Tracker.analyzeFunction(StartFn);
    } else {
      errs() << "No entry point (main) found\n";
    }

    return PreservedAnalyses::all();
  }
};
} // namespace

// New PM registration
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "FuncPtrAccessAnalysis", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "funcptr-access") {
            MPM.addPass(FuncPtrAccessAnalysis());
            return true;
          }
          return false;
        });
    }
  };
}
