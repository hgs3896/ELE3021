#include "types.h"
#include "stat.h"
#include "user.h"

#define TESTCASE(N, cond) \
    printf(1, "Testcase #" #N ": "); \
    printf(1, (cond) ? "correct\n" : "fail\n");

int
main(int argc, char *argv[])
{
    TESTCASE(1, set_cpu_share(100) == -1);
    TESTCASE(2, set_cpu_share(50) == 0);
    TESTCASE(3, set_cpu_share(0) == -1);
    TESTCASE(4, set_cpu_share(-10) == -1);
    exit();
}