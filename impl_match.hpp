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
        int*refs;
        std::string::iterator*values;
        Captures():refs(new int),values(new std::string::iterator[re->numCaptures]){*refs=1;}
        Captures(nullptr_t):refs(nullptr),values(nullptr){}
        Captures(const Captures&other):refs(other.refs),values(other.values){++*refs;};
        Captures(Captures&& other):Captures(nullptr){swap(*this,other);}
        Captures& operator=(Captures other){
            swap(*this,other);
            return *this;
        };
        friend void swap(Captures&a, Captures&b){
            using std::swap;
            swap(a.refs,b.refs);
            swap(a.values,b.values);
        }
        ~Captures(){
            if(values!=nullptr&&--*refs==0){
                delete refs;delete[]values;values=nullptr;
            }
        }
        Captures update(int idx,std::string::iterator val)const{
            if(values==nullptr){throw "dead";}
            Captures edited;
            for(int i=0;i<re->numCaptures;i++)edited.values[i]=values[i];
            edited.values[idx]=val;
            return edited;
        }
    };
    struct Thread{
        typedef NanoRegex::StatePtr StatePtr;
        typedef NanoRegex::State State;
        StatePtr state;
        Captures captures;
        Thread(StatePtr s,Captures c):state(s),captures(std::move(c)){}
        static void add(StatePtr state,Captures captures,std::vector<Thread>*l,std::string::iterator sp,int gen) {
            if(re->states[state].gen==gen){return;}
            re->states[state].gen=gen;
            if(re->states[state].type == State::SPLIT){
                Thread::add(re->states[state].out,captures,l,sp,gen);
                Thread::add(re->states[state].out1,captures,l,sp,gen);
                return;
            }
            if(re->states[state].type == State::SAVE){
                Thread::add(re->states[state].out,captures.update(re->states[state].capture,sp),l,sp,gen);
                return;
            }
            l->emplace_back(state,captures);
        }
    };
};