# BRANCH: metaclass

This branch implements expression-based class inheritance.

Some adherence to the Lox specification is lost as a result: inheritance from objects that are not classes or arrays of classes, or from itself, is a runtime error.

Will be behind branch `containers`, which is the preferred experimental branch.

## Bytecode-Based Lox Interpreter

This is an ongoing C implemetation of the Lox interpreter as described in the second half of the book:  
- **"Crafting Interpreters"**: https://craftinginterpreters.com/ by Robert Nystrom.

Sister project to the AST-based Lox Interpreter: https://github.com/GiveMe30Dollars/lox-interpreter-ast

While the code is written in C, it is written to be forward-compatible with the C++ compiler and may be compiled as a C++ project.

For further information, check the [documentation](docs).

### Timeline

1st June 2025: Initialized.

11 June 2025: Milestone achieved! This Lox implementation can now run the recursive Fibonacci function.  
  Speed of execution is 150x improved compared to the AST-based interpreter.
<p align="center">
<img src=https://github.com/user-attachments/assets/5e172e18-70e8-4ae0-b999-658f90d3c559 />
</p>

17 June 2025: Milestone achieved! All standard Lox functionality implemented.

**Current status:** Implemented global and local variables, control flow, functions and closures.  
Additionally added ternary conditional (+ Elvis operator), compound assignment operators, break and continue statements for loops, anonymous function expressions.

## Usage  
Run from Git Bash (or any command-line interface that executes .sh files):  
  - `./lox.sh`: opens in REPL mode.
  - `./lox.sh [path]`: opens the plaintext file at `path` and executes it as a Lox program.
    
This will compile and run the project as executable `main.exe`.  

## Dependencies  
To compile and run the project, the following dependencies are required:
- *Any C/C++ Compiler.*
- **CMake**: https://cmake.org/
- **vcpkg**: https://vcpkg.io/en/
  - This directory should be cloned onto your local machine.
  - The system environmental path `VCPKG_ROOT` should be set to the root of this directory.
- *Any command-line interface that executes `.sh` files.*
  - For Windows: **Git Bash**: https://git-scm.com/downloads
  - For Linux and macOS: *None.* Natively supported.  
