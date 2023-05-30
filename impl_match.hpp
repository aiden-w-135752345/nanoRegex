#include <vector>
#include <string>
#include <list>
#include <utility>
#include "nanoRegex.hpp"
namespace nanoRegexImpl{
    static NanoRegex* re;
    static int gen=1;
    struct Thread;
    struct Captures{
        size_t refs;std::string::iterator values[1];
        void decref(){if(--refs==0){free(this);}}
        static Captures*create(size_t numCaptures){
            Captures* caps=(Captures*)malloc(sizeof(Captures)+((ssize_t)numCaptures-1)*(ssize_t)sizeof(std::string::iterator));
            caps->refs=1;
            return caps;
        }
        Captures*update(size_t idx,std::string::iterator val,size_t numCaptures)const{
            Captures*edited=create(numCaptures);
            for(size_t i=0;i<numCaptures;i++)edited->values[i]=values[i];
            edited->values[idx]=val;
            return edited;
        }
    };
    struct Thread{
        typedef NanoRegex::StatePtr StatePtr;
        typedef NanoRegex::State State;
        StatePtr state;
        Captures*captures;
        Thread(StatePtr s,Captures*c):state(s),captures(c){}
        static void add(StatePtr state,Captures*captures,std::vector<Thread>*l,std::string::iterator sp,int gen) {
            if(re->states[state].gen==gen){return;}
            re->states[state].gen=gen;
            if(re->states[state].type == State::SPLIT){
                captures->refs+=2;
                Thread::add(re->states[state].out,captures,l,sp,gen);
                Thread::add(re->states[state].out1,captures,l,sp,gen);
                return;
            }
            if(re->states[state].type == State::SAVE){
                Thread::add(re->states[state].out,captures->update(re->states[state].capture,sp,re->numCaptures),l,sp,gen);
                return;
            }
            captures->refs++;
            l->emplace_back(state,captures);
        }
    };
};