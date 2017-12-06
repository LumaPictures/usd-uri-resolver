#include <z85/z85.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return -1;
    }
    std::cout << z85::encode_with_padding(std::string(argv[1]));
    return 0;
}
