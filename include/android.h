#pragma once

#ifdef ANDROID

#include <string>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <climits>
#include <cerrno>

namespace std
{

template <typename T>
string to_string(T value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

/*
inline unsigned long stoul (std::string const& str, size_t *idx = 0, int base = 10)
{
    char *endp;
    unsigned long value = strtoul(str.c_str(), &endp, base);
    if (endp == str.c_str())
        throw std::invalid_argument("my_stoul");

    if (value == ULONG_MAX && errno == ERANGE)
        throw std::out_of_range("my_stoul");

    if (idx)
        *idx = endp - str.c_str();

    return value;
}
*/
}// namespace std
#endif
