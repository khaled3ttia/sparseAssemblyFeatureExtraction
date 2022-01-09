; ModuleID = 'ex.c'
source_filename = "ex.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @another() #0 {
entry:
  %a = alloca [10 x i32], align 16
  %x = alloca i32, align 4
  %othercount = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 0, i32* %x, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, i32* %x, align 4
  %cmp = icmp slt i32 %0, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, i32* %x, align 4
  %2 = load i32, i32* %x, align 4
  %idxprom = sext i32 %2 to i64
  %arrayidx = getelementptr inbounds [10 x i32], [10 x i32]* %a, i64 0, i64 %idxprom
  store i32 %1, i32* %arrayidx, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %3 = load i32, i32* %x, align 4
  %inc = add nsw i32 %3, 1
  store i32 %inc, i32* %x, align 4
  br label %for.cond, !llvm.loop !4

for.end:                                          ; preds = %for.cond
  store i32 0, i32* %othercount, align 4
  store i32 0, i32* %y, align 4
  br label %for.cond1

for.cond1:                                        ; preds = %for.inc6, %for.end
  %4 = load i32, i32* %y, align 4
  %cmp2 = icmp slt i32 %4, 12
  br i1 %cmp2, label %for.body3, label %for.end8

for.body3:                                        ; preds = %for.cond1
  %5 = load i32, i32* %y, align 4
  %rem = srem i32 %5, 10
  %cmp4 = icmp eq i32 %rem, 1
  br i1 %cmp4, label %if.then, label %if.end

if.then:                                          ; preds = %for.body3
  br label %for.end8

if.end:                                           ; preds = %for.body3
  %6 = load i32, i32* %othercount, align 4
  %inc5 = add nsw i32 %6, 1
  store i32 %inc5, i32* %othercount, align 4
  br label %for.inc6

for.inc6:                                         ; preds = %if.end
  %7 = load i32, i32* %y, align 4
  %inc7 = add nsw i32 %7, 1
  store i32 %inc7, i32* %y, align 4
  br label %for.cond1, !llvm.loop !6

for.end8:                                         ; preds = %if.then, %for.cond1
  ret void
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %somecount = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 0, i32* %somecount, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, i32* %i, align 4
  %cmp = icmp slt i32 %0, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %1 = load i32, i32* %i, align 4
  %rem = srem i32 %1, 2
  %cmp1 = icmp eq i32 %rem, 0
  br i1 %cmp1, label %if.then, label %if.else

if.then:                                          ; preds = %for.body
  %2 = load i32, i32* %somecount, align 4
  %inc = add nsw i32 %2, 1
  store i32 %inc, i32* %somecount, align 4
  br label %if.end

if.else:                                          ; preds = %for.body
  %3 = load i32, i32* %somecount, align 4
  %dec = add nsw i32 %3, -1
  store i32 %dec, i32* %somecount, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  br label %for.inc

for.inc:                                          ; preds = %if.end
  %4 = load i32, i32* %i, align 4
  %inc2 = add nsw i32 %4, 1
  store i32 %inc2, i32* %i, align 4
  br label %for.cond, !llvm.loop !7

for.end:                                          ; preds = %for.cond
  ret i32 0
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 1}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"clang version 14.0.0 (https://github.com/llvm/llvm-project.git 446b27f74d80fee7dfeab0f077cba6eb20516cc5)"}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.mustprogress"}
!6 = distinct !{!6, !5}
!7 = distinct !{!7, !5}
