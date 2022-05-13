#include "types.h"
#include "stat.h"
#include "user.h"

int g = 10;
int
main(int argc, char *argv[])
{
    int x = 10;
    int *y = malloc(sizeof(int));
    *y = 10;
    printf(1, "x = %d, *y = %d, g = %d\n", x, *y, g);
    printf(1, "&x = %x, y = %x, &g = %x\n", &x, y, &g);
    exit();
}
// 4096(10) = 1000
// static   =  810  0번째 페이지
// stack    = 2FCC  2번째 페이지
// heap     = AFF8 10번째 페이지