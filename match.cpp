#include "match.hpp"
RegExp* re;
struct Thread;
struct Captures{
	int*refs;
	std::string::iterator*values;
	Captures():refs(new int),values(new std::string::iterator[re->numCaptures]){*refs=1;}
	Captures(nullptr_t):refs(nullptr),values(nullptr){}
	Captures(const Captures&other):refs(other.refs),values(other.values){++*refs;};
	Captures(Captures&& other) noexcept:refs(std::exchange(other.refs, nullptr)),values(std::exchange(other.values, nullptr)){}
	Captures& operator=(const Captures&other)=delete;
	Captures& operator=(Captures&& other) noexcept{
		std::swap(refs,other.refs);
		std::swap(values,other.values);
		return *this;
	};
	~Captures(){
		if(values!=nullptr&&--*refs==0){
			delete refs;delete[]values;values=nullptr;
		}
	}
	Captures update(int idx,std::string::iterator val){
		if(values==nullptr){throw "dead";}
		Captures edited;
		for(int i=0;i<re->numCaptures;i++)edited.values[i]=values[i];
		edited.values[idx]=val;
		return edited;
	}
};
struct Thread{
	StatePtr state;
	Captures captures;
	Thread(StatePtr s,Captures c):state(s),captures(std::move(c)){}
	static void add(StatePtr state,Captures captures,std::vector<Thread>*l,std::string::iterator sp,int gen) {
		if(re->states[state].gen==gen){return;}
		re->states[state].gen=gen;
		if(re->states[state].type == State::SPLIT){
			Thread::add(re->states[state].out,captures,l,sp,gen);
			Thread::add(re->states[state].out1,captures,l,sp,gen);
			return;
		}
		if(re->states[state].type == State::SAVE){
			Thread::add(re->states[state].out,captures.update(re->states[state].capture,sp),l,sp,gen);
			return;
		}
		l->emplace_back(state,captures);
	}
};
static int gen=1;
std::string::iterator* RegExp::match(std::string str) {
	re=this;
	std::vector<Thread>curr,next;
	std::string::iterator iter=str.begin();
	{
		Captures captures;
		for(int i=0;i<this->numCaptures;i++){captures.values[i]=std::string::iterator();}
		Thread::add(this->start,std::move(captures),&curr, iter,gen);
	}
	Captures*match=nullptr;
	for(;;iter++){
		// +++ check cache
		gen++;
		for(Thread&thread:curr){
			if(re->states[thread.state].type==State::MATCH&&iter==str.end()&&match==nullptr){
				match=new Captures(thread.captures);
			}
			if(re->states[thread.state].type==State::CHAR&&iter!=str.end()&&re->states[thread.state].c == *iter){
				Thread::add(re->states[thread.state].out,thread.captures,&next,iter+1,gen);
			}
			if(re->states[thread.state].type == State::ANY&&iter!=str.end()){
				Thread::add(re->states[thread.state].out,thread.captures,&next,iter+1,gen);
			}
		}
		curr.clear();
		curr.swap(next);
		if(iter==str.end()){break;}
	}
	return match!=nullptr?match->values:nullptr;
}