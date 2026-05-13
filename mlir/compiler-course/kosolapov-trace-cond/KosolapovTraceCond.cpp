#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"

    using namespace mlir;

namespace {

struct TracePair {
  StringRef enter;
  StringRef leave;
};

class TraceConditionPassKosolapov
    : public PassWrapper<TraceConditionPassKosolapov, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "kosolapov-trace-condition"; }
  StringRef getDescription() const final {
    return "Insert trace calls into conditional regions";
  }
  void runOnOperation() override {
    ModuleOp module = getOperation();
    initializeTraceFunctions(module);
    module.walk([&](Operation *op) {
      if (auto scfIf = dyn_cast<scf::IfOp>(op)) {
        instrumentConditionalRegion(scfIf.getThenRegion(), thenTrace);
        instrumentConditionalRegion(scfIf.getElseRegion(), elseTrace);
        return;
      }
      if (auto affineIf = dyn_cast<affine::AffineIfOp>(op)) {
        instrumentConditionalRegion(affineIf.getThenRegion(), thenTrace);
        instrumentConditionalRegion(affineIf.getElseRegion(), elseTrace);
      }
    });
  }

private:
  const TracePair thenTrace{"trace_condition_then_begin",
                            "trace_condition_then_end"};

  const TracePair elseTrace{"trace_condition_else_begin",
                            "trace_condition_else_end"};

  void initializeTraceFunctions(ModuleOp module) {
    MLIRContext *context = module.getContext();
    auto ensure = [&](StringRef name) {
      if (module.lookupSymbol<func::FuncOp>(name))
        return;
      OpBuilder builder(module.getBodyRegion());
      builder.setInsertionPointToStart(module.getBody());
      auto signature = FunctionType::get(context, {}, {});
      auto function =
          builder.create<func::FuncOp>(module.getLoc(), name, signature);
      function.setPrivate();
    };

    ensure(thenTrace.enter);
    ensure(thenTrace.leave);
    ensure(elseTrace.enter);
    ensure(elseTrace.leave);
  }

  void instrumentConditionalRegion(Region &region, const TracePair &trace) {
    if (region.empty())
      return;

    for (Block &block : region) {
      insertTraceCallAtEntry(block, trace.enter);
      insertTraceCallBeforeExit(block, trace.leave);
    }
  }

  void insertTraceCallAtEntry(Block &block, StringRef functionName) {
    OpBuilder builder(&block, block.begin());

    builder.create<func::CallOp>(block.getLoc(), functionName, TypeRange{},
                                 ValueRange{});
  }

  void insertTraceCallBeforeExit(Block &block, StringRef functionName) {
    Operation *terminator = block.getTerminator();
    if (!terminator)
      return;
    OpBuilder builder(terminator);
    builder.create<func::CallOp>(terminator->getLoc(), functionName,
                                 TypeRange{}, ValueRange{});
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TraceConditionPassKosolapov)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TraceConditionPassKosolapov)

static PassPluginLibraryInfo getTraceConditionPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TraceConditionPassKosolapov", "1.0",
          []() { PassRegistration<TraceConditionPassKosolapov>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  return getTraceConditionPluginInfo();
}
