#include <stdio.h>
#include <string.h>
#include "primitives.h"

void main(int argc, char *argv[])
{
	while(*++argv) {
		int id;
		if(strlen(*argv) != 4) {
			printf("- %s [must be exactly four characters]\n",*argv);
			continue;
		}
		id = CHAR4(argv[0][0],argv[0][1],argv[0][2],argv[0][3]);
		printf("%d\n",id);
	}
}
