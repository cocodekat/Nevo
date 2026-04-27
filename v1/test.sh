clang compiler.c errors.c -o compiler
./compiler test.n out.s
clang out.s -o test
./test
