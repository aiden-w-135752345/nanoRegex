#include "main.hpp"
extern std::vector<std::any>stack;
enum Flags{MULTILINE=1,DOT_NEWLINES=2,UNGREEDY=4};
struct Node;
extern std::vector<std::any> nodes;
typedef std::vector<Node>::size_type NodePtr;
struct Node{
	enum Type{CHAR,ANY,REP,CAT,ALT,CAPTURE}type;
	NodePtr left;
	union{
		char c;
		struct{int min;int max;bool greedy;}repeat;
		Flags parentFlags;
		NodePtr right;
	};
	private:
	class Private{friend class Node;private: Private(){}};
	public:
	static void create(Type type,char c){
		if(type!=CHAR)throw"wrong args";
		stack.push_back(nodes.size());
		nodes.push_back(type);nodes.push_back(c);
	}
	static void create(Type type){
		if(type!=ANY){throw"wrong args";}
		stack.push_back(nodes.size());
		nodes.push_back(type);
	}
	static void create(Type type,NodePtr repeated,int min,int max,bool greedy){
		if(type!=Node::REP)throw"wrong args";
		stack.push_back(nodes.size());
		nodes.push_back(type);nodes.push_back(repeated);
		nodes.push_back(min);nodes.push_back(max);nodes.push_back(greedy);
	}
	static void create(Type type,NodePtr left,NodePtr right){
		if(type!=CAT&&type!=ALT)throw"wrong args";
		stack.push_back(nodes.size());
		nodes.push_back(type);nodes.push_back(left);nodes.push_back(right);
	}
	static void create(Type type,NodePtr content){
		if(type!=CAPTURE)throw"wrong args";
		stack.push_back(nodes.size());
		nodes.push_back(type);nodes.push_back(content);
	}
	static void clear();
	static NodePtr parse(char*);
};
