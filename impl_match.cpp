#include "impl_match.hpp"
std::string::iterator* NanoRegex::match(std::string str) {
    using namespace nanoRegexImpl;
    re=this;
    std::vector<Thread>curr,next;
    std::string::iterator iter=str.begin();
    {
        Captures captures;
        for(int i=0;i<this->numCaptures;i++){captures.values[i]=std::string::iterator();}
        Thread::add(this->start,std::move(captures),&curr, iter,gen);
    }
    Captures*match=nullptr;
    for(;;iter++){
        // +++ check cache
        gen++;
        for(Thread&thread:curr){
            if(re->states[thread.state].type==State::MATCH&&iter==str.end()&&match==nullptr){
                match=new Captures(thread.captures);
            }
            if(re->states[thread.state].type==State::CHAR&&iter!=str.end()&&re->states[thread.state].c == *iter){
                Thread::add(re->states[thread.state].out,thread.captures,&next,iter+1,gen);
            }
            if(re->states[thread.state].type == State::ANY&&iter!=str.end()){
                Thread::add(re->states[thread.state].out,thread.captures,&next,iter+1,gen);
            }
        }
        curr.clear();
        curr.swap(next);
        if(iter==str.end()){break;}
    }
    re=nullptr;
    return match!=nullptr?match->values:nullptr;
}