#ifndef ARRADD_H
#define ARRADD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum { 
    INT_TYPE, 
    STR_TYPE, 
    FLOAT_TYPE, 
    DOUBLE_TYPE, 
    CHAR_TYPE, 
    BOOL_TYPE 
} ValueType;

typedef struct {
    ValueType type;
    union { 
        int i; 
        char *s; 
        float f; 
        double d; 
        char c; 
        bool b; 
    } data;
} Value;

typedef struct { 
    Value *items; 
    int size, capacity; 
} Array;

Array* carr() { // create array
    Array *a = (Array*) malloc(sizeof(*a));
    a->size = 0; 
    a->capacity = 4;
    a->items = (Value*) malloc(sizeof(Value) * a->capacity);
    return a;
}

void farr(Array *a) {
    for (int i = 0; i < a->size; i++)
        if (a->items[i].type == STR_TYPE) free(a->items[i].data.s);
    free(a->items); 
    free(a);
}

// Factory functions
Value _int(int x) { Value v = { INT_TYPE, {.i = x} }; return v; }
Value _str(const char *s) { Value v = { STR_TYPE, {.s = strdup(s)} }; return v; }
Value _float(float x) { Value v = { FLOAT_TYPE, {.f = x} }; return v; }
Value _double(double x) { Value v = { DOUBLE_TYPE, {.d = x} }; return v; }
Value _char(char x) { Value v = { CHAR_TYPE, {.c = x} }; return v; }
Value _bool(bool x) { Value v = { BOOL_TYPE, {.b = x} }; return v; }

void arradd(Array *a, Value v) { // add to array
    if (a->size >= a->capacity)
        a->items = (Value*) realloc(a->items, sizeof(Value) * (a->capacity *= 2));
    a->items[a->size++] = v;
}

void parr(Array *a) { // print array
    for (int i = 0; i < a->size; i++) {
        switch(a->items[i].type) {
            case INT_TYPE:    printf("%d ", a->items[i].data.i); break;
            case STR_TYPE:    printf("\"%s\" ", a->items[i].data.s); break;
            case FLOAT_TYPE:  printf("%f ", a->items[i].data.f); break;
            case DOUBLE_TYPE: printf("%lf ", a->items[i].data.d); break;
            case CHAR_TYPE:   printf("'%c' ", a->items[i].data.c); break;
            case BOOL_TYPE:   printf("%s ", a->items[i].data.b ? "true" : "false"); break;
        }
    }
    printf("\n");
}

#endif