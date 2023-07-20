#include "nanoRegex.hpp"
#include <cstdio>
#include <cstring>
NanoRegex test1="fo+ba(r|z)"_regex;
NanoRegex test2=operator""_regex<char,'f', 'o', '+', 'b', 'a', '(', 'r', '|', 'z', ')'>();
int main(int argc, char **argv) {
    (void)test1;(void)test2;
	if(argc < 3){
		fprintf(stderr, "usage: %s regexp string...\n",argc>0?argv[0]:"progname");
		return 1;
	}
	try {
		NanoRegex re = NanoRegex(argv[1]);
		fprintf(stderr, "Regex %s (%d captures)\n",argv[1],re.captures());
		for(int i=2; i<argc; i++){
			char** matches=re.match(argv[i],argv[i]+strlen(argv[i]));
			if(matches!=nullptr){
                printf("%s matches:",argv[i]);
                for(int i=0;i<re.captures();i+=2){
                    putchar(' ');
                    size_t len=matches[i+1]-matches[i];
                    std::fwrite(matches[i],1,len,stdout);
                }
                printf("\n");
                delete[]matches;
            }
		}
		return 0;
	} catch (const char *err) {
		fprintf(stderr, "%s\n",err);
		return 1;
	}
}
