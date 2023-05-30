#include "nanoRegex.hpp"
#include <cstdio>
int main(int argc, char **argv) {
	if(argc < 3){
		fprintf(stderr, "usage: %s regexp string...\n",argc>0?argv[0]:"progname");
		return 1;
	}
	try {
		NanoRegex re = NanoRegex(argv[1]);
		fprintf(stderr, "Regex %s (%d captures)\n",argv[1],re.captures());
		fprintf(stderr, "matches:\n");
		for(int i=2; i<argc; i++){
			std::string::iterator* match=re.match(argv[i]);
			if(match!=nullptr)printf("%s %s\n", argv[i],std::string(match[0],match[1]).c_str());
		}
		return 0;
	} catch (const char *err) {
		fprintf(stderr, "%s\n",err);
		return 1;
	}
}