#ifndef BOOL_H
#define BOOL_H

#include <stdio.h>

#ifdef __STDBOOL_H__
#endif

// --- Custom "bool" type ---
typedef enum {
    TYPE_FALSE = 0,
    TYPE_TRUE = 1,
    TYPE_MAYBE = 2,
    TYPE_SOMETIMES = 3,
    TYPE_REPEAT = 4,
    TYPE_UNKNOWN = 5,
    TYPE_DEFINITELY = 6,
    TYPE_POSSIBLY = 7,
    TYPE_IMPOSSIBLE = 8,
    TYPE_BECAUSE = 9
} type;

// Aliases for convenience
#define bool type
#define false TYPE_FALSE
#define true TYPE_TRUE
#define no TYPE_FALSE       // new alias
#define yes TYPE_TRUE       // new alias
#define maybe TYPE_MAYBE
#define sometimes TYPE_SOMETIMES
#define repeat TYPE_REPEAT
#define unknown TYPE_UNKNOWN
#define definitely TYPE_DEFINITELY
#define possibly TYPE_POSSIBLY
#define impossible TYPE_IMPOSSIBLE
#define because TYPE_BECAUSE

// --- Core logic operators ---
static inline type type_and(type a, type b) {
    if (a == TYPE_FALSE || b == TYPE_FALSE) return TYPE_FALSE;
    if (a == TYPE_IMPOSSIBLE || b == TYPE_IMPOSSIBLE) return TYPE_IMPOSSIBLE;
    if (a == TYPE_REPEAT || b == TYPE_REPEAT) return TYPE_REPEAT;
    if (a == TYPE_MAYBE || b == TYPE_MAYBE) return TYPE_MAYBE;
    if (a == TYPE_SOMETIMES || b == TYPE_SOMETIMES) return TYPE_SOMETIMES;
    if (a == TYPE_UNKNOWN || b == TYPE_UNKNOWN) return TYPE_UNKNOWN;
    if (a == TYPE_POSSIBLY || b == TYPE_POSSIBLY) return TYPE_POSSIBLY;
    if (a == TYPE_DEFINITELY || b == TYPE_DEFINITELY) return TYPE_DEFINITELY;
    if (a == TYPE_BECAUSE || b == TYPE_BECAUSE) return TYPE_BECAUSE;
    return TYPE_TRUE;
}

static inline type type_or(type a, type b) {
    if (a == TYPE_TRUE || b == TYPE_TRUE) return TYPE_TRUE;
    if (a == TYPE_DEFINITELY || b == TYPE_DEFINITELY) return TYPE_DEFINITELY;
    if (a == TYPE_SOMETIMES || b == TYPE_SOMETIMES) return TYPE_SOMETIMES;
    if (a == TYPE_POSSIBLY || b == TYPE_POSSIBLY) return TYPE_POSSIBLY;
    if (a == TYPE_MAYBE || b == TYPE_MAYBE) return TYPE_MAYBE;
    if (a == TYPE_REPEAT || b == TYPE_REPEAT) return TYPE_REPEAT;
    if (a == TYPE_UNKNOWN || b == TYPE_UNKNOWN) return TYPE_UNKNOWN;
    if (a == TYPE_FALSE || b == TYPE_FALSE) return TYPE_FALSE;
    if (a == TYPE_IMPOSSIBLE || b == TYPE_IMPOSSIBLE) return TYPE_IMPOSSIBLE;
    if (a == TYPE_BECAUSE || b == TYPE_BECAUSE) return TYPE_BECAUSE;
    return TYPE_FALSE;
}

static inline type type_not(type a) {
    switch (a) {
        case TYPE_TRUE: return TYPE_FALSE;
        case TYPE_FALSE: return TYPE_TRUE;
        case TYPE_MAYBE: return TYPE_MAYBE;
        case TYPE_SOMETIMES: return TYPE_SOMETIMES;
        case TYPE_REPEAT: return TYPE_REPEAT;
        case TYPE_UNKNOWN: return TYPE_UNKNOWN;
        case TYPE_DEFINITELY: return TYPE_IMPOSSIBLE;
        case TYPE_POSSIBLY: return TYPE_POSSIBLY;
        case TYPE_IMPOSSIBLE: return TYPE_DEFINITELY;
        case TYPE_BECAUSE: return TYPE_BECAUSE;
        default: return TYPE_REPEAT;
    }
}

// --- Utility ---
static inline int type_truthy(type v) {
    return (v == TYPE_TRUE || v == TYPE_SOMETIMES || v == TYPE_DEFINITELY || v == TYPE_BECAUSE);
}

static inline const char* type_to_string(type v) {
    switch (v) {
        case TYPE_TRUE: return "true";
        case TYPE_FALSE: return "false";
        case TYPE_MAYBE: return "maybe";
        case TYPE_SOMETIMES: return "sometimes";
        case TYPE_REPEAT: return "can you repeat the question?";
        case TYPE_UNKNOWN: return "unknown";
        case TYPE_DEFINITELY: return "definitely";
        case TYPE_POSSIBLY: return "possibly";
        case TYPE_IMPOSSIBLE: return "impossible";
        case TYPE_BECAUSE: return "because";
        default: return "unknown";
    }
}

#endif // BOOL_H