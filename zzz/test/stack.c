//armv7-a clang (trunk)
#include <stdio.h>
#include <math.h>

int square(int a,int b){
    return a*b;
}

int fp(int b)
{
    int a = 1;
    return square(a+b,a+b);
}

int main()
{
    int sum = 1;
    for(int a = 0;a < 100; a++){
        sum = sum + fp(a);
    }
    return sum;
}