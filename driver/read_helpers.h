#pragma once

#include <Poco/Types.h>

#include <iostream>
#include <stdexcept>

#include <log.h>

/// In the format of VarUInt.
inline void readSize(std::int64_t & res, std::istream & istr)
{
    static constexpr auto MAX_LENGTH_OF_SIZE = 4;   /// Limits the size to 256 megabytes (2 ^ (7 * 4)).

    res = 0;
    for (size_t i = 0; i < MAX_LENGTH_OF_SIZE; ++i)
    {
        int byte = istr.get();

        if (byte == EOF)
            throw std::runtime_error("Incomplete result received.");

        res |= (static_cast<std::int64_t>(byte) & 0x7F) << (7 * i);

        if (!(byte & 0x80))
            return;
    }

    throw std::runtime_error("Too large size.");
}


inline void readString(std::string & res, std::istream & istr)
{
    std::int64_t size = 0;
    readSize(size, istr);

    res.resize(size);
    istr.read(&res[0], size);

LOG("rrrread size=" << size << " res=" << res);

    if (!istr.good())
        throw std::runtime_error("Incomplete result received.");
}
