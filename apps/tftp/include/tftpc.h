/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TFTPC_H
#define TFTPC_H

#include "types_adapt.h"

#if LWIP_TFTP /* don't build if not configured for use in lwipopts.h */

#if defined (__cplusplus) && __cplusplus
extern "C" {
#endif
/**
 * @file tftpc.h
 * @brief TFTP客户端核心头文件
 * @details 定义TFTP协议相关的宏、枚举类型、结构体和接口函数声明，实现TFTP客户端的文件传输功能
 *          支持ASCII和二进制两种传输模式，包含完整的错误处理机制
 */

/**
 * @defgroup TFTP_COMMON 公共宏定义
 * @{ 
 */

/**
 * @brief 32位无符号整数无效值
 * @note 用于表示无效的32位无符号整数，值为0xFFFFFFFFUL
 */
#define TFTP_NULL_UINT32        ((u32_t)0xffffffffUL)

/**
 * @brief 32位整数无效值
 * @note 用于表示无效的32位整数，值为-1
 */
#define TFTP_NULL_INT32         (-1)

/** @} */ // end of TFTP_COMMON

/**
 * @defgroup TFTP_ENUM 枚举类型定义
 * @{ 
 */

/** @cond liteos
 * @defgroup TFTP_Interfaces
 * @ingroup Enums
 * 本章节包含TFTP相关枚举类型
 */
/**
 * @brief TFTP传输模式枚举
 * @details 指定TFTP客户端处理文件的传输模式
 */
typedef enum tagTFTPC_TransferMode {
    TRANSFER_MODE_ASCII = 0,  /**< ASCII传输模式，适用于文本文件 */
    TRANSFER_MODE_BINARY,     /**< 二进制传输模式，适用于二进制文件（如可执行文件、图片等） */
    TRANSFER_MODE_BUTT        /**< 无效传输模式，作为枚举边界值 */
} TFTPC_TRANSFER_MODE_E;

/**
 * @brief TFTP错误码枚举
 * @details 定义TFTP客户端操作中可能出现的错误类型
 */
typedef enum tagTFTPC_ErrCode {
    TFTPC_SOCKET_FAILURE = 1,         /**< 创建UDP套接字失败 */
    TFTPC_BIND_FAILURE = 2,           /**< 绑定UDP套接字失败 */
    TFTPC_SELECT_ERROR = 3,           /**< select()系统调用错误 */
    TFTPC_RECVFROM_ERROR = 4,         /**< 从对等方接收数据失败 */
    TFTPC_SENDTO_ERROR = 5,           /**< 向对等方发送数据失败 */
    TFTPC_FILE_NOT_FOUND = 6,         /**< 请求的文件未找到 */
    TFTPC_CANNOT_RESOLVE_HOSTNAME = 7,/**< 服务器主机名无法解析 */
    TFTPC_INVALID_PARAVALUE = 8,      /**< 传递给TFTP接口的输入参数无效 */
    TFTPC_PROTO_ERROR = 9,            /**< TFTP数据包中检测到错误或从服务器收到错误 */
    TFTPC_SYNC_FAILURE = 10,          /**< 发送时数据包同步错误或收到意外数据包 */
    TFTPC_FILE_TOO_BIG = 11,          /**< 文件大小超出限制（最大块数0xFFFF，每块512字节） */
    TFTPC_SRC_FILENAME_LENGTH_ERROR = 12, /**< 源文件名长度超过256字节 */
    TFTPC_IP_NOT_WITHIN_RANGE = 13,   /**< 主机IP地址无效 */
    TFTPC_ACCESS_ERROR = 14,          /**< TFTP服务器返回文件访问错误 */
    TFTPC_DISK_FULL = 15,             /**< TFTP服务器返回磁盘已满错误 */
    TFTPC_FILE_EXISTS = 16,           /**< TFTP服务器返回文件已存在错误 */
    TFTPC_FILE_NOT_EXIST = 17,        /**< tftp_put_file_by_filename返回源文件不存在错误 */
    TFTPC_MEMALLOC_ERROR = 18,        /**< TFTP客户端内存分配失败 */
    TFTPC_FILEOPEN_ERROR = 19,        /**< 文件打开失败 */
    TFTPC_FILEREAD_ERROR = 20,        /**< 文件读取错误 */
    TFTPC_FILECREATE_ERROR = 21,      /**< 文件创建错误 */
    TFTPC_FILEWRITE_ERROR = 22,       /**< 文件写入错误 */
    TFTPC_TIMEOUT_ERROR = 23,         /**< 等待文件接收超时（超过最大尝试次数） */
    TFTPC_PKT_SIZE_ERROR = 24,        /**< 接收的数据包大小无效（小于4字节或大于512字节） */
    TFTPC_ERROR_NOT_DEFINED = 25,     /**< TFTP服务器返回未定义的协议错误 */
    TFTPC_DEST_PATH_LENGTH_ERROR = 26,/**< 目标文件路径长度超过256字节 */
    TFTPC_UNKNOWN_TRANSFER_ID = 27,   /**< TFTP服务器返回未知的传输ID错误 */
    TFTPC_IOCTLSOCKET_FAILURE = 28,   /**< TFTP客户端设置套接字为非阻塞模式时IOCTL函数失败 */
    TFTPC_MEMCPY_FAILURE = 29         /**< TFTP内存复制操作失败 */
} TFTPC_ERR_CODE_E;

/**
 * @brief TFTP操作码枚举
 * @details 定义TFTP协议中的各种操作类型
 */
typedef enum tagTFTPC_OpCode {
    TFTPC_OP_RRQ = 1,         /* 读请求（Read Request），客户端请求从服务器读取文件 */
    TFTPC_OP_WRQ,             /* 写请求（Write Request），客户端请求向服务器写入文件 */
    TFTPC_OP_DATA,            /* 数据 packet（Data Packet），用于传输文件数据 */
    TFTPC_OP_ACK,             /* 确认 packet（Acknowledgment），用于确认接收到数据 */
    TFTPC_OP_ERROR,           /* 错误 packet（Error Packet），用于传输错误信息 */
    TFTPC_OP_OPT              /* 选项 packet（Option Packet），用于协商传输选项 */
} TFTPC_OPCODE_E;

/**
 * @brief TFTP协议错误码枚举
 * @details 定义TFTP协议规范中定义的错误类型，用于错误 packet 中
 */
typedef enum tagTFTPC_PROTOCOL_ErrCode {
    TFTPC_PROTOCOL_USER_DEFINED = 0,              /**< 用户自定义错误 */
    TFTPC_PROTOCOL_FILE_NOT_FOUND,                /**< 文件未找到 */
    TFTPC_PROTOCOL_ACCESS_ERROR,                  /**< 访问权限错误 */
    TFTPC_PROTOCOL_DISK_FULL,                     /**< 磁盘空间不足 */
    TFTPC_PROTOCOL_PROTO_ERROR,                   /**< 协议错误 */
    TFTPC_PROTOCOL_UNKNOWN_TRANSFER_ID,           /**< 未知的传输ID */
    TFTPC_PROTOCOL_FILE_EXISTS,                   /**< 文件已存在 */
    TFTPC_PROTOCOL_CANNOT_RESOLVE_HOSTNAME        /**< 无法解析主机名 */
} TFTPC_PROT_ERRCODE_E;

/** @} */ // end of TFTP_ENUM

/**
 * @defgroup TFTP_CONFIG 配置宏定义
 * @{ 
 */

/**
 * @brief TFTP最大发送请求尝试次数
 * @note 如果未定义，则默认值为5次
 */
#ifndef TFTPC_MAX_SEND_REQ_ATTEMPTS
#define TFTPC_MAX_SEND_REQ_ATTEMPTS         5 /* tftp最大尝试次数 */
#endif

/**
 * @brief TFTP超时时间（秒）
 * @note 如果未定义，则默认值为5秒
 */
#ifndef TFTPC_TIMEOUT_PERIOD
#define TFTPC_TIMEOUT_PERIOD         5 /* tftp超时时间，单位：秒 */
#endif

/**
 * @brief TFTP服务器知名端口号
 * @note 标准TFTP服务器端口号为69
 */
#define TFTPC_SERVER_PORT          69 /* tftp服务器知名端口号 */

/**
 * @brief TFTP最大循环等待次数
 * @details TFTP中最大文件大小为32MB，此处设置75的原因是：
 *          (75 * 512字节 = 38400字节) / 1024 = 37.5MB，确保接收/发送循环
 *          能够接收网络中的完整最大消息
 */
#define TFTPC_MAX_WAIT_IN_LOOP         75

/**
 * @brief TFTP数据块大小
 * @note 遵循IEN-133标准，数据块大小为512字节
 */
#define TFTP_BLKSIZE               512     /* 数据块大小（IEN-133） */

/**
 * @brief TFTP头部大小
 * @note TFTP头部包含2字节操作码和2字节块编号，共4字节
 */
#define TFTP_HDRSIZE               4       /* TFTP头部大小 */

/**
 * @brief TFTP数据包大小
 * @note 数据包大小 = 数据块大小 + 头部大小 = 512 + 4 = 516字节
 */
#define TFTP_PKTSIZE              (TFTP_BLKSIZE + TFTP_HDRSIZE) /* 数据包大小 */

/**
 * @brief 传输模式字符串最大长度
 * @note 包括ASCII(5字节)和octet(6字节)等模式，最大长度为9字节
 */
#define TFTP_MAX_MODE_SIZE         9  /* 模式字符串最大长度 */

/**
 * @brief 错误消息字符串最大长度
 * @note 错误消息最大长度为100字节
 */
#define TFTP_MAXERRSTRSIZE         100 /* 错误消息字符串最大长度 */

/**
 * @brief 最大路径或文件名长度
 * @note 路径或文件名最大长度为256字节
 */
#define TFTP_MAX_PATH_LENGTH       256 /* 最大路径或文件名长度 */

/**
 * @brief 最大块编号
 * @note 块编号为16位无符号整数，最大值为0xFFFF（十进制65535）
 */
#define TFTP_MAX_BLK_NUM          (0xFFFFL) /* 最大块编号 */

/**
 * @brief IP地址范围定义
 * @details 排除保留IP（0和127段）和多播地址（D类）
 *          - TFTPC_IP_ADDR_MIN: 最小有效IP地址（1.0.0.0）
 *          - TFTPC_IP_ADDR_EX_RESV: A类地址上限（126.255.255.255）
 *          - TFTPC_IP_ADDR_CLASS_B: B类地址下限（128.0.0.0）
 *          - TFTPC_IP_ADDR_EX_CLASS_DE: C类地址上限（223.255.255.255）
 */
#define TFTPC_IP_ADDR_MIN         0x01000000      /* 1.0.0.0 */
#define TFTPC_IP_ADDR_EX_RESV     0x7effffff      /* 126.255.255.255 */
#define TFTPC_IP_ADDR_CLASS_B     0x80000000      /* 128.0.0.0 */
#define TFTPC_IP_ADDR_EX_CLASS_DE 0xdfffffff      /* 223.255.255.255 */

/**
 * @brief 最小数据包大小
 * @note TFTP最小数据包大小为4字节（头部大小）
 */
#define TFTPC_FOUR 4  /* 最小数据包大小 */

/** @} */ // end of TFTP_CONFIG

/**
 * @defgroup TFTP_STRUCT 结构体定义
 * @{ 
 */

/****************************************************************************/
/*                            结构体定义                                      */
/****************************************************************************/

/**
 * @brief TFTP数据 packet 结构体
 * @details 用于存储TFTP数据 packet 的内容
 */
typedef struct tagTFTPC_DATA {
    u16_t usBlknum;                      /* 块编号（网络字节序） */
    u8_t ucDataBuf[TFTP_BLKSIZE];       /* 实际数据缓冲区，大小为TFTP_BLKSIZE(512字节) */
} TFTPC_DATA_S;


/**
 * @brief TFTP错误 packet 结构体
 * @details 用于存储TFTP错误 packet 的内容
 */
typedef struct tagTFTPC_ERROR {
    u16_t usErrNum;                       /* 错误编号（网络字节序） */
    u8_t ucErrMesg[TFTP_MAXERRSTRSIZE]; /* 错误消息字符串，最大长度为TFTP_MAXERRSTRSIZE(100字节) */
} TFTPC_ERROR_S;


/**
 * @brief TFTP packet 格式结构体
 * @details 通用TFTP packet 结构体，根据操作码不同，union成员表示不同类型的 packet 内容
 */
typedef struct tagTFTPC_PACKET {
    u16_t usOpcode; /* 操作码（网络字节序），取值见TFTPC_OPCODE_E枚举 */
    union {
        /* 包含文件名和传输模式字符串，用于RRQ和WRQ packet */
        s8_t ucName_Mode[TFTP_MAX_PATH_LENGTH + TFTP_MAX_MODE_SIZE];
        u16_t usBlknum; /* 块编号，用于ACK packet */
        TFTPC_DATA_S stTFTP_Data; /* 数据 packet，用于DATA packet */
        TFTPC_ERROR_S stTFTP_Err; /* 错误 packet，用于ERROR packet */
    } u;
} TFTPC_PACKET_S;

/** @defgroup TFTP_Interfaces
* This section contains the TFTP Interfaces
*/
/*
Func Name:  lwip_tftp_get_file_by_filename
*/
/**
* @ingroup TFTP_Interfaces
* @brief
* This API gets the source file from the server. It then stores the received file in the destination path
* on the client system.
*
* @param[in]    ulHostAddr          IP address of Host. This is the TFTP server IP. [NA]
* @param[in]    usTftpServPort    TFTP server port. If the value is passed as 0 then the default TFTP
*                                                   PORT 69 is used. [NA]
* @param[in]    ucTftpTransMode File transfer mode, either TRANSFER_MODE_BINARY or TRANSFER_MODE_ASCII. [NA]
* @param[in]    szSrcFileName     Source file in the tftp server. [NA]
* @param[in]    szDestDirPath     Destination file path in the in the client. [NA]
* @param[out]  [N/A]
*
* @return
*  ERR_OK: On success \n
*  TFTPC_ERR_CODE_E: On failure
*
* @note
* \n
* The behavior of this API is such that if the destination file already exists, it will be overwritten.
*/
u32_t lwip_tftp_get_file_by_filename(u32_t ulHostAddr,
                                     u16_t usTftpServPort,
                                     u8_t ucTftpTransMode,
                                     s8_t *szSrcFileName,
                                     s8_t *szDestDirPath);


/* @defgroup TFTP_Interfaces
* This section contains the TFTP Interfaces
*/
/*
Func Name:  lwip_tftp_put_file_by_filename
*/
/**
* @ingroup TFTP_Interfaces

* @brief
* This API reads the contents of the source file on the client system and sends it to the server and
* server then receives the data and stores it in the specified destination path.
*
* @param[in]    ulHostAddr         Indicates the IP address of Host. This is the TFTP server IP.
* @param[in]    usTftpServPort    Indicates the TFTP server port. If the value is passed as 0 then the default TFTP
*                                                   PORT 69 is used.
* @param[in]    ucTftpTransMode  Indicates the file transfer mode, either TRANSFER_MODE_BINARY or TRANSFER_MODE_ASCII.
* @param[in]    szSrcFileName     Indicates the source file in the client.
* @param[in]    szDestDirPath     Indicates the destination file path on the tftp server.
*
* @return
*  ERR_OK: On success \n
*  TFTPC_ERR_CODE_E: On failure
*
*/
u32_t lwip_tftp_put_file_by_filename(u32_t ulHostAddr,
                                     u16_t usTftpServPort,
                                     u8_t cTftpTransMode,
                                     s8_t *szSrcFileName,
                                     s8_t *szDestDirPath);

#ifdef TFTP_TO_RAWMEM
/* @defgroup TFTP_Interfaces
* This section contains the TFTP Interfaces
*/
/*
Func Name:  lwip_tftp_get_file_by_filename_to_rawmem
*/
/**
* @ingroup TFTP_Interfaces

* @brief
* This API gets the source file from the server. It then stores the received file in the target memory
* on the client system.
*
* @param[in]    ulHostAddr         Indicates the IP address of the Host. This is the TFTP server IP.
* @param[in]    usTftpServPort     Indicates the TFTP server port. If the value is passed as 0 then the default TFTP
*                                                   PORT 69 is used.
* @param[in]    ucTftpTransMode  Indicates the File transfer mode, either TRANSFER_MODE_BINARY or TRANSFER_MODE_ASCII.
* @param[in]    szSrcFileName      Indicates the Source file in the TFTP server.
* @param[in]    szDestMemAddr      Indicates the target memory address in the client.
* @param[in/out]    ulFileLength       Indicates the target memory address can cache the size of the content,
                                       and The real size of the Source file.
*
* @return
*  ERR_OK: On success \n
*  TFTPC_ERR_CODE_E: On failure
* @note

* 1.You must define TFTP_TO_RAWMEM when using this API. \n
* 2.The behavior of this API is such that if the destination file already exists, it will be overwritten.
* @endcond
*/

u32_t lwip_tftp_get_file_by_filename_to_rawmem(u32_t ulHostAddr,
                                               u16_t usTftpServPort,
                                               u8_t ucTftpTransMode,
                                               s8_t *szSrcFileName,
                                               s8_t *szDestMemAddr,
                                               u32_t *ulFileLength);
#endif

#if defined (__cplusplus) && __cplusplus
}
#endif

#endif /* LWIP_TFTP */

#endif /* TFTPC_H */
