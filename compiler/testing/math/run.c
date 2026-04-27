#include <stdio.h>

extern int var(void);   // declare the assembly function

int main() {
    int result = var();  // call assembly function
    printf("Variable value = %d\n", result);
    return 0;
}
