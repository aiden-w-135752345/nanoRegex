#include "nanoRegex.hpp"
//#include <utility>
namespace{
    template<typename value> struct Rob{static value delegate();friend auto constexpr rob_func(){return delegate();}};
    auto constexpr rob_func();
    template struct Rob<NanoRegex::State>;
    typedef decltype(rob_func()) State;
    struct Captures{
        size_t refs;const char* values[1];
        void decref(){if(--refs==0){free(this);}}
    };
    struct Thread{State::Idx state;Captures*captures;};
    struct Matcher{
        Thread*next;
        size_t*sparse;
        size_t nextCount;
        const char*sp;
        const State*const states;
        const size_t numCaptures;
        Captures* createCaptures(){
            Captures* caps=(Captures*)malloc(sizeof(Captures)+((ssize_t)numCaptures-1)*(ssize_t)sizeof(const char*));
            caps->refs=1;
            return caps;
        }
        void addThread(State::Idx stateIdx,Captures*captures){
            const State&state=states[stateIdx];
            if(sparse[stateIdx]<nextCount&&next[sparse[stateIdx]].state==stateIdx){return;}
            sparse[stateIdx]=nextCount;
            if(state.type == State::SPLIT){
                addThread(state.out.out,captures);
                addThread(state.out1.out,captures);
                return;
            }
            if(state.type == State::SAVE){
                Captures*edited=createCaptures();
                for(size_t i=0;i<numCaptures;i++)edited->values[i]=captures->values[i];
                edited->values[state.capture]=sp;
                addThread(state.out.out,edited);
                edited->decref();
                return;
            }
            captures->refs++;
            next[nextCount++]=Thread{stateIdx,captures};
        };
    };
}
const char** NanoRegex::match(const char*begin,const char*end)const{
    Thread*curr=(Thread*)malloc(numStates*sizeof(Thread));
    Matcher m={
        (Thread*)malloc(numStates*sizeof(Thread)),
        (size_t*)malloc(numStates*sizeof(size_t)),
        0,
        begin,
        states,
        numCaptures
    };
    {
        Captures*captures=m.createCaptures();
        for(size_t i=0;i<numCaptures;i++){captures->values[i]=nullptr;}
        m.addThread(start,captures);
        captures->decref();
    }
    size_t currCount=m.nextCount;m.nextCount=0;
    {Thread*space=curr;curr=m.next;m.next=space;}
    while(m.sp!=end){
        char currChar=*(m.sp++);
        for(size_t thread=0;thread<currCount;thread++){
            const State&state=states[curr[thread].state];
            Captures*captures=curr[thread].captures;
            if((state.type==State::CHAR && state.character==currChar)||state.type==State::ANY){
                m.addThread(state.out.out,captures);
            }
            captures->decref();
        }
        currCount=m.nextCount;m.nextCount=0;
        {Thread*space=curr;curr=m.next;m.next=space;}
    }
    free(m.sparse);free(m.next);
    Captures*match=nullptr;
    for(size_t thread=0;thread<currCount;thread++){
        Captures* captures=curr[thread].captures;
        if(states[curr[thread].state].type==State::MATCH){match=captures;}else{captures->decref();}
    }
    free(curr);
    if(match==nullptr){return nullptr;}
    const char**values=new const char*[numCaptures];
    for(size_t i=0;i<numCaptures;i++){values[i]=match->values[i];}
    match->decref();
    return values;
}
NanoRegex::NanoRegex(const char *source):shouldDelete(true){
    const Sizes sizes=Sizes::calculate(source);
    states=new State[numStates=sizes.states];
    Node*nodes=new Node[sizes.nodes];
    Parser(source,nodes);
    Compiler compile((State*)states);
    start=compile.run(nodes+sizes.nodes-1);
    delete[]nodes;
    numCaptures=compile.numCaptures;
}
