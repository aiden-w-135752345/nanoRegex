#include <vector>
#include <string>
namespace nanoRegexImpl{
    class Node;class Frag;class Compiler;class Thread;class Captures;
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
}
class NanoRegex {
    friend class nanoRegexImpl::Node;
    friend class nanoRegexImpl::Frag;
    friend class nanoRegexImpl::Compiler;
    friend class nanoRegexImpl::Thread;
    typedef nanoRegexImpl::State State;
    typedef nanoRegexImpl::StatePtr StatePtr;
    
    std::vector<State>states;
    StatePtr start;
    size_t numCaptures;
    int gen=1;
public:
    NanoRegex(char*);
    std::string::iterator* match(std::string);
    int captures()const{return numCaptures;}
private:
    template<typename... Args> StatePtr createState(Args... args);
    typedef nanoRegexImpl::Frag Frag;
    void patch(Frag&frag,StatePtr state);
    Frag build(nanoRegexImpl::Node* node);
    void addThread(StatePtr state,nanoRegexImpl::Captures*captures,std::vector<nanoRegexImpl::Thread>*l,std::string::iterator sp,int gen);
};