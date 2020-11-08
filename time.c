#include "types.h"
#include "user.h"
#include "fs.h"


int main(int argc, char **argv)
{
	int wtime, rtime, status = 0;
	int pid = fork();
	if (pid < 0)
	{
		printf(1,"Forking error\n");
		exit();
	}
    else if (!pid)
	{
		if (argc == 1)
		{
			printf(2, "Insufficient Arguments \n");
			exit();
		}
        else
		{
			printf(1, "Timing %s\n", argv[1]);
			if (exec(argv[1], argv + 1) < 0)
			{
				printf(2, "exec %s failed\n", argv[1]);
				exit();
			}
		}
	}
    else if (pid > 0)
	{
		status = waitx(&wtime, &rtime);
		if (argc == 1)
		{
			printf(1, "Error Insufficient Arguments\n");
		}
        else
			printf(1, "Time taken by %s\nWait time: %d\nRun time: %d with Status %d\n\n", argv[1], wtime, rtime, status);
		exit();
	}
}