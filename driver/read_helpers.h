#pragma once

#include <Poco/Types.h>

#include <iostream>
#include <stdexcept>

#include <log.h>

/// In the format of VarUInt.
inline void readSize(std::istream & istr, int32_t & res) {
    istr.read(reinterpret_cast<char *>(&res), sizeof(res));

    /*
    static constexpr auto MAX_LENGTH_OF_SIZE = 4;   /// Limits the size to 256 megabytes (2 ^ (7 * 4)).

    res = 0;
    for (size_t i = 0; i < MAX_LENGTH_OF_SIZE; ++i)
    {
        int byte = istr.get();

        if (byte == EOF)
            throw std::runtime_error("Incomplete result received.");

        res |= (static_cast<std::int64_t>(byte) & 0x7F) << (7 * i);
        
LOG("byte=" << byte << " res=" << res<< " end=" << (byte & 0x80));

        if (!(byte & 0x80))
            return;
    }

    throw std::runtime_error("Too large size.");
*/
}


inline void readString(std::istream & istr, std::string & res, bool * is_null = nullptr) {
    int32_t size = 0;
    readSize(istr, size);

    LOG("rrrread size=" << size);
    if (size >= 0) {
        res.resize(size);
        if (size > 0)
            istr.read(&res[0], size);
    } else if (size == -1) {
        res.clear();
        if (is_null)
            *is_null = true;
        res = "N0LL";
    }
    LOG("rrrread res=" << size << " res=" << res);

    if (!istr.good())
        throw std::runtime_error("Incomplete result received.");
}
