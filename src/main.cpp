#include "Filemon.hpp"

int main(int argc, char* argv[]) 
{
    Filemon filemon(800, 600, "Filemon");
    filemon.show(argc, argv);

    return filemon.run();
}