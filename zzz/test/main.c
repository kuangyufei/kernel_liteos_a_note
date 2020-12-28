#include <stdio.h>
#include <stdarg.h>

typedef unsigned int       UINTPTR;
typedef unsigned char      UINT8;
typedef unsigned long PTE_T;
typedef unsigned long VADDR_T;
#define IS_ALIGNED(a, b)                 (!(((UINTPTR)(a)) & (((UINTPTR)(b)) - 1)))

typedef struct LOS_DL_LIST {
    struct LOS_DL_LIST *pstPrev; /**< Current node's pointer to the previous node */
    struct LOS_DL_LIST *pstNext; /**< Current node's pointer to the next node */
} LOS_DL_LIST;

void b(){
    UINT8 w[3]={0};
    PTE_T pte1BasePtr = 0x11100000;
    VADDR_T vaddr = 0x80738903;  

    #define MMU_DESCRIPTOR_L1_SMALL_SIZE                            0x100000 //1M
    #define MMU_DESCRIPTOR_L1_SMALL_MASK                            (MMU_DESCRIPTOR_L1_SMALL_SIZE - 1)
    #define MMU_DESCRIPTOR_L1_SMALL_FRAME                           (~MMU_DESCRIPTOR_L1_SMALL_MASK)
    #define MMU_DESCRIPTOR_L1_SMALL_SHIFT                           20
    #define MMU_DESCRIPTOR_L1_SECTION_ADDR(x)                       ((x) & MMU_DESCRIPTOR_L1_SMALL_FRAME)
    #define OS_TSK_HIGH_BITS       3U
    #define OS_TSK_LOW_BITS        (32U - OS_TSK_HIGH_BITS) //29
    #define OS_TSK_SORTLINK_LOGLEN OS_TSK_HIGH_BITS	//3U

    PTE_T  l1Entry = pte1BasePtr + vaddr >> MMU_DESCRIPTOR_L1_SMALL_SHIFT;
    printf("pte1BasePtr ad: %x\n",&pte1BasePtr);
    printf("w[1] ad: %x\n",&w[1]);
    printf("vaddr: %x\n",(MMU_DESCRIPTOR_L1_SMALL_SIZE >> 12));
    printf("l1Entry: %x\n",l1Entry);
    printf("MMU_DESCRIPTOR_L1_SMALL_MASK:%x\n",MMU_DESCRIPTOR_L1_SMALL_MASK);
    printf("MMU_DESCRIPTOR_L1_SMALL_FRAME:%x\n",MMU_DESCRIPTOR_L1_SMALL_FRAME);
    printf("pa: %x\n",MMU_DESCRIPTOR_L1_SECTION_ADDR(l1Entry) + (vaddr & (MMU_DESCRIPTOR_L1_SMALL_SIZE - 1)));
}

void a(){
    #define BITMAP_BITS_PER_WORD (sizeof(UINTPTR) * 8)
    #define BITMAP_NUM_WORDS(x) (((x) + BITMAP_BITS_PER_WORD - 1) / BITMAP_BITS_PER_WORD)
    printf("BITMAP_BITS_PER_WORD %d\n",BITMAP_BITS_PER_WORD);
    printf("BITMAP_NUM_WORDS %d\n",BITMAP_NUM_WORDS(1UL << 8));
}

void round1(){
    #define PAGE_SIZE                        (0x1000U) // 页大小4K
    #define ROUNDUP(a, b)                    (((a) + ((b) - 1)) & ~((b) - 1))
    #define ROUNDDOWN(a, b)                  ((a) & ~((b) - 1))
    #define ROUNDOFFSET(a, b)                ((a) & ((b) - 1))
    int a = 0xFF;
    //printf("a>> %d\n",a>>3);
    //printf("a/ %d\n",a/8);
    printf("ROUNDUP %d,%d,%d\n", ROUNDUP(7,4) ,ROUNDUP(8,4) ,ROUNDUP(9,4));
    printf("ROUNDDOWN %d,%d,%d\n", ROUNDDOWN(7,4),ROUNDDOWN(8,4),ROUNDDOWN(9,4));
    printf("ROUNDOFFSET %d,%d,%d\n", ROUNDOFFSET(7,4),ROUNDOFFSET(8,4),ROUNDOFFSET(9,4));
}
void arg_test(int i, ...)
{
    int j=0,k=0;
    va_list arg_ptr;
    va_start(arg_ptr, i);
    printf("&i = %p\n", &i);//打印参数i在堆栈中的地址
    printf("arg_ptr = %p\n", arg_ptr);//打印va_start之后arg_ptr地址
    printf("%d %d %d\n", i, j, k);
    j=va_arg(arg_ptr, int);
    printf("arg_ptr = %p\n", arg_ptr);//打印va_arg后arg_ptr的地址
    printf("%d %d %d\n", i, j, k);
    printf("arg_ptr = %p\n", arg_ptr);//打印va_arg后arg_ptr的地址
    k=va_arg(arg_ptr, int);
    printf("%d %d %d\n", i, j, k);
    printf("arg_ptr = %p\n", arg_ptr);//打印va_arg后arg_ptr的地址
    /*这时arg_ptr应该比参数i的地址高sizeof(int)个字节,即指向下一个参数的地址,如果已经是最后一个参数，arg_ptr会为NULL*/
    va_end(arg_ptr);
    printf("arg_ptr = %p\n", arg_ptr);//打印va_arg后arg_ptr的地址
    printf("%d %d\n", i, j);
}
int main()
{
    int size = 0;
    printf("sizeof(int) %d\n",sizeof(int));
    round1();
    //arg_test(99, 4,8,9);
    //size = sizeof(LOS_DL_LIST) << OS_TSK_SORTLINK_LOGLEN;
    //printf("LOS_DL_LIST %d\n",sizeof(LOS_DL_LIST *));
    //printf("size %d\n",size);
    return 0;
}

