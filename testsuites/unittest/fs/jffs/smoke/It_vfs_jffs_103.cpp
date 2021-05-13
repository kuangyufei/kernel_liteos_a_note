#include "It_vfs_jffs.h"

static UINT32 testcase(VOID)
{
/*    INT32  ret,fd,len;
    off_t off;
    CHAR pathname1[50] = JFFS_PATH_NAME0;
    CHAR pathname2[50]=JFFS_PATH_NAME0;
    struct stat64 buf1={0};
    struct stat64 buf2={0};
    struct stat buf3={0};
    CHAR readbuf[20]={0};

    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat(pathname1, "/dir");
    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    strcat(pathname2, "/dir/files");
    fd = open64(pathname2, O_NONBLOCK | O_CREAT | O_RDWR, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT3);

    len = write(fd, "ABCDEFGHJK", 10);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT3);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, JFFS_IS_ERROR, off, EXIT3);

    len = read(fd,readbuf,10);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "ABCDEFGHJK", readbuf, EXIT3);

    ret = stat64(pathname2, &buf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    jffs_stat64_printf(buf1);

    ret = fstat64(fd, &buf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    jffs_stat64_printf(buf2);

    ret = stat(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    jffs_stat_printf(buf3);

    ICUNIT_GOTO_EQUAL(buf1.st_size,  10, buf1.st_size, EXIT3);
    ICUNIT_GOTO_EQUAL(buf2.st_size,  10, buf2.st_size, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.st_size,  0, buf3.st_size, EXIT3);

    ICUNIT_GOTO_EQUAL(buf1.st_mode& S_IFMT,  S_IFREG, buf1.st_mode& S_IFMT, EXIT3);
    ICUNIT_GOTO_EQUAL(buf2.st_mode& S_IFMT,  S_IFREG, buf2.st_mode& S_IFMT, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.st_mode& S_IFMT,  S_IFDIR, buf3.st_mode& S_IFMT, EXIT3);

    ICUNIT_GOTO_EQUAL(buf1.st_blocks, buf2.st_blocks, buf1.st_blocks, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_size, buf2.st_size, buf1.st_size, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_blksize, buf2.st_blksize, buf1.st_blksize, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_ino, buf2.st_ino, buf1.st_ino, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_mode& S_IFMT, buf2.st_mode& S_IFMT, buf1.st_mode& S_IFMT, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_mtim.tv_nsec, buf2.st_mtim.tv_nsec, buf1.st_mtim.tv_nsec, EXIT3);

    ret = close(fd) ;
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = unlink(pathname2) ;
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT3:
    close(fd);
EXIT2:
    jffs_strcat2(pathname1, "/dir/files",strlen(pathname1));
    remove(pathname1);
EXIT1:
    jffs_strcat2(pathname1, "/dir",strlen(pathname1));
    remove(pathname1);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;*/
}

/*
**********
testcase brief in English
**********
*/

VOID IT_FS_JFFS_103(VOID)
{

    TEST_ADD_CASE("IT_FS_JFFS_103", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);

}

