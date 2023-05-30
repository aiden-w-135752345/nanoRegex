#include "impl_match.hpp"
std::string::iterator* NanoRegex::match(std::string str) {
    using namespace nanoRegexImpl;
    re=this;
    std::vector<Thread>curr,next;
    std::string::iterator iter=str.begin();
    {
        Captures*captures=Captures::create(numCaptures);
        for(size_t i=0;i<numCaptures;i++){captures->values[i]=std::string::iterator();}
        Thread::add(start,captures,&curr, iter,gen);
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
                Thread::add(states[thread.state].out,thread.captures,&next,iter+1,gen);
            }
            if(states[thread.state].type == State::ANY&&iter!=str.end()){
                Thread::add(states[thread.state].out,thread.captures,&next,iter+1,gen);
            }
			thread.captures->decref();
        }
        curr.clear();
        curr.swap(next);
        if(iter==str.end()){break;}
    }
    re=nullptr;
    return match!=nullptr?match->values:nullptr;
}