
#include "md4_hash.hpp"

namespace libed2k{



const md4_hash::md4hash_container md4_hash::m_emptyMD4Hash = 
{'\x31', '\xD6', '\xCF', '\xE0', '\xD1', '\x6A', '\xE9', '\x31' , '\xB7', '\x3C', '\x59', '\xD7', '\xE0', '\xC0', '\x89', '\xC0' };


std::ostream& operator<< (std::ostream& stream, const md4_hash& hash)
{

    for (size_t n = 0; n < sizeof(md4_hash::md4hash_container); n++)
    {
        stream << std::hex << hash.m_hash[n];
    }

    return (stream);
}

}
