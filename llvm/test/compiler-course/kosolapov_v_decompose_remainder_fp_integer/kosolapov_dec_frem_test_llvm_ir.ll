; RUN: opt -load-pass-plugin %llvmshlibdir/kosolapov_v_decompose_remainder_fp_integer_LLVM_IR%pluginext \
; RUN: -passes=kosolapov_v_decompose_remainder_fp_integer -S %s | FileCheck %s

; CHECK-LABEL: define double @test_frem(double %a, double %b)
; CHECK-NEXT:    %div = fdiv double %a, %b
; CHECK-NEXT:    [[TRUNC:%[0-9]+]] = call double @llvm.trunc.f64(double %div)
; CHECK-NEXT:    %mul = fmul double [[TRUNC]], %b
; CHECK-NEXT:    %sub = fsub double %a, %mul
; CHECK-NEXT:    ret double %sub
define double @test_frem(double %a, double %b) {
  %r = frem double %a, %b
  ret double %r
}

; CHECK-LABEL: define i32 @test_srem(i32 %a, i32 %b)
; CHECK-NEXT:    %div = sdiv i32 %a, %b
; CHECK-NEXT:    %mul = mul i32 %div, %b
; CHECK-NEXT:    %sub = sub i32 %a, %mul
; CHECK-NEXT:    ret i32 %sub
define i32 @test_srem(i32 %a, i32 %b) {
  %r = srem i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: define i32 @test_urem(i32 %a, i32 %b)
; CHECK-NEXT:    %div = udiv i32 %a, %b
; CHECK-NEXT:    %mul = mul i32 %div, %b
; CHECK-NEXT:    %sub = sub i32 %a, %mul
; CHECK-NEXT:    ret i32 %sub
define i32 @test_urem(i32 %a, i32 %b) {
  %r = urem i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: define <2 x double> @test_frem_vector(<2 x double> %a, <2 x double> %b)
; CHECK-NEXT:    %div = fdiv <2 x double> %a, %b
; CHECK-NEXT:    [[TRUNC:%[0-9]+]] = call <2 x double> @llvm.trunc.v2f64(<2 x double> %div)
; CHECK-NEXT:    %mul = fmul <2 x double> [[TRUNC]], %b
; CHECK-NEXT:    %sub = fsub <2 x double> %a, %mul
; CHECK-NEXT:    ret <2 x double> %sub
define <2 x double> @test_frem_vector(<2 x double> %a, <2 x double> %b) {
  %r = frem <2 x double> %a, %b
  ret <2 x double> %r
}