#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int index = 0;
	FILE *fd = fopen("/dev/shm/smv92.mem","r+b");
	setvbuf(fd, NULL, _IONBF, 0);
	fseek(fd, 4,0);
	fread((char *)&index,4,1,fd);
	printf("max: %i\n", index);

	while(1)
	{
		fseek(fd, 8,0);
		fread((char *)&index,4,1,fd);
		printf("index: %i\n", index);
		usleep(1);
	}
	return 0;
}

