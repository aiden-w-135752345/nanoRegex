#include <vector>
#include <string>
#include <list>
#include <utility>
#include "nanoRegex.hpp"
namespace nanoRegexImpl{
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
        StatePtr state;
        Captures*captures;
        Thread(StatePtr s,Captures*c):state(s),captures(c){}
    };
}
using namespace nanoRegexImpl;
void NanoRegex::addThread(StatePtr state,Captures*captures,std::vector<Thread>*l,std::string::iterator sp,int gen) {
    if(states[state].gen==gen){return;}
    states[state].gen=gen;
    if(states[state].type == State::SPLIT){
        captures->refs+=2;
        addThread(states[state].out,captures,l,sp,gen);
        addThread(states[state].out1,captures,l,sp,gen);
        return;
    }
    if(states[state].type == State::SAVE){
        addThread(states[state].out,captures->update(states[state].capture,sp,numCaptures),l,sp,gen);
        return;
    }
    captures->refs++;
    l->emplace_back(state,captures);
}
std::string::iterator* NanoRegex::match(std::string str) {
    std::vector<Thread>curr,next;
    std::string::iterator iter=str.begin();
    {
        Captures*captures=Captures::create(numCaptures);
        for(size_t i=0;i<numCaptures;i++){captures->values[i]=std::string::iterator();}
        addThread(start,captures,&curr, iter,gen);
    }
    Captures*match=nullptr;
    for(;;iter++){
        // +++ check cache
        gen++;
        for(Thread&thread:curr){
            if(states[thread.state].type==State::MATCH&&iter==str.end()&&match==nullptr){
                match=thread.captures;
				match->refs++;
            }
            if(states[thread.state].type==State::CHAR&&iter!=str.end()&&states[thread.state].c == *iter){
                addThread(states[thread.state].out,thread.captures,&next,iter+1,gen);
            }
            if(states[thread.state].type == State::ANY&&iter!=str.end()){
                addThread(states[thread.state].out,thread.captures,&next,iter+1,gen);
            }
			thread.captures->decref();
        }
        curr.clear();
        curr.swap(next);
        if(iter==str.end()){break;}
    }
    if(match==nullptr){return nullptr;}
    std::string::iterator*values=new std::string::iterator[numCaptures];
    for(size_t i=0;i<numCaptures;i++){values[i]=match->values[i];}
    match->decref();
    return values;
}