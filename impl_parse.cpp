#include "impl_parse.hpp"
using namespace nanoRegexImpl;

Node* Parser::atom(){
    if(('A'<=src[srcIdx]&&src[srcIdx]<='A')||('a'<=src[srcIdx]&&src[srcIdx]<='z')){
        return new Node(Node::CHAR,src[srcIdx++]);
    }
    switch(src[srcIdx]){
    case '.':return new Node(Node::ANY);
    case '\\':
        switch(src[++srcIdx]){
            case '\\':case '.':case '?':case '*':case '+':case '{':case '}':case '|':case '(':case ')':
                return new Node(Node::CHAR,src[srcIdx++]);
            default: throw "unknown escape";
        }
    case '(':{
        if(src[srcIdx+1]=='?'){
            Flags parentFlags=currentFlags;
            for(srcIdx+=2; src[srcIdx]!='-'&&src[srcIdx]!=':'; srcIdx++){
                switch(src[srcIdx]){
                    case 'm':currentFlags=Flags(currentFlags|MULTILINE);break;
                    case 's':currentFlags=Flags(currentFlags|DOT_NEWLINES);break;
                    case 'U':currentFlags=Flags(currentFlags|UNGREEDY);break;
                    default: throw "bad flag";
                }
            }
            if(src[srcIdx]=='-'){for(srcIdx++;src[srcIdx]!=':'; srcIdx++){switch(src[srcIdx]){
                case 'm':currentFlags=Flags(currentFlags&~MULTILINE);break;
                case 's':currentFlags=Flags(currentFlags&~DOT_NEWLINES);break;
                case 'U':currentFlags=Flags(currentFlags&~UNGREEDY);break;
                default: throw "bad flag";
            }}}
            srcIdx++;
            Node*node=alternate();
            if(src[srcIdx++]!=')')throw "unexpected end of string";
            currentFlags=parentFlags;
            return node;
        }else{
            srcIdx++;
            Node*node=alternate();
            if(src[srcIdx++]!=')')throw "unexpected end of string";
            return new Node(Node::CAPTURE,node);
        }
    }
    default: throw "unknown character";
    }
}
Node* Parser::repeat(){
    Node* node=atom();
    int min;
    int max;
    switch(src[srcIdx]){
        case '?':min=0;max=1;break;
        case '*':min=0;max=0;break;
        case '+':min=1;max=0;break;
        case '{':
            min=0;
            for(srcIdx++; src[srcIdx]!=','&&src[srcIdx]!='}'; srcIdx++){
                if(src[srcIdx]<'0'||'9'<src[srcIdx]){throw "not digit";}
                min*=10;min+=src[srcIdx]-'0';
            }
            if(src[srcIdx]==','){
                max=0;
                for(srcIdx++;src[srcIdx]!='}'; srcIdx++){
                    if(src[srcIdx]<'0'||'9'<src[srcIdx]){throw "not digit";}
                    max*=10;max+=src[srcIdx]-'0';
                }
                if(max<min){throw "max<min";}
            }else{max=min;}
            if(max==0&&min==0){throw "max=min=0";}
            break;
        default:return node;
    }
    bool greedy=(currentFlags&UNGREEDY)==0;
    if(src[++srcIdx]=='?'){srcIdx++;greedy=!greedy;}
    return new Node(Node::REP,node,min,max,greedy);
}
Node* Parser::catenate(){
    Node* node=repeat();
    while(src[srcIdx]!='|'&&src[srcIdx]!=')'&&src[srcIdx]!='\0'){
        node=new Node(Node::CAT,node,repeat());
    }
    return node;
}
Node* Parser::alternate(){
    Node* node=catenate();
    while(src[srcIdx]=='|'){
        srcIdx++;
        node=new Node(Node::ALT,node,catenate());
    }
    return node;
}

