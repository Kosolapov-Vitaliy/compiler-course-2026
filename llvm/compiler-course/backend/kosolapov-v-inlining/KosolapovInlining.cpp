#include "X86.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include <unordered_map>

using namespace llvm;

#define DEBUG_TYPE "kosolapov-inlining"

namespace {

class KosolapovInlining : public MachineFunctionPass {
public:
  static char ID;
  KosolapovInlining() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  static constexpr unsigned MaxInstrCount = 15;
  static constexpr unsigned MaxDepth = 3;

  std::unordered_map<const Function *, unsigned> Depth;

  bool tryInline(MachineInstr &MI, MachineFunction &CallerMF);
  bool canInline(const MachineFunction &MF);
  void doInline(MachineInstr &CallMI, MachineFunction &CalleeMF,
                MachineFunction &CallerMF);
};

} // namespace

char KosolapovInlining::ID = 0;

bool KosolapovInlining::runOnMachineFunction(MachineFunction &MF) {
  bool Changed;

  do {
    Changed = false;

    for (auto &BB : MF) {
      for (auto It = BB.begin(); It != BB.end();) {
        MachineInstr &MI = *It++;

        if (MI.isCall()) {
          if (tryInline(MI, MF)) {
            Changed = true;
            break;
          }
        }
      }
      if (Changed)
        break;
    }
  } while (Changed);

  return true;
}

bool KosolapovInlining::tryInline(MachineInstr &MI, MachineFunction &CallerMF) {
  const MachineOperand &Op = MI.getOperand(0);
  if (!Op.isGlobal())
    return false;

  const Function *F = dyn_cast<Function>(Op.getGlobal());
  if (!F || F == &CallerMF.getFunction())
    return false;

  if (Depth[F] >= MaxDepth)
    return false;

  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  MachineFunction *CalleeMF = MMI.getMachineFunction(*F);
  if (!CalleeMF || CalleeMF->empty())
    return false;

  if (!canInline(*CalleeMF))
    return false;

  Depth[F]++;
  doInline(MI, *CalleeMF, CallerMF);
  Depth[F]--;
  return true;
}

bool KosolapovInlining::canInline(const MachineFunction &MF) {
  if (MF.size() > 1)
    return false;
  unsigned Cnt = 0;

  for (auto &BB : MF)
    for (auto &MI : BB)
      if (!MI.isDebugInstr() && !MI.isReturn())
        if (++Cnt > MaxInstrCount)
          return false;

  return true;
}

void KosolapovInlining::doInline(MachineInstr &CallMI,
                                 MachineFunction &CalleeMF,
                                 MachineFunction &CallerMF) {
  MachineBasicBlock *CallBB = CallMI.getParent();
  auto InsertPt = CallMI.getIterator();

  MachineRegisterInfo &CallerMRI = CallerMF.getRegInfo();
  MachineRegisterInfo &CalleeMRI = CalleeMF.getRegInfo();

  DenseMap<Register, Register> VRegMap;

  for (auto &BB : CalleeMF) {
    for (auto &MI : BB) {
      if (MI.isReturn())
        continue;

      MachineInstr *NewMI = CallerMF.CloneMachineInstr(&MI);

      for (auto &Op : NewMI->operands()) {
        if (!Op.isReg() || !Op.getReg().isVirtual())
          continue;
        Register OldReg = Op.getReg();
        if (!VRegMap.count(OldReg)) {
          const TargetRegisterClass *RC = CalleeMRI.getRegClass(OldReg);
          Register NewReg = CallerMRI.createVirtualRegister(RC);
          VRegMap[OldReg] = NewReg;
        }
        Op.setReg(VRegMap[OldReg]);
      }

      CallBB->insert(InsertPt, NewMI);
    }
  }

  CallMI.eraseFromParent();
}

static RegisterPass<KosolapovInlining>
    X("kosolapov-inlining", "Kosolapov Inlining Pass", false, false);