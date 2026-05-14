// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/kosolapov-trace-cond_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(kosolapov-trace-condition)" %s | FileCheck %s

// CHECK-DAG: func.func private @trace_condition_then_begin()
// CHECK-DAG: func.func private @trace_condition_then_end()
// CHECK-DAG: func.func private @trace_condition_else_begin()
// CHECK-DAG: func.func private @trace_condition_else_end()


// CHECK-LABEL: func.func @simple_if
func.func @simple_if(%cond: i1) {
  // CHECK: scf.if
  scf.if %cond {
    // CHECK: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    scf.yield
  } else {
    // CHECK: func.call @trace_condition_else_begin()
    %c2 = arith.constant 2 : i32
    // CHECK: func.call @trace_condition_else_end()
    scf.yield
  }
  return
}


// CHECK-LABEL: func.func @if_without_else
func.func @if_without_else(%flag: i1) {
  scf.if %flag {
    // CHECK: func.call @trace_condition_then_begin()
    %v = arith.constant 7 : i32
    // CHECK: func.call @trace_condition_then_end()
    scf.yield
  }
  return
}


// CHECK-LABEL: func.func @nested_if
func.func @nested_if(%a: i1, %b: i1) {
  scf.if %a {
    // CHECK: func.call @trace_condition_then_begin()
    scf.if %b {
      // CHECK: func.call @trace_condition_then_begin()
      %x = arith.constant 10 : i32
      // CHECK: func.call @trace_condition_then_end()
      scf.yield
    } else {
      // CHECK: func.call @trace_condition_else_begin()
      %y = arith.constant 20 : i32
      // CHECK: func.call @trace_condition_else_end()
      scf.yield
    }
    // CHECK: func.call @trace_condition_then_end()
    scf.yield
  } else {
    // CHECK: func.call @trace_condition_else_begin()
    %z = arith.constant 30 : i32
    // CHECK: func.call @trace_condition_else_end()
    scf.yield
  }
  return
}
#affine_test = affine_set<(d0) : (d0 - 3 >= 0)>

// CHECK-LABEL: func.func @affine_if_case
func.func @affine_if_case(%idx: index) {
  affine.if #affine_test(%idx) {
    // CHECK: func.call @trace_condition_then_begin()
    %a = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
  } else {
    // CHECK: func.call @trace_condition_else_begin()
    %b = arith.constant 0 : i32
    // CHECK: func.call @trace_condition_else_end()
  }
  return
}



// CHECK-LABEL: func.func @existing_symbols
func.func @existing_symbols(%cond: i1) {
  scf.if %cond {
    // CHECK: func.call @trace_condition_then_begin()
    %t = arith.constant 5 : i32
    // CHECK: func.call @trace_condition_then_end()
    scf.yield
  }
  return
}
