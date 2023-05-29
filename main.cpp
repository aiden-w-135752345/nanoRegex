/*
 * Regular expression implementation.
 * Compiles to NFA using Thompson's algorithm and then simulates NFA
 * See also http://swtch.com/~rsc/regexp/ and https://doi.org/10.1145/363347.363387
 */
#include "match.hpp"
int main(int argc, char **argv) {
	if(argc < 3){
		fprintf(stderr, "usage: %s regexp string...\n",argc>0?argv[0]:"progname");
		return 1;
	}
	try {
		RegExp re = RegExp(argv[1]);
		fprintf(stderr, "Regex %s (%d captures)\n",argv[1],re.numCaptures);
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
/*
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */