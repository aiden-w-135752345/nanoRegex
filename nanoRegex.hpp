#include <cstdlib>
// #include <cassert>
#include <array>
class NanoRegex {
    [[noreturn]] inline static void unreachable_halt(const char*);
    inline static void unreachable_continue(const char*);
    struct State{
        enum Type{CHAR,ANY,MATCH,SPLIT,SAVE}type;
        typedef size_t Idx;
        union Out{
            Idx out;Out*next;
            constexpr Out(Idx o):out(o){}
            constexpr Out():next(nullptr){}
        };
        Out out;
        union{
            char character;
            Out out1;
            size_t capture;
        };
        constexpr State():type(ANY),out(),character('\0'){}
        constexpr State(Type t,char cc):type(t),out(),character(cc){if(type!=CHAR)unreachable_halt("wrong args");}
        constexpr State(Type t,size_t cc):type(t),out(),capture(cc){if(type!=SAVE)unreachable_halt("wrong args");}
        constexpr State(Type t):type(t),out(),out1(){if(type!=ANY&&type!=SPLIT&&type!=MATCH)unreachable_halt("wrong args");}
    };
    const State*states;
    State::Idx numStates;
    bool shouldDelete;
    State::Idx start;
    size_t numCaptures;
public:
    NanoRegex(const char*);
    const char** match(const char*,const char*)const;
    char** match(char*begin,char*end)const{
        return const_cast<char**>(match(
            static_cast<const char*>(begin),
            static_cast<const char*>(end)
        ));
    };
    int captures()const{return numCaptures;}
    ~NanoRegex(){if(shouldDelete){delete[]states;}}
private:
    enum Flags{MULTILINE=1,DOT_NEWLINES=2,UNGREEDY=4};
    struct Node{
        enum Type{ANY,CHAR,REP_GREEDY,REP_UNGREEDY,CAT,ALT,CAPTURE}type;
        union{
            std::nullptr_t none;
            char character;
            struct{int min;int max;} repeat;
            Node* left;
        };
        constexpr Node():type(ANY),none(){}
        constexpr Node(Type t,char c):type(t),character(c){if(t!=CHAR){unreachable_halt("wrong args");}}
        constexpr Node(Type t):type(t),none(){if(t!=ANY&&t!=CAPTURE){unreachable_halt("wrong args");}}
        constexpr Node(Type t,int min,int max):type(t),repeat({min,max}){if(t!=REP_GREEDY&&t!=REP_UNGREEDY){unreachable_halt("wrong args");}}
        constexpr Node(Type t,Node* l):type(t),left(l){if(t!=CAT&&t!=ALT){unreachable_halt("wrong args");}}
    };
    class Sizes{
        class Calculate;
    public:
        size_t nodes;
        State::Idx states;
        constexpr static Sizes calculate(const char *src){
            Calculate calculator(src);
            Sizes ret=calculator.run();
            if(*calculator.src){throw "unexpected )";}
            ret.states++;
            return ret;
        }
    };
    class Sizes::Calculate{
    public:
        const char * src;
        constexpr Calculate(const char *source):src(source){}
        constexpr Sizes run(){return alternate();}
    private:
        constexpr Sizes __attribute__((always_inline)) atom(){
            if(('A'<=(*src)&&(*src)<='Z')||('a'<=(*src)&&(*src)<='z')){
                src++;return Sizes{1,1};
            }
            switch(*(src++)){
            case '.':return Sizes{1,1};
            case '\\':
                switch(*src){
                case '\\':case '.':case '?':case '*':case '+':case '{':case '}':case '|':case '(':case ')':
                    src++;return Sizes{1,1};
                default: throw "unknown escape";
                }
            case '(':{
                if((*src)=='?'){
                    for(src++;(*src)!='-'&&(*src)!=':'; src++){
                        switch(*src){case 'm':case 's':case 'U':break;default: throw "bad flag";}
                    }
                    if((*src)=='-'){for(src++;(*src)!=':'; src++){
                        switch(*src){case 'm':case 's':case 'U':break;default: throw "bad flag";}
                    }}
                    src++;
                    Sizes ret=alternate();
                    if(*(src++)!=')')throw "mismatched ()";
                    return ret;
                }else{
                    Sizes ret=alternate();
                    if(*(src++)!=')')throw "mismatched ()";
                    ret.nodes++;ret.states+=2;return ret;
                }
            }
            default:throw "unknown character";
            }
        }
        constexpr Sizes repeat(){
            Sizes ret=atom();
            int min=0,max=0;
            switch(*src){
            case '?':min=0;max=1;break;
            case '*':min=0;max=0;break;
            case '+':min=1;max=0;break;
            case '{':
                for(src++;(*src)!=','&&(*src)!='}'; src++){
                    if((*src)<'0'||'9'<(*src)){throw "not digit";}
                    min*=10;min+=(*src)-'0';
                }
                if((*src)==','){
                    for(src++;(*src)!='}'; src++){
                        if((*src)<'0'||'9'<(*src)){throw "not digit";}
                        max*=10;max+=(*src)-'0';
                    }
                    if(max<min){throw "max<min";}
                }else{max=min;}
                if(max==0){throw "max=0";}
                break;
            default:return ret;
            }
            if(*(++src)=='?'){src++;}
            ++ret.nodes;
            if(min==0){
                if(max==0){ret.states=ret.states+1;}// [0,inf)
                else{ret.states=max*(ret.states+1);}// [0,max]
            }else{
                if(max==0){ret.states=min*ret.states+1;}// [min,inf)
                else{ret.states=min*ret.states+(max-min)*(ret.states+1);}// [min,max]
            }
            return ret;
        }
        constexpr Sizes catenate(){
            Sizes ret=repeat();
            while((*src)!='|'&&(*src)!=')'&&(*src)!='\0'){
                Sizes add=repeat();
                ret.nodes+=add.nodes+1;
                ret.states+=add.states;
            }
            return ret;
        }
        constexpr Sizes alternate(){
            Sizes ret=catenate();
            while((*src)=='|'){
                src++;
                Sizes add=catenate();
                ret.nodes+=add.nodes+1;
                ret.states+=add.states+1;
            }
            return ret;
        }
    };
    class Parser{
        Flags currentFlags;
        const char * src;
        Node* nodes;
    public:
        constexpr Parser(const char *source,Node*buffer)
            :currentFlags(Flags(0)),src(source),nodes(buffer){alternate();if(*src){unreachable_halt("unexpected )");}}
    private:
        constexpr __attribute__((always_inline)) void atom();
        constexpr void repeat(){
            atom();
            int min=0,max=0;
            switch(*src){
            case '?':min=0;max=1;break;
            case '*':min=0;max=0;break;
            case '+':min=1;max=0;break;
            case '{':
                for(src++;(*src)!=','&&(*src)!='}'; src++){
                    if((*src)<'0'||'9'<(*src)){unreachable_halt("not digit");}
                    min*=10;min+=(*src)-'0';
                }
                if((*src)==','){
                    for(src++;(*src)!='}'; src++){
                        if((*src)<'0'||'9'<(*src)){unreachable_halt("not digit");}
                        max*=10;max+=(*src)-'0';
                    }
                    if(max<min){unreachable_halt("max<min");}
                }else{max=min;}
                if(max==0&&min==0){unreachable_halt("max=min=0");}
                break;
            default:return;
            }
            bool greedy=(currentFlags&UNGREEDY)==0;
            if(*(++src)=='?'){src++;greedy=!greedy;}
            *(nodes++)=Node(greedy?Node::REP_GREEDY:Node::REP_UNGREEDY,min,max);
        }
        constexpr void catenate(){
            repeat();
            while((*src)!='|'&&(*src)!=')'&&(*src)!='\0'){
                Node* left=nodes-1;
                repeat();
                *(nodes++)=Node(Node::CAT,left);
            }
        }
        constexpr void alternate(){
            catenate();
            while((*src)=='|'){
                src++;
                Node* left=nodes-1;
                catenate();
                *(nodes++)=Node(Node::ALT,left);
            }
        }
    };
    class Compiler{
        class OutList{
            State::Out*first;
            State::Out*last;
        public:
            constexpr OutList():first(nullptr),last(nullptr){}
            constexpr OutList(State::Out*value):first(value),last(value){
                if(value->next){unreachable_halt("invalid state");}
            }
            constexpr __attribute__((always_inline)) void push(State::Out*value){
                if((!first)!=(!last)){unreachable_halt("invalid state");}
                if(last&&last->next){unreachable_halt("invalid state");}
                if(value->next){unreachable_halt("invalid state");}
                if(last){last->next=value;}else{first=value;}
                last=value;
            }
            constexpr __attribute__((always_inline)) void append(OutList& other){
                if((!first)!=(!last)){unreachable_halt("invalid state");}
                if(last&&last->next){unreachable_halt("invalid state");}
                if(other.last&&other.last->next){unreachable_halt("invalid state");}
                if((!other.first)!=(!other.last)){unreachable_halt("invalid state");}
                if(last){last->next=other.first;}else{first=other.first;}
                last=other.last;
                other.first=other.last=nullptr;
            }
            constexpr void patch(State::Idx state){
                if((!first)!=(!last)){unreachable_halt("invalid state");}
                if(last&&last->next){unreachable_halt("invalid state");}
                for(;first;){State::Out*next=first->next;*first=state;first=next;}
                last=nullptr;
            }
        };
        struct Frag {State::Idx start;OutList out;};
        State* states;
        State::Idx numStates;
        public:
        size_t numCaptures;
        constexpr Compiler(State*buffer):states(buffer),numStates(0),numCaptures(0){}
        constexpr State::Idx run(Node* node){
            Frag frag=build(node);
            frag.out.patch(create(State::MATCH));
            return frag.start;
        }
        private:
        struct IdxWithPtr{
            State::Idx idx;State* ptr;
            constexpr operator State::Idx(){return idx;}
            constexpr operator State*(){return ptr;}
            constexpr State* operator->(){return ptr;}
        };
        template<typename... Args>constexpr IdxWithPtr create(Args... args){
            IdxWithPtr ret={numStates,&states[numStates]};
            *ret=State(args...);numStates++;
            return ret;
        }
        constexpr Frag build(Node* node){
            switch(node->type){
            case Node::CHAR:{
                IdxWithPtr state=create(State::CHAR,node->character);
                return Frag{state,&state->out};
            }
            case Node::ANY:{
                IdxWithPtr state=create(State::ANY);
                return Frag{state,&state->out};
            }
            case Node::REP_GREEDY:case Node::REP_UNGREEDY:{
                int min=node->repeat.min, max=node->repeat.max;
                const bool greedy=node->type==Node::REP_GREEDY;
                const auto set_cont=[&](State* state,State::Idx out){if(greedy){state->out=out;}else{state->out1=out;}};
                const auto get_stop=[&](State* state){return greedy?&(state->out1):&(state->out);};
                if(min==0){
                    if(max==0){// [0,inf)
                        IdxWithPtr split=create(State::SPLIT);
                        Frag frag=build(node-1);
                        frag.out.patch(split);
                        set_cont(split,frag.start);
                        return Frag{split,get_stop(split)};
                    }else{// [0,max]
                        IdxWithPtr split=create(State::SPLIT);
                        Frag frag=build(node-1);
                        OutList stops;
                        stops.push(get_stop(split));
                        set_cont(split,frag.start);
                        for(int i=1;i<max;i++){
                            split=create(State::SPLIT);
                            frag.out.patch(split);
                            frag=build(node-1);
                            set_cont(split,frag.start);
                            stops.push(get_stop(split));
                        }
                        frag.out.append(stops);
                        return Frag{split,frag.out};
                    }
                }else{
                    if(max==0){// [min,inf)
                        Frag ret=build(node-1);
                        State::Idx loopback=ret.start;
                        for(int i=1;i<min;i++){
                            Frag frag=build(node-1);
                            ret.out.patch(loopback=frag.start);
                            ret.out=frag.out;
                        }
                        IdxWithPtr split=create(State::SPLIT);
                        ret.out.patch(split);
                        set_cont(split,loopback);
                        return Frag{ret.start,get_stop(split)};
                    }else{// [min,max]
                        Frag ret=build(node-1);
                        State::Idx loopback=ret.start;
                        for(int i=1;i<min;i++){
                            Frag frag=build(node-1);
                            ret.out.patch(loopback=frag.start);
                            ret.out=frag.out;
                        }
                        OutList stops;
                        for(int i=min;i<max;i++){
                            IdxWithPtr split=create(State::SPLIT);
                            ret.out.patch(split);
                            stops.push(get_stop(split));
                            Frag frag=build(node-1);
                            set_cont(split,frag.start);
                            ret.out=frag.out;
                        }
                        ret.out.append(stops);
                        return ret;
                    }
                }
            } // end of Node::REP case
            case Node::CAT:{
                Frag left=build(node->left);
                Frag right=build(node-1);
                left.out.patch(right.start);
                return Frag{left.start,right.out};
            }
            case Node::ALT:{
                IdxWithPtr start=create(State::SPLIT);
                Frag left=build(node->left);
                Frag right=build(node-1);
                start->out=left.start;start->out1=right.start;
                left.out.append(right.out);
                return Frag{start,left.out};
            }
            case Node::CAPTURE:{
                IdxWithPtr start = create(State::SAVE,numCaptures);
                numCaptures+=2;
                Frag content=build(node-1);
                start->out=content.start;
                IdxWithPtr end=create(State::SAVE,start->capture+1);
                content.out.patch(end);
                return Frag{start,&end->out};
            }
            default:unreachable_halt("bad node type");
            }
        }
    };
    template<State::Idx N>struct init_struct{
        std::array<State,N>states;State::Idx start;size_t numCaptures;
    };
    template<const NanoRegex::Sizes*const sizes>constexpr static auto init_func(const char*src){
        std::array<State,sizes->states>states;
        std::array<Node,sizes->nodes>nodes;
        Parser(src,&nodes.front());
        Compiler compile(&states.front());
        State::Idx start=compile.run(&nodes.back());
        size_t numCaptures=compile.numCaptures;
        return init_struct<sizes->states>{states,start,numCaptures};
    }
    template<char... chars>class init_class{
        constexpr static const char source[]={chars...};
        constexpr static Sizes sizes=Sizes::calculate(source);
    public:
        constexpr static auto value=init_func<&sizes>(source);
    };
public:
    template<State::Idx N>constexpr NanoRegex(const init_struct<N>&init):
    states(&init.states.front()),numStates(N),shouldDelete(false),start(init.start),numCaptures(init.numCaptures){}
    template<typename T, T... chars>friend constexpr auto operator""_regex();
};
[[noreturn]] inline void NanoRegex::unreachable_halt(const char*error){
#ifdef NDEBUG
# ifdef __GNUC__ 
    __builtin_unreachable();
# endif
# ifdef _MSC_VER
    __assume(false&&error);
# endif
#endif
    throw error;
}
inline void NanoRegex::unreachable_continue(const char*error){
#ifndef NDEBUG
    throw error;
#else
    (void)error;
#endif
}
constexpr __attribute__((always_inline)) void NanoRegex::Parser::atom(){
    if(('A'<=(*src)&&(*src)<='A')||('a'<=(*src)&&(*src)<='z')){
        *(nodes++)=Node(Node::CHAR,*(src++));return;
    }
    switch(*(src++)){
    case '.':*(nodes++)=Node(Node::ANY);return;
    case '\\':
        
        switch(
#ifndef NDEBUG
               *src
#else
               '\\'
#endif
               ){
        case '\\':case '.':case '?':case '*':case '+':case '{':case '}':case '|':case '(':case ')':
            *(nodes++)=Node(Node::CHAR,*(src++));return;
        default:unreachable_halt("unknown escape");
        }
    case '(':{
        if((*src)=='?'){
            Flags parentFlags=currentFlags;
            for(src++;(*src)!='-'&&(*src)!=':'; src++){
                switch(*src){
                case 'm':currentFlags=Flags(currentFlags|MULTILINE);break;
                case 's':currentFlags=Flags(currentFlags|DOT_NEWLINES);break;
                case 'U':currentFlags=Flags(currentFlags|UNGREEDY);break;
                default:unreachable_halt("bad flag");
                }
            }
            if((*src)=='-'){for(src++;(*src)!=':'; src++){
                switch(*src){
                case 'm':currentFlags=Flags(currentFlags&~MULTILINE);break;
                case 's':currentFlags=Flags(currentFlags&~DOT_NEWLINES);break;
                case 'U':currentFlags=Flags(currentFlags&~UNGREEDY);break;
                default:unreachable_halt("bad flag");
                }
            }}
            src++;
            alternate();
            if(*src!=')')unreachable_halt("mismatched ()");
            src++;currentFlags=parentFlags;
            return;
        }else{
            alternate();
            if(*src!=')')unreachable_halt("mismatched ()");
            src++;*(nodes++)=Node(Node::CAPTURE);
            return;
        }
    }
    default:unreachable_halt("unknown character");
    }
}
template<typename T, T... chars>constexpr auto operator""_regex(){
    return NanoRegex::init_class<chars...,'\0'>::value;
}
