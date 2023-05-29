#include "parse.hpp"
static Flags currentFlags;
static char* src;
static int srcIdx;
std::vector<std::any> nodes;
std::vector<std::any>stack;
static void alternate();
static void atom(){
	if(('A'<=src[srcIdx]&&src[srcIdx]<='A')||('a'<=src[srcIdx]&&src[srcIdx]<='z')){
		Node::create(Node::CHAR,src[srcIdx++]);return;
	}
	switch(src[srcIdx]){
	case '.':Node::create(Node::ANY);return;
	case '\\':
		switch(src[++srcIdx]){
			case '\\':case '.':case '?':case '*':case '+':case '{':case '}':case '|':case '(':case ')':
				Node::create(Node::CHAR,src[srcIdx++]);return;
			default: throw "unknown escape";
		}
	case '(':{
		if(src[srcIdx+1]=='?'){
			stack.push_back(currentFlags);
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
			alternate();
			if(src[srcIdx++]!=')')throw "unexpected end of string";
			currentFlags=std::any_cast<Flags>(stack.back());stack.pop_back();
			return;
		}else{
			srcIdx++;
			alternate();
			if(src[srcIdx++]!=')')throw "unexpected end of string";
			NodePtr node=std::any_cast<NodePtr>(stack.back());stack.pop_back();
			Node::create(Node::CAPTURE,node);return;
		}
	}
	default: throw "unknown character";
	}
}
static void repeat(){
	atom();
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
		default:return;
	}
	bool greedy=(currentFlags&UNGREEDY)==0;
	if(src[++srcIdx]=='?'){srcIdx++;greedy=!greedy;}
	NodePtr node=std::any_cast<NodePtr>(stack.back());stack.pop_back();
	Node::create(Node::REP,node,min,max,greedy);
}
static void catenate(){
	repeat();
	while(src[srcIdx]!='|'&&src[srcIdx]!=')'&&src[srcIdx]!='\0'){
		repeat();
		NodePtr right=std::any_cast<NodePtr>(stack.back());stack.pop_back();
		NodePtr left=std::any_cast<NodePtr>(stack.back());stack.pop_back();
		Node::create(Node::CAT,left,right);
	}
}
static void alternate(){
	catenate();
	while(src[srcIdx]=='|'){
		srcIdx++;
		catenate();
		NodePtr right=std::any_cast<NodePtr>(stack.back());stack.pop_back();
		NodePtr left=std::any_cast<NodePtr>(stack.back());stack.pop_back();
		Node::create(Node::ALT,left,right);
	}
}
void Node::clear(){nodes.clear();}
NodePtr Node::parse(char *source) {
	currentFlags=Flags(0);
	src=source;
	srcIdx=0;
	alternate();
	NodePtr node=std::any_cast<NodePtr>(stack.back());stack.pop_back();
	return node;
};