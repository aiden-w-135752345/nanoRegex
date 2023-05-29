#include "match.hpp"
#include "parse.hpp"
static RegExp* re;
template<typename... Args> StatePtr State::create(Args... args){
	re->states.emplace_back(Private(),args...);
	return re->states.size()-1;
}
struct Frag {
	StatePtr start;
	struct Patch{
		StatePtr ptr;StatePtr State::*member;
		Patch(StatePtr p,StatePtr State::*m):ptr(p),member(m){}
	};
	std::list<Patch>out;
	Frag(StatePtr s,std::list<Patch>o):start(s),out(o){};
	static Frag build(NodePtr node){
		switch(std::any_cast<Node::Type>(nodes[node])){
			case Node::CHAR:{
				StatePtr state=State::create(State::CHAR,std::any_cast<char>(nodes[node+1]));
				return {state,{Patch(state,&State::out)}};
			}
			case Node::ANY:{
				StatePtr state=State::create(State::ANY);
				return{state,{Patch(state,&State::out)}};
			}
			case Node::REP:{
				int min=std::any_cast<int>(nodes[node+2]);
				int max=std::any_cast<int>(nodes[node+3]);
				int greedy=std::any_cast<bool>(nodes[node+4]);
				StatePtr State::*cont=greedy?&State::out:&State::out1;
				StatePtr State::*stop=greedy?&State::out1:&State::out;
				if(min==0){
					if(max==0){// [0,inf)
						StatePtr split=State::create(State::SPLIT);
						Frag frag=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
						frag.patch(split);
						re->states[split].*cont=frag.start;
						return {split,{Patch(split,stop)}};
					}else{// [0,max]
						StatePtr split=State::create(State::SPLIT);
						Frag frag=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
						std::list<Patch>stops;
						stops.emplace_back(split,stop);
						re->states[split].*cont=frag.start;
						for(int i=1;i<max;i++){
							split=State::create(State::SPLIT);
							frag.patch(split);
							frag=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
							re->states[split].*cont=frag.start;
							stops.emplace_back(split,stop);
						}
						frag.out.splice(frag.out.end(),stops);
						return {split,frag.out};
					}
				}else{
					if(max==0){// [min,inf)
						Frag ret=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
						StatePtr loopback=ret.start;
						for(int i=1;i<min;i++){
							Frag frag=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
							ret.patch(loopback=frag.start);
							ret.out=std::move(frag.out);
						}
						StatePtr split=State::create(State::SPLIT);
						ret.patch(split);
						re->states[split].*cont=loopback;
						return {ret.start,{Patch(split,stop)}};
					}else{// [min,max]
						Frag ret=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
						StatePtr loopback=ret.start;
						for(int i=1;i<min;i++){
							Frag frag=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
							ret.patch(loopback=frag.start);
							ret.out=std::move(frag.out);
						}
						std::list<Patch>stops;
						for(int i=min;i<max;i++){
							StatePtr split=State::create(State::SPLIT);
							ret.patch(split);
							stops.push_back(Patch(split,stop));
							Frag frag=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
							re->states[split].*cont=frag.start;
							ret.out=std::move(frag.out);
						}
						ret.out.splice(ret.out.end(),stops);
						return ret;
					}
				}
			} // end of Node::REP case
			case Node::CAT:{
				Frag left=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
				Frag right=Frag::build(std::any_cast<NodePtr>(nodes[node+2]));
				left.patch(right.start);
				return{left.start,std::move(right.out)};
			}
			case Node::ALT:{
				StatePtr start=State::create(State::SPLIT);
				Frag left=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
				Frag right=Frag::build(std::any_cast<NodePtr>(nodes[node+2]));
				re->states[start].out=left.start;re->states[start].out1=right.start;
				std::list<Patch>outs;
				outs.splice(outs.end(),left.out);outs.splice(outs.end(),right.out);
				return{start,std::move(outs)};
			}
			case Node::CAPTURE:{
				StatePtr start = State::create(State::SAVE,re->numCaptures);
				re->numCaptures+=2;
				Frag content=Frag::build(std::any_cast<NodePtr>(nodes[node+1]));
				re->states[start].out=content.start;
				StatePtr end=State::create(State::SAVE,re->states[start].capture+1);
				content.patch(end);
				return{start,{Patch(end,&State::out)}};
			}
			default:throw "???";
		}
	}
	void patch(StatePtr state){
		if(out.empty()){throw"already patched";}
		for(Patch patch:out){re->states[patch.ptr].*patch.member=state;}out.clear();
	}
};
/* compile regular expression */
RegExp::RegExp(char *source) {
	re=this;
	numCaptures=0;
	NodePtr node=Node::parse(source);
	
	Frag frag=Frag::build(node);
	Node::clear();
	frag.patch(State::create(State::MATCH));
	start=frag.start;
	re=nullptr;
};