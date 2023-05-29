#include "main.hpp"
struct State;
typedef std::vector<State*>::size_type StatePtr;
class RegExp {
public:
	std::vector<State>states;
	StatePtr start;
	int numCaptures;
	RegExp(char*);
	std::string::iterator* match(std::string);
};


struct State{
	enum Type{CHAR,ANY,MATCH,SPLIT,SAVE}type;
	StatePtr out;
	int gen=0;
	union{
		char c;
		StatePtr out1;
		int capture;
	};
	private:
	class Private{friend class State;private: Private(){}};
	public:
	State(Private,Type t,char cc):type(t),out(-1),c(cc){if(type!=CHAR)throw"wrong args";}
	State(Private,Type t,int cc):type(t),out(-1),capture(cc){if(type!=SAVE)throw"wrong args";}
	State(Private,Type t):type(t),out(-1){if(type!=ANY&&type!=SPLIT&&type!=MATCH)throw"wrong args";}
	template<typename... Args> static StatePtr create(Args... args);
};