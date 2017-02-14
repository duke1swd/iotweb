#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern char *myname;

char *
ds_copy(char *s)
{
	int n;
	char *p;

	n = strlen(s) + 1;
	p = malloc((unsigned) n);
	if (!p) {
		fprintf(stderr, "%s: malloc of %d bytes failed.\n",
			myname, n);
		exit(1);
	}

	strcpy(p, s);
	
	return p;
}
