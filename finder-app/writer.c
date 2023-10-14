#include <syslog.h>
#include <stddef.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	openlog("writer",0,LOG_USER);
	
	if (argc != 3)
	{
		printf("Please specify a write file as arg 1 and a write string as arg 2\r\n");
		syslog(LOG_ERR,"Invalid number of arguments: %d",argc);
		return 1;	
	}
	else
	{
		FILE *fp;
		char* writefile = argv[1];
		char* writestr = argv[2];
		
		syslog(LOG_DEBUG,"Writing %s to %s",writestr, writefile);
		
		fp = fopen(writefile, "w");
		fputs(writestr, fp);
		fclose(fp);
	}
	return 0;

}
