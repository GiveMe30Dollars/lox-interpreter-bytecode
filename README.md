# template-cpp  
A personal template repository for C++ projects.

## Usage  
Run from Git Bash (or any command-line interface that executes .sh files):  
  - `./program.sh [command-line arguments]...`
    
This will compile and run the project as executable `main.exe`.  
`program.sh` and `main.exe` may be renamed based on your project.  

## Dependencies  
To compile and run the project, the following dependencies are required:
- *Any C++ Compiler.*
- **CMake**: https://cmake.org/
- **vcpkg**: https://vcpkg.io/en/
  - This directory should be cloned onto your local machine.
  - The system environmental path `VCPKG_ROOT` should be set to the root of this directory.
- *Any command-line interface that executes `.sh` files.*
  - For Windows and macOS: **Git Bash**: https://git-scm.com/downloads
  - For Linux: *None.* Natively supported.  
