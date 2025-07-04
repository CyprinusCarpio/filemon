#include "Filemon.hpp"

#include <FLE/Fle_Colors.hpp>
#include <FLE/Fle_Schemes.hpp>

int main(int argc, char* argv[]) 
{
    Filemon filemon(800, 600, "Filemon");
    filemon.show(argc, argv);

    fle_set_colors("dark2");
    fle_set_scheme("fle_scheme2");

    //fle_set_colors("light");
    //fle_set_scheme("oxy");

    return filemon.run();
}