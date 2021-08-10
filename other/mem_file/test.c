#include <stdio.h>
#include <unistd.h>
#include <time.h>

long current_time()
{
    struct timespec x;
    clock_gettime(CLOCK_MONOTONIC, &x);
    return (long) (x.tv_sec * 1000000 + x.tv_nsec/1000);
}

int main(void)
{
	long start = current_time();
	int index = 0;
	FILE *fd = fopen("/dev/shm/smv92.mem","r+b");
	setvbuf(fd, NULL, _IONBF, 0);
	fseek(fd, 4,0);
	fread((char *)&index,4,1,fd);
	printf("max: %i\n", index);
	int i = 0;
	while(i++ < 100000)
	{
		fseek(fd, 8,0);
		fread((char *)&index,4,1,fd);
		//printf("index: %i, i=%i\n", index,i);
		usleep(1);
	}
        long stop = current_time();
        printf("time: %li\n", stop-start);
	return 0;
}

