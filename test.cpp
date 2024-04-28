#include "nanoRegex.hpp"
#include "cx.hpp"
#include <cstdio>
#include <cstring>
NanoRegex test2="(fo+)ba[rz]"_regex;
int main(int, char **) {
    const char* teststr="foooobaz";
    try{
        NanoRegex test1="(fo*)ba[rz]";
        printf("test1 (%d captures): ",test1.captures());
        const char** matches=test1.match(teststr,teststr+strlen(teststr));
        if(matches!=nullptr){
            printf("%s matches: ",teststr);
            for(int i=0;i<test1.captures();i+=2){
                putchar(' ');
                size_t len=matches[i+1]-matches[i];
                fwrite(matches[i],1,len,stdout);
            }
            printf("\n");
            delete[]matches;
        }else{printf("%s does not match\n",teststr);}
	} catch (const char *err) {
		fprintf(stderr, "%s\n",err);
		return 1;
	}
    try{
        printf("test2 (%d captures): ",test2.captures());
        const char** matches=test2.match(teststr,teststr+strlen(teststr));
        if(matches!=nullptr){
            printf("%s matches: ",teststr);
            for(int i=0;i<test2.captures();i+=2){
                putchar(' ');
                size_t len=matches[i+1]-matches[i];
                fwrite(matches[i],1,len,stdout);
            }
            printf("\n");
            delete[]matches;
        }else{printf("%s does not match\n",teststr);}
	} catch (const char *err) {
		fprintf(stderr, "%s\n",err);
		return 1;
	}
}
