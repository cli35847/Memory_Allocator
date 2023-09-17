#include <stdlib.h>
#include <string.h>
#include "hw4_mm_test.h"
#define LOOP_NUM 10

int main(int argc, char *argv[]) 
{
char buf[1024];
char cmd[64];
char value[64];

	while ( !feof(stdin) )
	{
		if ( fgets(buf, sizeof(buf), stdin) == NULL)
			break;

		if (sscanf(buf, "%s %s\n", cmd, value ) != 2)
		{
			if (buf[0] != '\n')
				printf("command error\n");
			continue;
		}
		if (strcmp(cmd, "alloc") == 0)
		{
		int v = strtol(value,NULL,0);
		void *ptr;

			ptr = hw_malloc(v);
			printf("0x%08x\n",(unsigned int) (ptr - hw_get_start_brk()));
		}
		else
		if (strcmp(cmd, "free") == 0)
		{
		unsigned int v = strtoul(value, NULL,0);
		void *ptr = hw_get_start_brk() + v;
			hw_free(ptr);
		}
		else
		if (strcmp(cmd, "print") == 0)
		{
		int num;
			if (sscanf(value, "bin[%d]", &num) != 1)
			{
				printf("command error!\n");
				continue;
			}
			print_bin(num);	
		}
	}
    return 0;
}
