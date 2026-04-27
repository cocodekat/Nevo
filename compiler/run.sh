clang compiler.c errors.c -o compiler
clang transpiler.c -o transpiler
./compiler test.n out.s
clang out.s -o test
./test