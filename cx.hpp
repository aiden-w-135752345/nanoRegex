#include "nanoRegex.hpp"
#include <cstdlib>
#include <cassert>
#include <array>
namespace NanoRegex_detail{
    typedef NanoRegex::parse_exception parse_exception;
    constexpr uint8_t decodehex(unsigned char u,unsigned char v){return(u<<4)+(u>'9'?144:0)+(v&0x0f)+(v>'9'?9:0);}
    class CharClass{
        uint8_t rows;
        uint32_t cols;
    public:
        constexpr CharClass(bool b):rows(-(uint32_t)b),cols(-(uint32_t)b){};
        constexpr CharClass(int c):rows(1<<((c>>5)&0x7)),cols(1<<(c&0x1f)){};
        constexpr bool has(unsigned char c)const{
            CharClass cc=c;
            return (rows&cc.rows)&&(cols&cc.cols);
        };
        struct Bldr;
    };
    struct CharClass::Bldr{
        CharClass classes[8];uint8_t nclasses;
    private:
        constexpr void parse(const char *&src,bool *set){
            if(*src==']'){throw parse_exception("empty set");}
            bool invert=(*src=='^');if(invert){src++;}
            while(*src!=']'){
                switch(*src){
                case '\0':throw parse_exception("unterminated character class");
                case '[':throw parse_exception("no named character classes... yet.");
                case '\\':{
                    switch(*++src){
                    case '0':set['\0']=true;src++;break;
                    case 'f':set['\f']=true;src++;break;
                    case 'n':set['\n']=true;src++;break;
                    case 'r':set['\r']=true;src++;break;
                    case 't':set['\t']=true;src++;break;
                    case 'v':set['\v']=true;src++;break;
                    case 'c':{
                        if(!isalpha(src[1])){throw parse_exception("bad control escape");}
                        set[src[1]&0x1f]=true;src+=2;break;
                    }
                    case 'x':{
                        if(!(isxdigit(src[1])&&isxdigit(src[2]))){throw parse_exception("bad hex escape");}
                        set[decodehex(src[1],src[2])]=true;src+=3;break;
                    }
                    case '\\':case '-':case ':':case '.':case '?':case '*':case '+':case '{':case '}':case '[':case '|':case ']':case '(':case ')':{
                        set[(uint8_t)*(src++)]=true;break;
                    }
                    default:throw parse_exception("unknown escape");
                    }
                    break;
                }
                default:set[(uint8_t)*(src++)]=true;break;
                }
            }
            src++;
            if(invert){uint8_t i=0;do{set[i]=!set[i];}while(++i);}
        }
        constexpr uint8_t generate_prime(bool *set,uint8_t *primeRows,uint32_t *primeCols){
            uint8_t rows[8]={};uint8_t nrows=0;
            for(uint8_t i=0;i<8;i++){for(uint8_t j=0;j<32;j++){if(set[i*32+j]){rows[nrows++]=i;j=32;}}}
            uint32_t cols[256]={};
            for(uint16_t i=1;i<(1<<nrows);i++){
                for(uint8_t j=0;j<32;j++){
                    bool allSet=true;
                    for(uint8_t k=0;k<nrows;k++){if((i&(1<<k))&&!set[32*rows[k]+j]){allSet=false;break;}}
                    if(allSet){cols[i]|=(1<<j);}
                }
            }
            uint8_t primeKeys[255]={};uint8_t nprime=0;
            for(uint16_t i=1;i<(1<<nrows);i++){
                bool best=true;
                for(uint16_t j=1;j<(1<<nrows);j++){
                    if((i!=j)&&!(i&~j)&&!(cols[i]&~cols[j])){best=false;break;}
                }
                if(best){primeKeys[nprime++]=i;}
            }
            for(uint8_t i=0;i<nprime;i++){
                primeCols[i]=cols[primeKeys[i]];
                for(uint8_t j=0;j<nrows;j++){if(primeKeys[i]&(1<<j)){primeRows[i]|=(1<<rows[j]);break;}}
            }
            return nprime;
        }
        constexpr uint16_t generate_sets(bool *set,uint8_t nprime,uint8_t *primeRows,uint32_t *primeCols,bool (*primeSets)[256]){
            uint16_t nset=0;
            uint8_t rowMasks[256]={};uint32_t colMasks[256]={};
            {uint8_t i=0;do{if(set[i]){rowMasks[nset]=(1<<(i/32));colMasks[nset]=((uint32_t)1<<(i%32));nset++;}}while(++i);}
            for(uint16_t i=0;i<nprime;i++){for(uint16_t j=0;j<nset;j++){
                if((primeRows[i]&rowMasks[j])&&(primeCols[i]&colMasks[j])){primeSets[i][j]=1;}
            }}
            return nset;
        }
        template<class Bitset>constexpr void impl(uint8_t nprime,uint16_t nset,bool(*primeSets)[256],uint8_t*classPrimes){
            Bitset setsMap[256]={};
            for(uint8_t i=0;i<nprime;i++){
                for(uint8_t j=0;j<nset;j++){if(primeSets[i][j]){setsMap[i].set(j);}}
                for(uint16_t j=nset;j<Bitset::nbits;j++){setsMap[i].set(j);}
            }
            for(uint8_t i=0;i<8;i++){classPrimes[i]=nprime;}
            while(1){
                uint8_t i=0;
                while(classPrimes[i]--==i){i++;}
                if(i==nclasses){nclasses++;}
                while(i--){classPrimes[i]=classPrimes[i+1]-1;}
                Bitset set;
                i=0;while(i<nclasses){set|=setsMap[classPrimes[i++]];}
                if(set.all_set()){return;}
            }
        }
        template<class word_t,uint8_t bits,uint8_t nwords>class bitset{
            word_t words[nwords];
        public:
#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")
            constexpr static uint16_t nbits=(uint16_t)nwords*bits;
            constexpr bitset():words{}{}
            constexpr void set(uint8_t i){words[i/bits]|=(word_t)1<<(i%bits);}
            constexpr bool all_set()const{
                for(uint8_t i=0;i<nwords;i++){if(words[i]!=(word_t)~0){return false;}}
                return true;
            }
            constexpr bitset&operator|=(const bitset&that){
                for(uint8_t i=0;i<nwords;i++){words[i]|=that.words[i];}
                return *this;
            }
            // __attribute__((always_inline))
#pragma GCC pop_options
        };
    public:
        constexpr Bldr(const char *&src):classes{false,false,false,false,false,false,false,false},nclasses(0){
            bool set[256]={};
            parse(src,set);
            uint8_t primeRows[255]={};uint32_t primeCols[255]={};
            uint8_t nprime=generate_prime(set,primeRows,primeCols);
            
            bool primeSets[255][256]={};
            uint16_t nset=generate_sets(set,nprime,primeRows,primeCols,primeSets);
            uint8_t selected[8]={};
            if(nset==0){throw parse_exception("empty set");}
            else if(nset<=8){impl<bitset<uint8_t,8,1>>(nprime,nset,primeSets,selected);}
            else if(nset<=16){impl<bitset<uint16_t,16,1>>(nprime,nset,primeSets,selected);}
            else if(nset<=32){impl<bitset<uint32_t,32,1>>(nprime,nset,primeSets,selected);}
            else if(nset<=64){impl<bitset<uint32_t,64,1>>(nprime,nset,primeSets,selected);}
            else if(nset<=128){impl<bitset<uint32_t,64,2>>(nprime,nset,primeSets,selected);}
            else if(nset<=192){impl<bitset<uint32_t,64,3>>(nprime,nset,primeSets,selected);}
            else{impl<bitset<uint32_t,64,4>>(nprime,nset,primeSets,selected);}
            for(uint8_t i=0;i<nclasses;i++){classes[i].rows=primeRows[selected[i]];classes[i].cols=primeCols[selected[i]];}
        }
    };
    struct State{
        enum class Type{CHAR,MATCH,SPLIT,SAVE}type;
        constexpr static const Type CHAR=Type::CHAR;
        constexpr static const Type MATCH=Type::MATCH;
        constexpr static const Type SPLIT=Type::SPLIT;
        constexpr static const Type SAVE=Type::SAVE;
        union Out{
            StateIdx out;Out*next;
            constexpr Out(StateIdx o):out(o){}
            constexpr Out():next(nullptr){}
        };
        Out out;
        union{
            CharClass charclass;
            Out out1;
            size_t capture;
        };
        constexpr State():type(MATCH),out(),out1(){};
        constexpr State(Type t,CharClass cc):type(t),out(),charclass(cc){assert(t==CHAR);}
        constexpr State(Type t,size_t c):type(t),out(),capture(c){assert(t==SAVE);}
        constexpr State(Type t):type(t),out(),out1(){assert(t==SPLIT||t==MATCH);}
    };
    struct init_struct{const State*states;StateIdx numStates;size_t numCaptures;};
    enum Flags{MULTILINE=1,DOT_NEWLINES=2,UNGREEDY=4};
    struct Node{
        enum class Type{CHAR,REP_GREEDY,REP_UNGREEDY,CAT,ALT,CAPTURE}type;
        constexpr static const Type CHAR=Type::CHAR;
        constexpr static const Type REP_GREEDY=Type::REP_GREEDY;
        constexpr static const Type REP_UNGREEDY=Type::REP_UNGREEDY;
        constexpr static const Type CAT=Type::CAT;
        constexpr static const Type ALT=Type::ALT;
        constexpr static const Type CAPTURE=Type::CAPTURE;
        union{
            std::nullptr_t none;
            CharClass charclass;
            struct{int min;int max;}repeat;
            Node* left;
        };
        constexpr Node():type(CAPTURE),none(){};
        constexpr Node(Type t,CharClass c):type(t),charclass(c){assert(t==CHAR);}
        constexpr Node(Type t):type(t),none(){assert(t==CAPTURE);}
        constexpr Node(Type t,int min,int max):type(t),repeat({min,max}){assert(t==REP_GREEDY||t==REP_UNGREEDY);}
        constexpr Node(Type t,Node* l):type(t),left(l){assert(t==CAT||t==ALT);}
    };
    struct Sizes{
        size_t nodes;
        StateIdx states;
        size_t captures;
        class Calculate{
            const char * src;
        public:
            constexpr Calculate(const char *source):src(source){}
            constexpr Sizes run(){
                Sizes ret=alternate();
                if(*src){throw parse_exception("mismatched ()");}
                ret.states++;
                return ret;
            }
        private:
            constexpr Sizes atom(){
                if(('A'<=*src&&*src<='Z')||('a'<=*src&&*src<='z')){
                    src++;return Sizes{1,1,0};
                }
                switch(*(src++)){
                case '.':return Sizes{1,1,0};
                case '\\':
                    switch(*src){
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        throw parse_exception("no backreferences");
                    case '0':case 'f':case 'n':case 'r':case 't':case 'v':
                        src++;return Sizes{1,1,0};
                    case 'c':{
                        if(!isalpha(src[1])){throw parse_exception("bad control escape");}
                        src+=2;return Sizes{1,1,0};
                    }
                    case 'x':{
                        if(!(isxdigit(src[1])&&isxdigit(src[2]))){throw parse_exception("bad hex escape");}
                        src+=3;return Sizes{1,1,0};
                    }
                    case '\\':case ':':case '.':case '?':case '*':case '+':case '{':case '}':case '[':case '|':case ']':case '(':case ')':
                        src++;return Sizes{1,1,0};
                    default:throw parse_exception("unknown escape");
                    }
                case '[':{
                    size_t count=CharClass::Bldr(src).nclasses*2-1;
                    return Sizes{count,count,0};
                }
                case '(':{
                    if(*src=='?'){
                        for(src++;*src!='-'&&*src!=':'; src++){
                            switch(*src){case 'm':case 's':case 'U':break;default: throw parse_exception("bad flag");}
                        }
                        if(*src=='-'){for(src++;*src!=':'; src++){
                            switch(*src){case 'm':case 's':case 'U':break;default: throw parse_exception("bad flag");}
                        }}
                        src++;
                        Sizes ret=alternate();
                        if(*(src++)!=')')throw parse_exception("mismatched ()");
                        return ret;
                    }else{
                        Sizes ret=alternate();
                        if(*(src++)!=')')throw parse_exception("mismatched ()");
                        ret.nodes++;ret.states+=2;ret.captures+=2;return ret;
                    }
                }
                default:throw parse_exception("unknown character");
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
                    for(src++;*src!=','&&*src!='}'; src++){
                        if(*src<'0'||'9'<*src){throw parse_exception("not digit");}
                        min*=10;min+=*src-'0';
                    }
                    if(*src==','){
                        for(src++;*src!='}'; src++){
                            if(*src<'0'||'9'<*src){throw parse_exception("not digit");}
                            max*=10;max+=*src-'0';
                        }
                        if(max<min){throw parse_exception("max<min");}
                    }else{max=min;}
                    if(max==0){throw parse_exception("max=0");}
                    break;
                default:return ret;
                }
                if(*(++src)=='?'){src++;}
                ++ret.nodes;
                if(min==0){
                    if(max==0){ret.states=ret.states+1;}// [0,inf)
                    else{ret.states=max*(ret.states+1);ret.captures*=max;}// [0,max]
                }else{
                    if(max==0){ret.states=min*ret.states+1;ret.captures*=min;}// [min,inf)
                    else{ret.states=max*ret.states+(max-min);ret.captures*=max;}// [min,max]
                }
                return ret;
            }
            constexpr Sizes catenate(){
                Sizes ret=repeat();
                while(*src!='|'&&*src!=')'&&*src!='\0'){
                    Sizes add=repeat();
                    ret.nodes+=add.nodes+1;
                    ret.states+=add.states;
                    ret.captures+=add.captures;
                }
                return ret;
            }
            constexpr Sizes alternate(){
                Sizes ret=catenate();
                while(*src=='|'){
                    src++;
                    Sizes add=catenate();
                    ret.nodes+=add.nodes+1;
                    ret.states+=add.states+1;
                    ret.captures+=add.captures;
                }
                return ret;
            }
        };
    };
    class Parser{
        Flags currentFlags;
        const char * src;
        Node* nodes;
    public:
        constexpr Parser(const char *source,Node*buffer)
            :currentFlags(Flags(0)),src(source),nodes(buffer){alternate();assert(!*src);}
    private:
        constexpr void atom(){
            if(('A'<=*src&&*src<='A')||('a'<=*src&&*src<='z')){
                *(nodes++)=Node(Node::CHAR,CharClass(*(src++)));
                return;
            }
            switch(*(src++)){
            case '.':*(nodes++)=Node(Node::CHAR,CharClass(true));return;
            case '\\':
                switch(*src){
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                    assert(false&&"no backreferences");
                case '0':*(nodes++)=Node(Node::CHAR,CharClass('\0'));src++;return;
                case 'f':*(nodes++)=Node(Node::CHAR,CharClass('\f'));src++;return;
                case 'n':*(nodes++)=Node(Node::CHAR,CharClass('\n'));src++;return;
                case 'r':*(nodes++)=Node(Node::CHAR,CharClass('\r'));src++;return;
                case 't':*(nodes++)=Node(Node::CHAR,CharClass('\t'));src++;return;
                case 'v':*(nodes++)=Node(Node::CHAR,CharClass('\v'));src++;return;
                case 'c':{
                    assert(isalpha(src[1]));
                    *(nodes++)=Node(Node::CHAR,CharClass(src[1]&0x1f));src+=2;return;
                }
                case 'x':{
                    assert(isxdigit(src[1])&&isxdigit(src[2]));
                    *(nodes++)=Node(Node::CHAR,CharClass(decodehex(src[1],src[2])));
                    src+=3;return;
                }
                case '\\':case ':':case '-':case '.':case '?':case '*':case '+':case '{':case '}':case '[':case '|':case ']':case '(':case ')':
                    *(nodes++)=Node(Node::CHAR,CharClass(*(src++)));return;
                default:assert(false&&"unknown escape");
                }
                case '[':{
                    CharClass::Bldr builder=CharClass::Bldr(src);
                    *(nodes++)=Node(Node::CHAR,builder.classes[0]);
                    for(int i=1;i<builder.nclasses;i++){
                        nodes[0]=Node(Node::CHAR,builder.classes[i]);
                        nodes[1]=Node(Node::ALT,nodes-1);
                        nodes+=2;
                    }
                    return;
                }
            case '(':{
                if(*src=='?'){
                    Flags parentFlags=currentFlags;
                    for(src++;*src!='-'&&*src!=':'; src++){
                        switch(*src){
                        case 'm':currentFlags=Flags(currentFlags|MULTILINE);break;
                        case 's':currentFlags=Flags(currentFlags|DOT_NEWLINES);break;
                        case 'U':currentFlags=Flags(currentFlags|UNGREEDY);break;
                        default:assert(false&&"bad flag");
                        }
                    }
                    if(*src=='-'){for(src++;*src!=':'; src++){
                        switch(*src){
                        case 'm':currentFlags=Flags(currentFlags&~MULTILINE);break;
                        case 's':currentFlags=Flags(currentFlags&~DOT_NEWLINES);break;
                        case 'U':currentFlags=Flags(currentFlags&~UNGREEDY);break;
                        default:assert(false&&"bad flag");
                        }
                    }}
                    src++;
                    alternate();
                    assert(*src==')');
                    src++;currentFlags=parentFlags;
                    return;
                }else{
                    alternate();
                    assert(*src==')');
                    src++;*(nodes++)=Node(Node::CAPTURE);
                    return;
                }
            }
            default:assert(false&&"unknown character");
            }
        }
        constexpr void repeat(){
            atom();
            int min=0,max=0;
            switch(*src){
            case '?':min=0;max=1;break;
            case '*':min=0;max=0;break;
            case '+':min=1;max=0;break;
            case '{':
                for(src++;*src!=','&&*src!='}'; src++){
                    assert('0'<=*src&&*src<='9');
                    min*=10;min+=*src-'0';
                }
                if(*src==','){
                    for(src++;*src!='}'; src++){
                        assert('0'<=*src&&*src<='9');
                        max*=10;max+=*src-'0';
                    }
                    assert(max>=min);
                }else{max=min;}
                assert(max);
                break;
            default:return;
            }
            bool greedy=(currentFlags&UNGREEDY)==0;
            if(*(++src)=='?'){src++;greedy=!greedy;}
            *(nodes++)=Node(greedy?Node::REP_GREEDY:Node::REP_UNGREEDY,min,max);
        }
        constexpr void catenate(){
            repeat();
            while(*src!='|'&&*src!=')'&&*src!='\0'){
                Node* left=nodes-1;
                repeat();
                *(nodes++)=Node(Node::CAT,left);
            }
        }
        constexpr void alternate(){
            catenate();
            while(*src=='|'){
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
                assert(!value->next);
            }
            constexpr __attribute__((always_inline)) void push(State::Out*value){
                assert((!first)==(!last));
                assert(!value->next);
                if(last){assert(!last->next);last->next=value;}else{first=value;}
                last=value;
            }
            constexpr __attribute__((always_inline)) void append(OutList&&other){append(other);}
            constexpr __attribute__((always_inline)) void append(OutList&other){
                assert((!first)==(!last));
                assert((!other.first)==(!other.last));
                if(last){
                    assert(!last->next);
                    if(other.last){assert(!other.last->next);last->next=other.first;last=other.last;}
                }else{
                    if(other.last){assert(!other.last->next);}
                    first=other.first;last=other.last;
                }
                other.first=other.last=nullptr;
            }
            constexpr void patch(StateIdx state){
                assert((!first)==(!last));
                if(last){assert(!last->next);}
                for(;first;){State::Out*next=first->next;*first=state;first=next;}
                last=nullptr;
            }
        };
        State* states;
        StateIdx numStates;
        public:
        size_t numCaptures;
        constexpr Compiler(State*buffer):states(buffer),numStates(0),numCaptures(0){}
        constexpr void run(Node* node){build(node).patch(create(State::MATCH));}
        private:
        struct IdxWithPtr{
            StateIdx idx;State* ptr;
            constexpr operator StateIdx(){return idx;}
            constexpr operator State*(){return ptr;}
            constexpr State* operator->(){return ptr;}
        };
        constexpr IdxWithPtr __attribute__((always_inline)) next(){return {numStates,&states[numStates]};}
        template<typename... Args>constexpr IdxWithPtr __attribute__((always_inline)) create(Args... args){
            IdxWithPtr ret=next();numStates++;
            *ret=State(args...);
            return ret;
        }
        constexpr OutList build(Node* node){
            switch(node->type){
            case Node::CHAR:{
                IdxWithPtr state=create(State::CHAR,node->charclass);
                return &state->out;
            }
            case Node::REP_GREEDY:case Node::REP_UNGREEDY:{
                int min=node->repeat.min, max=node->repeat.max;
                const bool greedy=node->type==Node::REP_GREEDY;
                const auto set_cont=[greedy](State* state,StateIdx out){if(greedy){state->out=out;}else{state->out1=out;}};
                const auto get_stop=[greedy](State* state){return greedy?&(state->out1):&(state->out);};
                if(min==0){
                    if(max==0){// [0,inf)
                        IdxWithPtr split=create(State::SPLIT);
                        set_cont(split,next());
                        build(node-1).patch(split);
                        return get_stop(split);
                    }else{// [0,max]
                        IdxWithPtr start=create(State::SPLIT);
                        set_cont(start,next());
                        OutList stops=get_stop(start);
                        OutList last=build(node-1);
                        for(int i=1;i<max;i++){
                            last.patch(next());
                            IdxWithPtr split=create(State::SPLIT);
                            set_cont(split,next());
                            stops.push(get_stop(split));
                            last=build(node-1);
                        }
                        last.append(stops);
                        return last;
                    }
                }else{
                    if(max==0){// [min,inf)
                        StateIdx loop=next();
                        for(int i=0;i<min;i++){loop=next();build(node-1).patch(next());}
                        IdxWithPtr split=create(State::SPLIT);
                        set_cont(split,loop);
                        return get_stop(split);
                    }else{// [min,max]
                        OutList ret=build(node-1);
                        for(int i=1;i<min;i++){
                            ret.patch(next());
                            ret=build(node-1);
                        }
                        OutList stops;
                        for(int i=min;i<max;i++){
                            ret.patch(next());
                            IdxWithPtr split=create(State::SPLIT);
                            set_cont(split,next());
                            stops.push(get_stop(split));
                            ret=build(node-1);
                        }
                        ret.append(stops);
                        return ret;
                    }
                }
            } // end of Node::REP case
            case Node::CAT:{
                build(node->left).patch(next());
                return build(node-1);
            }
            case Node::ALT:{
                IdxWithPtr start=create(State::SPLIT);
                start->out=(StateIdx)next();
                OutList ret=build(node->left);
                start->out1=(StateIdx)next();
                ret.append(build(node-1));
                return ret;
            }
            case Node::CAPTURE:{
                IdxWithPtr start = create(State::SAVE,numCaptures);
                start->out=(StateIdx)next();
                numCaptures+=2;
                build(node-1).patch(next());
                IdxWithPtr end=create(State::SAVE,start->capture+1);
                return &end->out;
            }
            default:throw "bad node type";
            }
        }
    };
    template<const Sizes& sizes>constexpr static auto gen_states(const char*src){
        std::array<State,sizes.states>states;
        std::array<Node,sizes.nodes>nodes;
        Parser(src,nodes.data());
        Compiler(states.data()).run(&nodes.back());
        return states;
    }
    template<char... chars>class init_class{
        constexpr static const char source[]={chars...};
        constexpr static Sizes sizes=Sizes::Calculate(source).run();
        constexpr static auto states=gen_states<sizes>(source);
    public:
        constexpr static init_struct value={states.data(),sizes.states,sizes.captures};
        
    };
}
constexpr NanoRegex::NanoRegex(const init_struct&init):
states(init.states),numStates(init.numStates),shouldDelete(false),numCaptures(init.numCaptures){}
template<typename T, T... chars>constexpr NanoRegex_detail::init_struct operator""_regex(){
    return NanoRegex_detail::init_class<chars...,'\0'>::value;
}
