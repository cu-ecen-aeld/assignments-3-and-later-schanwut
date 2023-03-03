#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

int main(int argc, char **argv)
{
	int status = -1;
	FILE *fp;
	char *writefile, *writestr, *dirc;

	openlog(NULL, 0, LOG_USER);

	if (argc != 3)
	{
		syslog(LOG_ERR, "Invalid arguments");	
		return 1;
	}

	writefile = argv[1];
	writestr = argv[2];
	
	dirc = strdup(writefile);

	status = mkdir(dirname(dirc), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	if (status != 0)
	{
		syslog(LOG_ERR, "Error creating directory %s: %s", dirname(dirc), strerror(errno));
	}

	fp = fopen (writefile, "a");

	if (fp == NULL)
	{
		syslog(LOG_ERR, "Error opening file %s: %s", writefile, strerror(errno));
		return 1;
	}

	fputs (writestr, fp);
	syslog(LOG_DEBUG, "Writing %s to %s" , writefile, writestr);

	fclose (fp);

	return 0;
}
