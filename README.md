# Bytecode-Based Lox Interpreter

This is an ongoing C implemetation of the Lox interpreter as described in the second half of the book:  
- **"Crafting Interpreters"**: https://craftinginterpreters.com/ by Robert Nystrom.

Sister project to the AST-based Lox Interpreter:
- https://github.com/GiveMe30Dollars/lox-interpreter-ast

While the code is written in C, it is forwards-compatible with the C++ compiler and may be compiled as a C++ project.

Initialized 1st June 2025.

## Usage  
Run from Git Bash (or any command-line interface that executes .sh files):  
  - `./program.sh [command-line arguments]...`
    
This will compile and run the project as executable `main.exe`.  
`program.sh` and `main.exe` may be renamed based on your project.  

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
