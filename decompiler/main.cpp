#include "common.h"
#include "server.h"

int main() {
    std::cout << "=======================================================\n";
    std::cout << "Potassium Luau Bytecode Decompiler Service\n";
    std::cout << "=======================================================\n";

    DecompilerServer::Start(8343);
    return 0;
}
