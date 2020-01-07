# Tiger compiler

This repo contains my code for 2017 SE302 compiler course. Being divided int 6 separate assignments, it implements a compiler for the tiger language described in [Modern Compiler Implementation in C](https://www.cs.princeton.edu/~appel/modern/c/). The main difference is that this one is implemented in C++ and supports X86-64 architecture.

- [x] lab1: A simple straightline code analyzer & interpreter. Not part of the compiler.
- [x] lab2: Lexical analysis using `flexc++`. See `src/lex`.
- [x] lab3: Language parser using `bisonc++`. See `src/parse`.
- [x] lab4: Semantic analysis. See `src/semant`.
- [x] lab5: IR translation and code generation. See `src/frame`, `src/translate`, `src/codegen`.
- [x] lab6: Register allocation. See `src/regalloc`.
