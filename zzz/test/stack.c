//armv7-a clang (trunk)
#include <stdio.h>
int fp(int b)
{
    int a = 1;
    return a+b;
}

int main()
{
    int sum = 0;
    for(int a = 0;a < 100; a++){
        sum = sum + fp(a);
    }
    return sum;
}