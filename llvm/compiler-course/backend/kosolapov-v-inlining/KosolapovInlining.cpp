#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include <map>

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
  static const unsigned MaxInstrCount = 15;
  static const unsigned MaxDepth = 3;

  DenseMap<const Function *, unsigned> RecDepth;

  bool tryInline(MachineInstr &CallMI, MachineFunction &CallerMF);
  bool canInline(const MachineFunction &CalleeMF) const;
  void performInlining(MachineInstr &CallMI, MachineFunction &CalleeMF,
                       MachineFunction &CallerMF);
};

} // namespace

char KosolapovInlining::ID = 0;

bool KosolapovInlining::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  bool LocalChanged;
  do {
    LocalChanged = false;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : make_early_inc_range(MBB)) {
        unsigned Opc = MI.getOpcode();
        if (Opc == X86::CALL64pcrel32 || Opc == X86::CALL64r ||
            Opc == X86::CALL64m) {
          if (tryInline(MI, MF)) {
            LocalChanged = true;
            break;
          }
        }
      }
      if (LocalChanged)
        break;
    }
    Changed |= LocalChanged;
  } while (LocalChanged);
  return Changed;
}

bool KosolapovInlining::tryInline(MachineInstr &CallMI,
                                  MachineFunction &CallerMF) {
  const MachineOperand &TargetOp = CallMI.getOperand(0);
  if (!TargetOp.isGlobal())
    return false;
  const GlobalValue *GV = TargetOp.getGlobal();
  if (!GV)
    return false;
  const Function *CalleeFunc = dyn_cast<Function>(GV);
  if (!CalleeFunc)
    return false;

  if (RecDepth[CalleeFunc] >= MaxDepth)
    return false;

  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  MachineFunction *CalleeMF = MMI.getMachineFunction(*CalleeFunc);
  if (!CalleeMF)
    return false;

  if (CalleeMF->size() != 1)
    return false;

  if (!canInline(*CalleeMF))
    return false;

  ++RecDepth[CalleeFunc];
  performInlining(CallMI, *CalleeMF, CallerMF);
  --RecDepth[CalleeFunc];

  return true;
}

bool KosolapovInlining::canInline(const MachineFunction &CalleeMF) const {
  unsigned InstrCount = 0;
  for (const MachineBasicBlock &MBB : CalleeMF) {
    for (const MachineInstr &MI : MBB) {
      if (!MI.isDebugInstr())
        ++InstrCount;
      if (InstrCount > MaxInstrCount)
        return false;
    }
  }
  return InstrCount <= MaxInstrCount;
}

void KosolapovInlining::performInlining(MachineInstr &CallMI,
                                        MachineFunction &CalleeMF,
                                        MachineFunction &CallerMF) {
  assert(CalleeMF.size() == 1 && "Only single-block callees are supported");

  MachineBasicBlock *CallMBB = CallMI.getParent();
  MachineBasicBlock::iterator InsertPos = CallMI.getIterator();
  MachineBasicBlock &CalleeEntry = *CalleeMF.begin();

  MachineRegisterInfo &CallerMRI = CallerMF.getRegInfo();
  const MachineRegisterInfo &CalleeMRI = CalleeMF.getRegInfo();
  DenseMap<Register, Register> VRegMap;

  SmallVector<MachineInstr *, 16> ToClone;
  for (MachineInstr &MI : CalleeEntry) {
    if (MI.isReturn())
      continue;
    ToClone.push_back(&MI);
  }

  for (MachineInstr *SrcMI : ToClone) {
    MachineInstr *NewMI = CallerMF.CloneMachineInstr(SrcMI);

    for (MachineOperand &MO : NewMI->operands()) {
      if (!MO.isReg())
        continue;
      Register R = MO.getReg();
      if (!R.isVirtual())
        continue;

      auto It = VRegMap.find(R);
      if (It == VRegMap.end()) {
        const TargetRegisterClass *RC = CalleeMRI.getRegClass(R);
        Register NewR = CallerMRI.createVirtualRegister(RC);
        It = VRegMap.insert({R, NewR}).first;
      }
      MO.setReg(It->second);
    }

    CallMBB->insert(InsertPos, NewMI);
  }

  CallMI.eraseFromParent();
}

static RegisterPass<KosolapovInlining>
    X("kosolapov-inlining", "Kosolapov Inlining Pass (fixed)", false, false);