template<typename... Args> NanoRegex::StatePtr NanoRegex::createState(Args... args){
    states.emplace_back(args...);
    return states.size()-1;
}
typedef Frag::Patch Patch;
void NanoRegex::patch(Frag&frag,StatePtr state){
    if(frag.out.empty()){throw"already patched";}
    for(Patch patch:frag.out){states[patch.ptr].*patch.member=state;}
    frag.out.clear();
}
Frag NanoRegex::build(Node* node){
    switch(node->type){
        case Node::CHAR:{
            StatePtr state=createState(State::CHAR,node->character);
            return {state,{Patch(state,&State::out)}};
        }
        case Node::ANY:{
            StatePtr state=createState(State::ANY);
            return{state,{Patch(state,&State::out)}};
        }
        case Node::REP:{
            int min=node->repeat.min;
            int max=node->repeat.max;
            int greedy=node->repeat.greedy;
            StatePtr State::*cont=greedy?&State::out:&State::out1;
            StatePtr State::*stop=greedy?&State::out1:&State::out;
            if(min==0){
                if(max==0){// [0,inf)
                    StatePtr split=createState(State::SPLIT);
                    Frag frag=build(node->left);
                    patch(frag,split);
                    states[split].*cont=frag.start;
                    return {split,{Patch(split,stop)}};
                }else{// [0,max]
                    StatePtr split=createState(State::SPLIT);
                    Frag frag=build(node->left);
                    std::list<Patch>stops;
                    stops.emplace_back(split,stop);
                    states[split].*cont=frag.start;
                    for(int i=1;i<max;i++){
                        split=createState(State::SPLIT);
                        patch(frag,split);
                        frag=build(node->left);
                        states[split].*cont=frag.start;
                        stops.emplace_back(split,stop);
                    }
                    frag.out.splice(frag.out.end(),stops);
                    return {split,frag.out};
                }
            }else{
                if(max==0){// [min,inf)
                    Frag ret=build(node->left);
                    StatePtr loopback=ret.start;
                    for(int i=1;i<min;i++){
                        Frag frag=build(node->left);
                        patch(ret,loopback=frag.start);
                        ret.out=std::move(frag.out);
                    }
                    StatePtr split=createState(State::SPLIT);
                    patch(ret,split);
                    states[split].*cont=loopback;
                    return {ret.start,{Patch(split,stop)}};
                }else{// [min,max]
                    Frag ret=build(node->left);
                    StatePtr loopback=ret.start;
                    for(int i=1;i<min;i++){
                        Frag frag=build(node->left);
                        patch(ret,loopback=frag.start);
                        ret.out=std::move(frag.out);
                    }
                    std::list<Patch>stops;
                    for(int i=min;i<max;i++){
                        StatePtr split=createState(State::SPLIT);
                        patch(ret,split);
                        stops.push_back(Patch(split,stop));
                        Frag frag=build(node->left);
                        states[split].*cont=frag.start;
                        ret.out=std::move(frag.out);
                    }
                    ret.out.splice(ret.out.end(),stops);
                    return ret;
                }
            }
        } // end of Node::REP case
        case Node::CAT:{
            Frag left=build(node->left);
            Frag right=build(node->right);
            patch(left,right.start);
            return{left.start,std::move(right.out)};
        }
        case Node::ALT:{
            StatePtr start=createState(State::SPLIT);
            Frag left=build(node->left);
            Frag right=build(node->right);
            states[start].out=left.start;states[start].out1=right.start;
            std::list<Patch>outs;
            outs.splice(outs.end(),left.out);outs.splice(outs.end(),right.out);
            return{start,std::move(outs)};
        }
        case Node::CAPTURE:{
            StatePtr start = createState(State::SAVE,numCaptures);
            numCaptures+=2;
            Frag content=build(node->left);
            states[start].out=content.start;
            StatePtr end=createState(State::SAVE,states[start].capture+1);
            patch(content,end);
            return{start,{Patch(end,&State::out)}};
        }
        default:throw "???";
    }
}
NanoRegex::NanoRegex(char *source):numCaptures(0) {
    using namespace nanoRegexImpl;
    Node* node=Parser(source).run();
    Frag frag=build(node);
    node->del();
    patch(frag,createState(State::MATCH));
    start=frag.start;
}