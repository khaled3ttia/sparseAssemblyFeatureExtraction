rm -rf build 
mkdir build 
cd build 
cmake .. 
make 
cd ..

#opt -load build/lib/StatsCount.so --stCounter --enable-new-pm=0 -disable-output main.ll
clang -O0 -fno-inline -c -emit-llvm  -Xclang -disable-O0-optnone main.c -o main.bc
opt -load build/lib/StatsCount.so -mem2reg -loop-rotate -scalar-evolution -stCounter --enable-new-pm=0 -disable-output main.bc
