#ifndef MSH_H
#define MSH_H

#define MAGENTA "\x1b[32m"
#define RESET   "\x1b[0m"

#include <stdio_ext.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct _pdetail
{
	int pno;
	pid_t pid;
	char pname[20];
	char state[20];
	char prior;

	struct _pdetail *link, *prev;

} pdt_t;

#endif
