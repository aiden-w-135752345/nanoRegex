#include <vector>
#include <string>
#include <list>
#include <utility>
#include "nanoRegex.hpp"
namespace nanoRegexImpl{
    enum Flags{MULTILINE=1,DOT_NEWLINES=2,UNGREEDY=4};
    struct Node{
        enum Type{CHAR,ANY,REP,CAT,ALT,CAPTURE}type;
        Node* left;
        union{
            char character;
            struct{int min;int max;bool greedy;}repeat;
            Flags parentFlags;
            Node* right;
        };
        Node(Type t,char c):type(t),character(c){
            if(type!=CHAR)throw"wrong args";
        }
        Node(Type t):type(t){
            if(type!=ANY){throw"wrong args";}
        }
        Node(Type t,Node* node,int min,int max,bool greedy):type(t),left(node),repeat{min,max,greedy}{
            if(type!=REP)throw"wrong args";
        }
        Node(Type t,Node* l,Node* r):type(t),left(l),right(r){
            if(type!=CAT&&type!=ALT)throw"wrong args";
        }
        Node(Type t,Node* node):type(t),left(node){
            if(type!=CAPTURE)throw"wrong args";
        }
        void del(){
            if(type==CAT||type==ALT){left->del();right->del();}
            if(type==REP||type==CAPTURE){left->del();}
            delete this;
        }
    };
    class Parser{
        Flags currentFlags;
        char* src;
        int srcIdx;
        public:
        Parser(char *source):currentFlags(Flags(0)),src(source),srcIdx(0){}
        Node*run(){return alternate();}
        private:
        Node* atom();
        Node* repeat();
        Node* catenate();
        Node* alternate();
    };
    struct Frag {
        typedef NanoRegex::StatePtr StatePtr;
        typedef NanoRegex::State State;
        StatePtr start;
        struct Patch{
            StatePtr ptr;StatePtr State::*member;
            Patch(StatePtr p,StatePtr State::*m):ptr(p),member(m){}
        };
        std::list<Patch>out;
        Frag(StatePtr s,std::list<Patch>o):start(s),out(o){};
    };
};