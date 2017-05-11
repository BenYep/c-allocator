#include "fcyc.h"
#include "allocator.h"
#include "segment.h"
#include "stdlib.h"
#include "stdio.h"

int main(){
	myinit();
	char *a=mymalloc(2040);
	char *b=mymalloc(2040);
	myfree(b);
	char *c = mymalloc(48);
	char *d=mymalloc(4072);
	myfree(d);
	char *e=mymalloc(4072);
	myfree(a);
	myfree(c);
	char *f=mymalloc(4072);
	myfree(e);
	myfree(f);
	return 0;
}
