#include "types.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "user.h"
#include "fcntl.h"


int main(int argc, char **argv)
{
    if(argc<3)
    {
        printf(2,"Error insufficient arguments\n");
        //printf(2,"%s \n",argv[0]);
        exit();
    }
    //printf(2,"%d %d \n",atoi(argv[1]),atoi(argv[2]));
    if(set_priority(atoi(argv[1]),atoi(argv[2])) == -1)
    {
        printf(2,"Error: Priority not changed\n");
    }
    exit();
}
