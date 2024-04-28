#include "nanoRegex.hpp"
#include <cassert>
#include <cstdint>
#include <array>
//include <memory> ?
namespace NanoRegex_detail{
    using parse_exception=NanoRegex::parse_exception;
    constexpr std::uint8_t decodehex(std::uint8_t u,std::uint8_t v){return(u<<4)+(u>'9'?144:0)+(v&0x0f)+(v>'9'?9:0);}
    struct CharClass{
        std::uint8_t select;
        std::uint64_t row;
        constexpr CharClass():select(0),row(0){};
        constexpr CharClass(std::uint8_t s,std::uint64_t r):select(s),row(r){};
        constexpr bool has(std::uint8_t c)const{
            std::uint8_t val=row>>(c&0x3f),ignore=select>>(4+(c>>6)),invert=select>>(c>>6);
            return ((val&~ignore)^invert)&1;
        };
        struct Bldr;
    };
    class bitset256{
        std::uint64_t bits[4];
        constexpr bitset256(std::nullptr_t,std::uint64_t row):bits{row,row,row,row}{};
    public:
        constexpr bitset256(bool b):bitset256(nullptr,-(std::uint64_t)b){};
        constexpr bitset256(std::uint8_t i):bitset256(false){bits[i/64]=(std::uint64_t)1<<(i%64);};
        constexpr bitset256(char i):bitset256((std::uint8_t)i){};
        constexpr bool has(std::uint8_t i)const{return bits[i/64] & ((std::uint64_t)1<<(i%64));}
        constexpr explicit bitset256(CharClass c):bitset256(false){
            for(std::uint8_t i=0;i<4;i++){
                std::uint64_t ignore=-((c.select>>(4+i))&1),invert=-((c.select>>i)&1);
                bits[i]=(c.row&~ignore)^invert;
            }
        }
        constexpr std::uint8_t build(CharClass*classes)const{
            std::uint64_t interesting[4]={0,0,0,0};
            std::uint8_t masks[4]={0,0,0,0};
            std::uint8_t nInteresting=0,select=0;
            for(std::uint8_t i=0;i<4;i++){if(bits[i]+1<=1){select|= (bits[i] & 0x01<<i) | 0x10<<i;}else{
                std::uint8_t j=0;
                while(1){
                    if(j>=4){throw "???";}
                    if(bits[i]==interesting[j]){masks[j]|=1<<i;break;}
                    if(j==nInteresting){masks[j]|=1<<i;interesting[j]=bits[i];nInteresting++;break;}
                    ++j;
                }
            }}
            if(nInteresting==0){classes[0]=CharClass(select&0x0f,0);return 1;}
            if(nInteresting==1){classes[0]=CharClass(select,interesting[0]);return 1;}
            bool oneClass=true;
            for(std::uint8_t i=1;i<nInteresting;i++){
                if((interesting[0]^interesting[i])+1>1){oneClass=false;break;}
            }
            if(oneClass){
                for(std::uint8_t i=1;i<nInteresting;i++){select|=(interesting[0]^interesting[i])&masks[i];}
                classes[0]=CharClass(select,interesting[0]);
                return 1;
            }
            if(nInteresting==2){
                classes[0]=CharClass(select|masks[1]<<4,interesting[0]);
                classes[1]=CharClass(select|masks[0]<<4,interesting[1]);
                return 2;
            }
            if(nInteresting==3){
                classes[0]=CharClass(select|(masks[1]|masks[2])<<4,interesting[0]);
                classes[1]=CharClass(select|(masks[0]|masks[2])<<4,interesting[1]);
                classes[2]=CharClass(select|(masks[0]|masks[1])<<4,interesting[2]);
                return 3;
            }else{// nInteresting==4
                classes[0]=CharClass(select|(masks[1]|masks[2]|masks[3])<<4,interesting[0]);
                classes[1]=CharClass(select|(masks[0]|masks[2]|masks[3])<<4,interesting[1]);
                classes[2]=CharClass(select|(masks[0]|masks[1]|masks[3])<<4,interesting[2]);
                classes[3]=CharClass(select|(masks[0]|masks[1]|masks[2])<<4,interesting[3]);
                return 4;
            }
        }
        constexpr explicit bitset256(const char *&src):bitset256(false){
            if(*src==']'){throw parse_exception("empty set");}
            bool invert=(*src=='^');if(invert){src++;}
            while(*src!=']'){
                switch(*src){
                case '\0':throw parse_exception("unterminated character class");
                case '[':throw parse_exception("no named character classes... yet.");
                case '\\':{
                    switch(*++src){
                    case '0':bits[0]|=1<<'\0';src++;break;
                    case 'f':bits[0]|=1<<'\f';src++;break;
                    case 'n':bits[0]|=1<<'\n';src++;break;
                    case 'r':bits[0]|=1<<'\r';src++;break;
                    case 't':bits[0]|=1<<'\t';src++;break;
                    case 'v':bits[0]|=1<<'\v';src++;break;
                    case 'c':{
                        if(!isalpha(src[1])){throw parse_exception("bad control escape");}
                        bits[0]|=1<<(src[1]&0x1f);src+=2;break;
                    }
                    case 'x':{
                        if(!(isxdigit(src[1])&&isxdigit(src[2]))){throw parse_exception("bad hex escape");}
                        std::uint8_t c=decodehex(src[1],src[2]);
                        bits[c/64]|=(std::uint64_t)1<<(c%64);src+=3;break;
                    }
                    case '\\':case '-':case ':':case '.':case '?':case '*':case '+':case '{':case '}':case '[':case '|':case ']':case '(':case ')':{
                        std::uint8_t c=*src++;bits[c/64]|=(std::uint64_t)1<<(c%64);break;
                    }
                    default:throw parse_exception("unknown escape");
                    }
                    break;
                }
                default:{std::uint8_t c=*src++;bits[c/64]|=(std::uint64_t)1<<(c%64);break;}
                }
            }
            src++;
            if(invert){bits[0]=~bits[0];bits[1]=~bits[1];bits[2]=~bits[2];bits[3]=~bits[3];}
        }
    };
    enum Flags{MULTILINE=1,DOT_NEWLINES=2,UNGREEDY=4};
    struct Sizes{
        std::size_t nodes;
        std::size_t charStates;
        std::size_t splitStates;
        std::size_t saveStates;
        class Calculate{
            const char * src;
        public:
            constexpr Calculate(const char *source):src(source){}
            constexpr Sizes run(){
                Sizes ret=alternate();
                if(*src){throw parse_exception("mismatched ()");}
                return ret;
            }
        private:
            constexpr Sizes atom(){
                if(('A'<=*src&&*src<='Z')||('a'<=*src&&*src<='z')){
                    src++;return Sizes{1,1,0,0};
                }
                switch(*src++){
                case '.':return Sizes{1,1,0,0};
                case '\\':
                    switch(*src){
                    case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                        throw parse_exception("no backreferences");
                    case '0':case 'f':case 'n':case 'r':case 't':case 'v':
                        src++;return Sizes{1,1,0,0};
                    case 'c':{
                        if(!isalpha(src[1])){throw parse_exception("bad control escape");}
                        src+=2;return Sizes{1,1,0,0};
                    }
                    case 'x':{
                        if(!(isxdigit(src[1])&&isxdigit(src[2]))){throw parse_exception("bad hex escape");}
                        src+=3;return Sizes{1,1,0,0};
                    }
                    case '\\':case ':':case '.':case '?':case '*':case '+':case '{':case '}':case '[':case '|':case ']':case '(':case ')':
                        src++;return Sizes{1,1,0,0};
                    default:throw parse_exception("unknown escape");
                    }
                case '[':{
                    CharClass ignored[4]={};
                    std::uint8_t nclasses=bitset256(src).build(ignored);
                    return Sizes{1,nclasses,nclasses-1u,0};
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
                        if(*src++!=')')throw parse_exception("mismatched ()");
                        return ret;
                    }else{
                        Sizes ret=alternate();
                        if(*src++!=')')throw parse_exception("mismatched ()");
                        ret.nodes++;ret.saveStates+=2;return ret;
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
                if(*++src=='?'){src++;}
                ++ret.nodes;
                if(min==0){
                    if(max==0){// [0,inf)
                        ret.splitStates++;
                    }else{// [0,max]
                        ret.charStates*=max;ret.splitStates=max*(ret.splitStates+1);ret.saveStates*=max;
                    }
                }else{
                    if(max==0){// [min,inf)
                        ret.charStates*=min;ret.splitStates=min*ret.splitStates+1;ret.saveStates*=min;
                    }else{// [min,max]
                        ret.charStates*=max;ret.splitStates=max*(ret.splitStates+1)-min;ret.saveStates*=max;
                    }
                }
                return ret;
            }
            constexpr Sizes catenate(){
                Sizes ret=repeat();
                while(*src!='|'&&*src!=')'&&*src!='\0'){
                    Sizes add=repeat();
                    ret.nodes+=add.nodes+1;ret.charStates+=add.charStates;
                    ret.splitStates+=add.splitStates;ret.saveStates+=add.saveStates;
                }
                return ret;
            }
            constexpr Sizes alternate(){
                Sizes ret=catenate();
                while(*src=='|'){
                    src++;
                    Sizes add=catenate();
                    ret.nodes+=add.nodes+1;ret.charStates+=add.charStates;
                    ret.splitStates+=add.splitStates+1;ret.saveStates+=add.saveStates;
                }
                return ret;
            }
        };
    };
    struct Parse{
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
                bitset256 charclass;
                struct{Node* content;int min;int max;}repeat;
                struct{Node* left;Node*right;}binary;
                struct{Node* content;std::size_t idx;}capture;
            };
            std::size_t charStates;
            std::size_t epsStates;
            constexpr Node():type(CAPTURE),none(),charStates(0),epsStates(0){};
            constexpr Node(Type t,bitset256 c,int nclasses=1)
                :type(t),charclass(c),charStates(nclasses),epsStates(nclasses-1){assert(t==CHAR);}
            constexpr Node(Type t,Node* i,int min,int max)
                :type(t),repeat({i,min,max}),charStates(i->charStates),epsStates(i->epsStates){
                    assert(t==REP_GREEDY||t==REP_UNGREEDY);
                    if(min==0){
                        epsStates++;
                        if(max){charStates*=max;epsStates*=max;}
                    }else{
                        if(max==0){charStates*=min;epsStates=min*epsStates+1;}
                        else{charStates*=max;epsStates=max*(epsStates+1)-min;}
                    }
                }
            constexpr Node(Type t,Node* l,Node* r):type(t),binary({l,r}),
            charStates(l->charStates+r->charStates),
            epsStates((t==ALT)+l->epsStates+r->epsStates){assert(t==CAT||t==ALT);}
            constexpr Node(Type t,Node* c,std::size_t i)
                :type(t),capture({c,i}),charStates(c->charStates),epsStates(c->epsStates+2){assert(t==CAPTURE);}
        };
        constexpr Parse(const char *source,Node*buffer)
            :currentFlags(Flags(0)),captureIdx(0),src(source),nodes(buffer){alternate();assert(!*src);}
    private:
        Flags currentFlags;std::size_t captureIdx;
        const char * src;
        Node* nodes;
        constexpr void atom(){
            if(('A'<=*src&&*src<='A')||('a'<=*src&&*src<='z')){
                *nodes++=Node(Node::CHAR,bitset256(*src++));
                return;
            }
            switch(*src++){
            case '.':*nodes++=Node(Node::CHAR,bitset256(true));return;
            case '\\':
                switch(*src){
                case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
                    assert(false&&"no backreferences");
                case '0':*nodes++=Node(Node::CHAR,bitset256('\0'));src++;return;
                case 'f':*nodes++=Node(Node::CHAR,bitset256('\f'));src++;return;
                case 'n':*nodes++=Node(Node::CHAR,bitset256('\n'));src++;return;
                case 'r':*nodes++=Node(Node::CHAR,bitset256('\r'));src++;return;
                case 't':*nodes++=Node(Node::CHAR,bitset256('\t'));src++;return;
                case 'v':*nodes++=Node(Node::CHAR,bitset256('\v'));src++;return;
                case 'c':{
                    assert(isalpha(src[1]));
                    *nodes++=Node(Node::CHAR,bitset256(std::uint8_t(src[1]&0x1f)));src+=2;return;
                }
                case 'x':{
                    assert(isxdigit(src[1])&&isxdigit(src[2]));
                    *nodes++=Node(Node::CHAR,bitset256(decodehex(src[1],src[2])));
                    src+=3;return;
                }
                case '\\':case ':':case '-':case '.':case '?':case '*':case '+':case '{':case '}':case '[':case '|':case ']':case '(':case ')':
                    *nodes++=Node(Node::CHAR,bitset256(*src++));return;
                default:assert(false&&"unknown escape");
                }
                case '[':{
                    bitset256 set=bitset256(src);
                    CharClass ignored[4]={};
                    *nodes++=Node(Node::CHAR,set,set.build(ignored));
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
                    Node*content=nodes-1;
                    assert(*src==')');
                    ++src;*nodes++=Node(Node::CAPTURE,content,captureIdx++);
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
            if(*++src=='?'){src++;greedy=!greedy;}
            Node*content=nodes-1;
            *nodes++=Node(greedy?Node::REP_GREEDY:Node::REP_UNGREEDY,content,min,max);
        }
        constexpr void catenate(){
            repeat();
            while(*src!='|'&&*src!=')'&&*src!='\0'){
                Node* left=nodes-1;
                repeat();
                Node* right=nodes-1;
                *nodes++=Node(Node::CAT,left,right);
            }
        }
        constexpr void alternate(){
            catenate();
            while(*src=='|'){
                src++;
                Node* left=nodes-1;
                catenate();
                Node* right=nodes-1;
                *nodes++=Node(Node::ALT,left,right);
            }
        }
    };
    struct NFAEpsState{
        enum class Type:std::uint8_t{CHAR,SPLIT=CHAR+240,SAVE}type;
        struct char_t{};struct split_t{};struct save_t{};
        const NFAEpsState*out;
        union{
            std::uint64_t charclass;
            const NFAEpsState*out1;
            std::size_t capture;
        };
        constexpr NFAEpsState():type(Type::CHAR),out(),out1(){};
        constexpr NFAEpsState(char_t,CharClass cc,const NFAEpsState* o):type((Type)cc.select),out(o),charclass(cc.row){}
        constexpr NFAEpsState(split_t,const NFAEpsState* o,const NFAEpsState* o1):type(Type::SPLIT),out(o),out1(o1){}
        constexpr NFAEpsState(save_t,std::size_t c,const NFAEpsState* o):type(Type::SAVE),out(o),capture(c){}
    };
    class RegEx2NFAEps{
        NFAEpsState* states;
        using Node=Parse::Node;
        public:
        constexpr RegEx2NFAEps(const Node& node,NFAEpsState*buffer)
            :states(buffer){build(node,buffer+node.charStates+node.epsStates);}
        private:
        constexpr void build(const Node& node,const NFAEpsState* dest){
            switch(node.type){
            case Node::CHAR:{
                CharClass classes[4]={};
                std::uint8_t nclasses=node.charclass.build(classes);
                for(int i=1;i<nclasses;i++){
                    *states=NFAEpsState(NFAEpsState::split_t{},states+1,states+nclasses);
                    ++states;
                }
                for(int i=0;i<nclasses;i++){
                    *states++=NFAEpsState(NFAEpsState::char_t{},classes[i],dest);
                }
                return;
            }
            case Node::REP_GREEDY:case Node::REP_UNGREEDY:{
                const bool greedy=node.type==Node::REP_GREEDY;
                const auto make_split=[greedy,dest,this](NFAEpsState* cont){
                    if(greedy){*states++=NFAEpsState(NFAEpsState::split_t{},cont,dest);}
                    else{*states++=NFAEpsState(NFAEpsState::split_t{},dest,cont);}
                };
                int min=node.repeat.min, max=node.repeat.max;
                const Node& content=*node.repeat.content;
                std::size_t contentStates=content.epsStates+content.charStates;
                if(min==0){
                    if(max==0){// [0,inf)
                        make_split(states+1);
                        build(content,states-1);
                        return;
                    }else{// [0,max]
                        for(int i=0;i<max-1;i++){make_split(states+1);build(content,states+contentStates);}
                        make_split(states+1);
                        build(content,dest);
                        return;
                    }
                }else{
                    if(max==0){// [min,inf)
                        for(int i=0;i<min;i++){build(content,states+contentStates);}
                        make_split(states-contentStates);
                        return;
                    }else{// [min,max]
                        for(int i=0;i<min;i++){build(content,states+contentStates);}
                        for(int i=min;i<max-1;i++){
                            make_split(states+1);
                            build(content,states+contentStates);
                        }
                        if(min!=max){make_split(states+1);build(content,dest);}
                        return;
                    }
                }
            } // end of Node::REP case
            case Node::CAT:{
                build(*node.binary.left,states+node.binary.left->epsStates+node.binary.left->charStates);
                build(*node.binary.right,dest);
                return;
            }
            case Node::ALT:{
                *states=NFAEpsState(NFAEpsState::split_t{},states+1,states+1+node.binary.left->epsStates+node.binary.left->charStates);
                states++;
                build(*node.binary.left,dest);
                build(*node.binary.right,dest);
                return;
            }
            case Node::CAPTURE:{
                *states=NFAEpsState(NFAEpsState::save_t{},node.capture.idx*2,states+1);
                const Node& content=*node.capture.content;
                build(content,++states+content.epsStates+content.charStates);
                *states++=NFAEpsState(NFAEpsState::save_t{},node.capture.idx*2+1,dest);
                return;
            }
            default:throw "bad node type";
            }
        }
    };
    struct NFAState{
        struct{StateIdx out;size_t*captures;}*transitions[256];
    };
    struct UnoptimizedState{
        using Type=NFAEpsState::Type;
        Type type;
        StateIdx out;
        union{
            uint64_t charclass;
            StateIdx out1;
            std::size_t capture;
        };
        constexpr UnoptimizedState():type(Type::CHAR),out(),out1(){};
        constexpr UnoptimizedState(const NFAEpsState*base,const NFAEpsState&that)
            :type(that.type),out(that.out-base),out1(){switch(type){
            default:charclass=that.charclass;break;
            case Type::SPLIT:out1=that.out1-base;break;
            case Type::SAVE:capture=that.capture;break;
            }}
    };
    struct init_struct{const UnoptimizedState*states;std::size_t numStates;std::size_t numCaptures;};
    template<char... chars>class init_class{
        constexpr static const char source[]={chars...};
        constexpr static Sizes sizes=Sizes::Calculate(source).run();
        constexpr static std::size_t numStates=sizes.charStates+sizes.splitStates+sizes.saveStates;
        std::array<UnoptimizedState,numStates>states;
        constexpr init_class():states(){
            std::array<Parse::Node,sizes.nodes>nodes;
            Parse(source,nodes.data());
            std::array<NFAEpsState,numStates>nfaEpsStates;
            RegEx2NFAEps(nodes.back(),nfaEpsStates.data());
            std::array<NFAEpsState,numStates>nfaEps!States;
            RegEx2NFAEps(nodes.back(),nfaEpsStates.data());
            for(StateIdx i=0;i<numStates;i++){states[i]=UnoptimizedState{nfaEpsStates.data(),nfaEpsStates[i]};}
        }
        constexpr static init_class init_value{};
    public:
        constexpr static init_struct value={init_value.states.data(),sizes.charStates+sizes.splitStates+sizes.saveStates,sizes.saveStates};
    };
}
constexpr NanoRegex::NanoRegex(const init_struct&init)
    :states(init.states),numStates(init.numStates),shouldDelete(false),numCaptures(init.numCaptures){}
template<typename T, T... chars>constexpr NanoRegex_detail::init_struct operator""_regex(){
    return NanoRegex_detail::init_class<chars...,'\0'>::value;
}