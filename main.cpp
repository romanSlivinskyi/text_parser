#include "parser.h"

int main()
{
    parser p;
    string src, dest;
    std::cout << "input file's path: ";
    std::cin >> src;
    std::cout << "input result's path: ";
    std::cin >> dest;
    p.word_count(src, dest);
    std::cout << "done.\n";
	return 0;
}







