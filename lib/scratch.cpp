#include <bitset>

enum class AnEnum { A,B,C};


template <typename ENUM, size_t N>
class enum_bitset: public std::bitset<N> {
public:
    using base=std::bitset<N>;

    void set(ENUM index,bool value) { base::set((int)index,value)}
    void set(ENUM index) { set(index,true);}
    void reset(ENUM index) { set(index,false);}
    bool test(ENUM index) const { return base::operator[]((int)index);}

    bool operator[](ENUM index) const { return test(index);}
};

class enum_bitset<AnEnum> t;

inline void test()
{
    using EnumBitset = enum_bitset<AnEnum,3>;

    EnumBitset a;
    EnumBitset b;

    a & b;
    auto i = a | b;
}
