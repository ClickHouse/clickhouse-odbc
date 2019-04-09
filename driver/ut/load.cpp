#include <iostream>
#include <Poco/SharedLibrary.h>
#include "../platform.h"

int main(int argc, char * argv[]) {
    if (argc < 2)
        return 1;
    const auto & endl = "\n";
    {
        std::cout << " Loading " << argv[1] << endl;
        Poco::SharedLibrary lib(argv[1]);

        std::string func_name = argc < 3 ? "SQLDummyOrdinal" : argv[2];
        auto has_symbol = lib.hasSymbol(func_name);
        std::cout << " hasSymbol(" << func_name << ") = " << has_symbol << endl;
        if (!has_symbol)
            return 2;
        auto symbol = lib.getSymbol(func_name);
        std::cout << " symbol = " << symbol << ". " << endl;
        RETCODE (*f)(void);
        f = reinterpret_cast<decltype(f)>(symbol);
        f();
        std::cout << " Run ok. " << endl;

        lib.unload();
        std::cout << " Unload ok. " << endl;
    }
    std::cout << " Destruct ok. " << endl;
    return 0;
}
