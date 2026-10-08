# Nevo

This repository contains two separate Nevo implementations:

- [`compiler/`](compiler/) is the v1.2 ARM64 compiler. It remains independent and is not part of the transpiler layout.
- [`transpiler/`](transpiler/) is the portable C transpiler, organized for separate macOS and Windows distribution work.

See the README in each implementation folder for its own build and development details. Historical releases, installers, and early source snapshots live under `transpiler/archive/`.