#ifndef BETTER_OPS_H
#define BETTER_OPS_H

#define AND &&
#define OR ||
#define NOT !
#define XOR(a, b) ((a) && !(b) || !(a) && (b))

// --- Derived / composite gates ---
#define NAND(a, b) (!(AND(a, b)))
#define NOR(a, b)  (!(OR(a, b)))
#define XNOR(a, b) (!(XOR(a, b)))

// --- Ternary / uncommon gates ---
#define IMPLIES(a, b)  (!(a) || (b))           // a → b
#define NIMPLIES(a, b) ((a) && !(b))           // a ↛ b
#define CONVERSE_IMPLIES(a, b) (!(b) || (a))   // b → a
#define BICONDITIONAL(a, b) (!(XOR(a, b)))     // a ↔ b
#define NOT_BICONDITIONAL(a, b) (XOR(a, b))    // XOR alias

// --- Custom / utility logic gates ---
#define BUFFER(a) (a)                          // output = input
#define INVERT(a) (!(a))                       // alias for NOT

#endif // BETTER_OPS_H
