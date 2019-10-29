#include "read_helpers.h"

#include <stdexcept>
#include <string>
#include <iostream>

void readSize(std::istream & istr, int32_t & res) {
    istr.read(reinterpret_cast<char *>(&res), sizeof(res));
}

void readString(std::istream & istr, std::string & res, bool * is_null) {
    int32_t size = 0;
    readSize(istr, size);

    if (size >= 0) {
        res.resize(size);
        if (size > 0)
            istr.read(&res[0], size);
    } else if (size == -1) {
        res.resize(0);
        if (is_null)
            *is_null = true;
    }

    if (!istr.good())
        throw std::runtime_error("Incomplete result received. Want size=" + std::to_string(size) + ".");
}
