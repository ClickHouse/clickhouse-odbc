#pragma once

#include <iostream>

void readSize(std::istream & istr, int32_t & res);
void readString(std::istream & istr, std::string & res, bool * is_null = nullptr);
