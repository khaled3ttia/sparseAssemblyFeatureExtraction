#clang -c -emit-llvm -S ex.c -o ex.ll
#clang -O1 -c -emit-llvm -S main.c -o main.ll
#opt -load build/lib/StatsCount.so --stCounter --enable-new-pm=0 -disable-output main.ll
#bash ~/Documents/anl/llvm-project/polly/test/create_ll.sh main.c
#clang -O0 -c -emit-llvm -Xclang -disable-O0-optnone main.c -o main.bc

#clang -O1 -c -emit-llvm -S main.c -o main.ll
clang -O0 -fno-inline -c -emit-llvm  -Xclang -disable-O0-optnone main.c -o main.bc
opt -load build/lib/StatsCount.so -mem2reg -loop-rotate -scalar-evolution -stCounter --enable-new-pm=0 -disable-output main.bc
