#include "nanoRegex.hpp"
#include "cx.hpp"
#include <cstdio>
using namespace NanoRegex_detail;
CharClass::Bldr test(const char *&src){return CharClass::Bldr(src);}
void print_nodes(const Node*node){
    switch(node->type){
    case Node::CHAR:{
        fprintf(stderr,"[");
        for(uint8_t i=0;i<0xff;i++){if(node->charclass.has(i)){fprintf(stderr,"%c",i);}}
        fprintf(stderr,"]");
        break;
    }
    case Node::REP_GREEDY:case Node::REP_UNGREEDY:{
        print_nodes(node-1);
        fprintf(stderr,"{%d %d}",node->repeat.min,node->repeat.max);
        break;
    }
    case Node::CAT:{
        print_nodes(node->left);print_nodes(node-1);break;
    }
    case Node::ALT:{
        print_nodes(node->left);
        fprintf(stderr,"|");
        print_nodes(node-1);
        break;
    }
    case Node::CAPTURE:{
        fprintf(stderr,"(");
        print_nodes(node-1);
        fprintf(stderr,")");
        break;
    }
    default:throw "bad node type";
    }
}
void print_state(size_t i,const State& state){
    switch(state.type){
    case State::CHAR:
        fprintf(stderr,"\n:%2zu CHAR to :%2zu",i,state.out.out);break;
    case State::MATCH:
        fprintf(stderr,"\n:%2zu MATCH",i);break;
    case State::SPLIT:
        fprintf(stderr,"\n:%2zu SPLIT to :%2zu, :%2zu",i,state.out.out,state.out1.out);break;
    case State::SAVE:
        fprintf(stderr,"\n:%2zu CAPTURE to :%2zu [%zu]",i,state.out.out,state.capture);break;
    }
}
NanoRegex::NanoRegex(const char *source):shouldDelete(true){
    const Sizes sizes=Sizes::Calculate(source).run();
    numCaptures=sizes.captures;
    Node*nodes=new Node[sizes.nodes];
    Parser(source,nodes);
    print_nodes(nodes+sizes.nodes-1);
    states=new State[numStates=sizes.states];
    Compiler((State*)states).run(nodes+sizes.nodes-1);
    delete[]nodes;
    for(size_t i=0;i<numStates;i++)print_state(i,states[i]);
}
NanoRegex::~NanoRegex(){if(shouldDelete){delete[]states;}}
namespace{
    struct Captures{
        size_t refs;const char* values[1];
        void decref(){if(--refs==0){free(this);}}
    };
    struct Thread{StateIdx state;Captures*captures;};
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
        void addThread(StateIdx stateIdx,Captures*captures){
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
    Thread*curr=new Thread[numStates];
    Matcher m={
        new Thread[numStates],
        new size_t[numStates],
        0,
        begin,
        states,
        numCaptures
    };
    {
        Captures*captures=m.createCaptures();
        for(size_t i=0;i<numCaptures;i++){captures->values[i]=nullptr;}
        m.addThread(0,captures);
        captures->decref();
    }
    size_t currCount=m.nextCount;m.nextCount=0;
    {Thread*tmp=curr;curr=m.next;m.next=tmp;}
    while(m.sp!=end){
        char currChar=*(m.sp++);
        for(size_t thread=0;thread<currCount;thread++){
            const State&state=states[curr[thread].state];
            Captures*captures=curr[thread].captures;
            if(state.type==State::CHAR && state.charclass.has(currChar)){
                m.addThread(state.out.out,captures);
            }
            captures->decref();
        }
        currCount=m.nextCount;m.nextCount=0;
        {Thread*tmp=curr;curr=m.next;m.next=tmp;}
    }
    delete[]m.sparse;delete[]m.next;
    Captures*match=nullptr;
    for(size_t thread=0;thread<currCount;thread++){
        Captures* captures=curr[thread].captures;
        if(states[curr[thread].state].type==State::MATCH){match=captures;}else{captures->decref();}
    }
    delete[]curr;
    if(match==nullptr){return nullptr;}
    const char**values=new const char*[numCaptures];
    for(size_t i=0;i<numCaptures;i++){values[i]=match->values[i];}
    match->decref();
    return values;
}
