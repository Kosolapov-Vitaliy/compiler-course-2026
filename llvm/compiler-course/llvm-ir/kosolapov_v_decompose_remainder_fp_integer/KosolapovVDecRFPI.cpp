#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct KosolapovVDecRFPIPass : PassInfoMixin<KosolapovVDecRFPIPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Changed = false;

    for (BasicBlock &BB : F) {
      for (Instruction &I : make_early_inc_range(BB)) {
        auto *BinOp = dyn_cast<BinaryOperator>(&I);
        if (!BinOp)
          continue;

        Instruction::BinaryOps Op = BinOp->getOpcode();
        if (Op != Instruction::FRem && Op != Instruction::SRem &&
            Op != Instruction::URem)
          continue;

        IRBuilder<> Builder(BinOp);
        Value *LHS = BinOp->getOperand(0);
        Value *RHS = BinOp->getOperand(1);
        Value *Replacement = nullptr;

        if (Op == Instruction::FRem) {
          Value *Div = Builder.CreateFDiv(LHS, RHS, "div");
          Value *Trunc = Builder.CreateUnaryIntrinsic(Intrinsic::trunc, Div);
          Value *Mul = Builder.CreateFMul(Trunc, RHS, "mul");
          Replacement = Builder.CreateFSub(LHS, Mul, "sub");
        } else if (Op == Instruction::SRem) {
          Value *Div = Builder.CreateSDiv(LHS, RHS, "div");
          Value *Mul = Builder.CreateMul(Div, RHS, "mul");
          Replacement = Builder.CreateSub(LHS, Mul, "sub");
        } else if (Op == Instruction::URem) {
          Value *Div = Builder.CreateUDiv(LHS, RHS, "div");
          Value *Mul = Builder.CreateMul(Div, RHS, "mul");
          Replacement = Builder.CreateSub(LHS, Mul, "sub");
        }

        BinOp->replaceAllUsesWith(Replacement);
        BinOp->eraseFromParent();
        Changed = true;
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "SimpleRemDecompose", "0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) -> bool {
                  if (Name == "kosolapov_v_decompose_remainder_fp_integer") {
                    FPM.addPass(KosolapovVDecRFPIPass{});
                    return true;
                  }
                  return false;
                });
          }};
}