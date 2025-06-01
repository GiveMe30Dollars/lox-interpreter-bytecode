
#include <iostream>

int main(int argc, char *argv[]) {
    // Disable output buffering
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "Hello World!\n";
    return 0;
}