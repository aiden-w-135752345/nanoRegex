#include "nanoRegex.hpp"
#include "cx.hpp"
#include <cstdio>
using namespace NanoRegex_detail;
void print_nodes(const Parse::Node*node){
    using Node=Parse::Node;
    switch(node->type){
    case Node::CHAR:{
        fprintf(stderr,"[");
        for(uint8_t i=0;i<0xff;i++){if(node->charclass.has(i)){fprintf(stderr,"%c",i);}}
        fprintf(stderr,"]");
        break;
    }
    case Node::REP_GREEDY:case Node::REP_UNGREEDY:{
        print_nodes(node->repeat.content);
        fprintf(stderr,"{%d %d}",node->repeat.min,node->repeat.max);
        break;
    }
    case Node::CAT:{
        print_nodes(node->binary.left);print_nodes(node->binary.right);break;
    }
    case Node::ALT:{
        print_nodes(node->binary.left);
        fprintf(stderr,"|");
        print_nodes(node->binary.right);
        break;
    }
    case Node::CAPTURE:{
        fprintf(stderr,"(");
        print_nodes(node->capture.content);
        fprintf(stderr,")");
        break;
    }
    default:throw "bad node type";
    }
}
namespace UnoptimizedType{
    constexpr static const UnoptimizedState::Type CHAR=UnoptimizedState::Type::CHAR;
    constexpr static const UnoptimizedState::Type SPLIT=UnoptimizedState::Type::SPLIT;
    constexpr static const UnoptimizedState::Type SAVE=UnoptimizedState::Type::SAVE;
}
void print_state(size_t i,const UnoptimizedState& state){
    switch(state.type){
    default:
        fprintf(stderr,"%2zu CHAR to %2zu\n",i,state.out);break;
    case UnoptimizedType::SPLIT:
        fprintf(stderr,"%2zu SPLIT to %2zu, %2zu\n",i,state.out,state.out1);break;
    case UnoptimizedType::SAVE:
        fprintf(stderr,"%2zu CAPTURE to %2zu [%zu]\n",i,state.out,state.capture);break;
    }
}
NanoRegex::NanoRegex(const char *source):shouldDelete(true){
    const Sizes sizes=Sizes::Calculate(source).run();
    numCaptures=sizes.saveStates;
    Parse::Node*nodes=new Parse::Node[sizes.nodes];
    Parse(source,nodes);
    print_nodes(nodes+sizes.nodes-1);fputc('\n',stderr);
    numStates=sizes.charStates+sizes.splitStates+sizes.saveStates;
    NFAEpsState*nfaEpsStates=new NFAEpsState[numStates];
    RegEx2NFAEps(nodes[sizes.nodes-1],nfaEpsStates);
    delete[]nodes;
    UnoptimizedState*unoptimizedStates=new UnoptimizedState[numStates];
    for(StateIdx i=0;i<numStates;i++){unoptimizedStates[i]=UnoptimizedState{nfaEpsStates,nfaEpsStates[i]};}
    delete[]nfaEpsStates;
    states=unoptimizedStates;
    for(size_t i=0;i<numStates;i++)print_state(i,states[i]);
    fprintf(stderr,"%2zu MATCH\n",numStates);
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
        const UnoptimizedState*const states;
        const size_t numStates;
        const size_t numCaptures;
        Captures* createCaptures(){
            Captures* caps=(Captures*)malloc(sizeof(Captures)+((ssize_t)numCaptures-1)*(ssize_t)sizeof(const char*));
            caps->refs=1;
            return caps;
        }
        void addThread(StateIdx stateIdx,Captures*captures){
            const UnoptimizedState&state=states[stateIdx];
            if(sparse[stateIdx]<nextCount&&next[sparse[stateIdx]].state==stateIdx){return;}
            if(stateIdx<numStates){
                if(state.type == UnoptimizedType::SPLIT){
                    addThread(state.out,captures);
                    addThread(state.out1,captures);
                    return;
                }
                if(state.type == UnoptimizedType::SAVE){
                    Captures*edited=createCaptures();
                    for(size_t i=0;i<numCaptures;i++)edited->values[i]=captures->values[i];
                    edited->values[state.capture]=sp;
                    addThread(state.out,edited);
                    edited->decref();
                    return;
                }
            }
            captures->refs++;
            sparse[stateIdx]=nextCount;next[nextCount++]=Thread{stateIdx,captures};
        };
    };
}
const char** NanoRegex::match(const char*begin,const char*end)const{
    Thread*curr=new Thread[numStates+1];
    Matcher m={
        new Thread[numStates+1],
        new size_t[numStates+1],
        0,begin,states,numStates,numCaptures
    };
    {
        Captures*captures=m.createCaptures();
        for(size_t i=0;i<numCaptures;i++){captures->values[i]=nullptr;}
        m.addThread(0,captures);
        captures->decref();
    }
    while(m.sp!=end){
        size_t currCount=m.nextCount;m.nextCount=0;
        {Thread*tmp=curr;curr=m.next;m.next=tmp;}
        char currChar=*(m.sp++);
        for(size_t thread=0;thread<currCount;thread++){
            StateIdx stateIdx=curr[thread].state;
            Captures*captures=curr[thread].captures;
            if(stateIdx<numStates){
                const UnoptimizedState&state=states[stateIdx];
                if(state.type!=UnoptimizedType::SPLIT && state.type!=UnoptimizedType::SAVE && CharClass(uint8_t(state.type),state.charclass).has(currChar)){
                    m.addThread(state.out,captures);
                }
            }
            captures->decref();
        }
    }
    delete[]curr;
    Captures*match=nullptr;
    if(m.sparse[numStates]<m.nextCount&&m.next[m.sparse[numStates]].state==numStates){
        match=m.next[m.sparse[numStates]].captures;match->refs++;
    }
    delete[]m.sparse;
    for(size_t thread=0;thread<m.nextCount;thread++){m.next[thread].captures->decref();}
    delete[]m.next;
    if(match==nullptr){return nullptr;}
    const char**values=new const char*[numCaptures];
    for(size_t i=0;i<numCaptures;i++){values[i]=match->values[i];}
    match->decref();
    return values;
}
