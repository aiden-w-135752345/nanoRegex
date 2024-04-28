#include <cstddef>
#include <stdexcept>
#ifndef NANOREGEX_HPP
#define NANOREGEX_HPP
namespace NanoRegex_detail{
    struct UnoptimizedState;
    using StateIdx=std::size_t;
    struct init_struct;
}
class NanoRegex{
    using UnoptimizedState=NanoRegex_detail::UnoptimizedState;
    using StateIdx=NanoRegex_detail::StateIdx;
    using init_struct=NanoRegex_detail::init_struct;
    const UnoptimizedState*states;
    std::size_t numStates;
    bool shouldDelete;
    std::size_t numCaptures;
public:
    struct parse_exception:public std::invalid_argument{using std::invalid_argument::invalid_argument;};
    NanoRegex(const char*);
    const char** match(const char*,const char*)const;
    char** match(char*begin,char*end)const{
        return const_cast<char**>(match(
            static_cast<const char*>(begin),
            static_cast<const char*>(end)
        ));
    };
    int captures()const{return numCaptures;}
    ~NanoRegex();
    constexpr NanoRegex(const init_struct&init);
    template<typename T, T... chars>friend constexpr init_struct operator""_regex();
};
#endif