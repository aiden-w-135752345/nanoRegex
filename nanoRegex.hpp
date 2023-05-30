#include <vector>
#include <string>
namespace nanoRegexImpl{class Node;class Frag;class Compiler;class Thread;class Captures;}
class NanoRegex {
    friend class nanoRegexImpl::Node;
    friend class nanoRegexImpl::Frag;
    friend class nanoRegexImpl::Compiler;
    friend class nanoRegexImpl::Thread;
    friend class nanoRegexImpl::Captures;

    struct State;
    typedef std::vector<State>::size_type StatePtr;
    struct State{
        enum Type{CHAR,ANY,MATCH,SPLIT,SAVE}type;
        StatePtr out;
        int gen=0;
        union{
            char c;
            StatePtr out1;
            size_t capture;
        };
        State(Type t,char cc):type(t),out(-1),c(cc){if(type!=CHAR)throw"wrong args";}
        State(Type t,size_t cc):type(t),out(-1),capture(cc){if(type!=SAVE)throw"wrong args";}
        State(Type t):type(t),out(-1){if(type!=ANY&&type!=SPLIT&&type!=MATCH)throw"wrong args";}
    };
    std::vector<State>states;
    StatePtr start;
    size_t numCaptures;
public:
    NanoRegex(char*);
    std::string::iterator* match(std::string);
    int captures()const{return numCaptures;}
private:
    template<typename... Args> StatePtr createState(Args... args);
    void patch(nanoRegexImpl::Frag&frag,StatePtr state);
    nanoRegexImpl::Frag build(nanoRegexImpl::Node* node);
};