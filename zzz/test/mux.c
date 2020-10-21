#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
 
// 打印机
void printer(char *str)
{
	while(*str!='\0')
	{
		putchar(*str);	
		fflush(stdout);
		str++;
		sleep(1);
	}
	printf("\n"); 
}
 
// 线程一
void *thread_fun_1(void *arg)
{
	char *str = "hello";
	printer(str); //打印
}
 
// 线程二
void *thread_fun_2(void *arg)
{
	char *str = "world";
	printer(str); //打印
}
 
int main(void)
{
	pthread_t tid1, tid2;
	
	// 创建 2 个线程
	pthread_create(&tid1, NULL, thread_fun_1, NULL);
	pthread_create(&tid2, NULL, thread_fun_2, NULL);
 
	// 等待线程结束，回收其资源
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL); 
	
	return 0;
}