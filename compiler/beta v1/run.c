#include <stdio.h>

extern int entry(void);   // declare the assembly function

int main() {
    int result = entry();  // call assembly function
    printf("Reg w0 =  %d\n", result);
    return 0;
}