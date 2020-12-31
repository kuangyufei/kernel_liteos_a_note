

typedef unsigned int       UINTPTR;
typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long PTE_T;
typedef unsigned long VADDR_T;
#define IS_ALIGNED(a, b)                 (!(((UINTPTR)(a)) & (((UINTPTR)(b)) - 1)))
#define PAGE_SIZE                        (0x1000U) // 页大小4K
#define ROUNDUP(a, b)                    (((a) + ((b) - 1)) & ~((b) - 1))
#define ROUNDDOWN(a, b)                  ((a) & ~((b) - 1))
#define ROUNDOFFSET(a, b)                ((a) & ((b) - 1))
#define MMU_DESCRIPTOR_L1_SMALL_SIZE                            0x100000 //1M
#define MMU_DESCRIPTOR_L1_SMALL_MASK                            (MMU_DESCRIPTOR_L1_SMALL_SIZE - 1)
#define MMU_DESCRIPTOR_L1_SMALL_FRAME                           (~MMU_DESCRIPTOR_L1_SMALL_MASK)
#define MMU_DESCRIPTOR_L1_SMALL_SHIFT                           20
#define MMU_DESCRIPTOR_L1_SECTION_ADDR(x)                       ((x) & MMU_DESCRIPTOR_L1_SMALL_FRAME)
#define OS_TSK_HIGH_BITS       3U
#define OS_TSK_LOW_BITS        (32U - OS_TSK_HIGH_BITS) //29
#define OS_TSK_SORTLINK_LOGLEN OS_TSK_HIGH_BITS	//3U
#define BITMAP_BITS_PER_WORD (sizeof(UINTPTR) * 8)
#define BITMAP_NUM_WORDS(x) (((x) + BITMAP_BITS_PER_WORD - 1) / BITMAP_BITS_PER_WORD)
#ifndef LITE_OS_SEC_TEXT
#define LITE_OS_SEC_TEXT        /* __attribute__((section(".text.sram"))) */
#endif
typedef struct LOS_DL_LIST {
    struct LOS_DL_LIST *pstPrev; /**< Current node's pointer to the previous node */
    struct LOS_DL_LIST *pstNext; /**< Current node's pointer to the next node */
} LOS_DL_LIST;