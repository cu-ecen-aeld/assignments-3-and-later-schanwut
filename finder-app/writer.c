#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char **argv)
{
	openlog(NULL, 0, LOG_USER);

	if (argc < 3) {
		syslog(LOG_ERR, "Invalid arguments");	
		exit(1);
	}

	const char *writefile = argv[1];
	const char *writestr = argv[2];

	FILE *fp = fopen (writefile, "w+");

	if (fp == NULL) {
		syslog(LOG_ERR, "Error opening file %s: %s", writefile, strerror(errno));
		exit(1);
	}

	syslog(LOG_DEBUG, "Writing %s to %s" , writefile, writestr);
	fprintf (fp, "%s", writestr);
	fclose (fp);

	return 0;
}
