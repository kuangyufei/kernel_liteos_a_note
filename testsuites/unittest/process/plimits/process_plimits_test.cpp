/*

 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "It_process_plimits.h"

using namespace std;
using namespace testing::ext;
namespace OHOS {
class ProcessPlimitsTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}

protected:
    virtual void SetUp();
    virtual void TearDown();

private:
    inline bool IsFile(const std::string &file);
    inline bool IsDir(const std::string &path);
    inline bool IsSpecialDir(const std::string &path);
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/**
* @tc.name: plimits_Test_001
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits001, TestSize.Level0)
{
    ItProcessPlimits001();
}

/**
* @tc.name: plimits_Test_002
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits002, TestSize.Level0)
{
    ItProcessPlimits002();
}

/**
* @tc.name: plimits_Test_003
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits003, TestSize.Level0)
{
    ItProcessPlimits003();
}

/**
* @tc.name: plimits_Test_004
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits004, TestSize.Level0)
{
    ItProcessPlimits004();
}

/**
* @tc.name: plimits_Test_005
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits005, TestSize.Level0)
{
    ItProcessPlimits005();
}

/**
* @tc.name: plimits_Test_006
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits006, TestSize.Level0)
{
    ItProcessPlimits006();
}

/**
* @tc.name: plimits_Test_007
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits007, TestSize.Level0)
{
    ItProcessPlimits007();
}

/**
* @tc.name: plimits_Test_008
* @tc.desc: plimits function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimits008, TestSize.Level0)
{
    ItProcessPlimits008();
}

/**
* @tc.name: plimits_pid_Test_001
* @tc.desc: pid plimit function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsPid001, TestSize.Level0)
{
    ItProcessPlimitsPid001();
}

/**
* @tc.name: plimits_pid_Test_002
* @tc.desc: pid plimit function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsPid002, TestSize.Level0)
{
    ItProcessPlimitsPid002();
}

/**
* @tc.name: plimits_pid_Test_003
* @tc.desc: pid plimit function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsPid003, TestSize.Level0)
{
    ItProcessPlimitsPid003();
}

/**
* @tc.name: plimits_pid_Test_004
* @tc.desc: pid plimit function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsPid004, TestSize.Level0)
{
    ItProcessPlimitsPid004();
}

/**
* @tc.name: plimits_pid_Test_005
* @tc.desc: pid plimit function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsPid005, TestSize.Level0)
{
    ItProcessPlimitsPid005();
}

/**
* @tc.name: plimits_pid_Test_006
* @tc.desc: pid plimit function test case
* @tc.type: FUNC
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsPid006, TestSize.Level0)
{
    ItProcessPlimitsPid006();
}

/**
* @tc.name: plimits_Mem_Test_001
* @tc.desc: mem plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsMemory001, TestSize.Level0)
{
    ItProcessPlimitsMemory001();
}

/**
* @tc.name: plimits_Mem_Test_002
* @tc.desc: mem plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsMemory002, TestSize.Level0)
{
    ItProcessPlimitsMemory002();
}

/**
* @tc.name: plimits_Dev_Test_001
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices001, TestSize.Level0)
{
    ItProcessPlimitsDevices001();
}

/**
* @tc.name: plimits_Dev_Test_002
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices002, TestSize.Level0)
{
    ItProcessPlimitsDevices002();
}

/**
* @tc.name: plimits_Dev_Test_003
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices003, TestSize.Level0)
{
    ItProcessPlimitsDevices003();
}

/**
* @tc.name: plimits_Dev_Test_004
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices004, TestSize.Level0)
{
    ItProcessPlimitsDevices004();
}

/**
* @tc.name: plimits_Dev_Test_005
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices005, TestSize.Level0)
{
    ItProcessPlimitsDevices005();
}

/**
* @tc.name: plimits_Dev_Test_006
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices006, TestSize.Level0)
{
    ItProcessPlimitsDevices006();
}

/**
* @tc.name: plimits_Dev_Test_007
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices007, TestSize.Level0)
{
    ItProcessPlimitsDevices007();
}

/**
* @tc.name: plimits_Dev_Test_008
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices008, TestSize.Level0)
{
    ItProcessPlimitsDevices008();
}

/**
* @tc.name: plimits_Dev_Test_009
* @tc.desc: devices plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsDevices009, TestSize.Level0)
{
    ItProcessPlimitsDevices009();
}

/**
* @tc.name: plimits_Ipc_Test_002
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc002, TestSize.Level0)
{
    ItProcessPlimitsIpc002();
}

/**
* @tc.name: plimits_Ipc_Test_003
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc003, TestSize.Level0)
{
    ItProcessPlimitsIpc003();
}

/**
* @tc.name: plimits_Ipc_Test_004
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc004, TestSize.Level0)
{
    ItProcessPlimitsIpc004();
}

/**
* @tc.name: plimits_Ipc_Test_005
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc005, TestSize.Level0)
{
    ItProcessPlimitsIpc005();
}

/**
* @tc.name: plimits_Ipc_Test_006
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc006, TestSize.Level0)
{
    ItProcessPlimitsIpc006();
}

/**
* @tc.name: plimits_Ipc_Test_007
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc007, TestSize.Level0)
{
    ItProcessPlimitsIpc007();
}

/**
* @tc.name: plimits_Ipc_Test_008
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc008, TestSize.Level0)
{
    ItProcessPlimitsIpc008();
}

/**
* @tc.name: plimits_Ipc_Test_009
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc009, TestSize.Level0)
{
    ItProcessPlimitsIpc009();
}

/**
* @tc.name: plimits_Ipc_Test_010
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc010, TestSize.Level0)
{
    ItProcessPlimitsIpc010();
}

/**
* @tc.name: plimits_Ipc_Test_011
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc011, TestSize.Level0)
{
    ItProcessPlimitsIpc011();
}

/**
* @tc.name: plimits_Ipc_Test_012
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc012, TestSize.Level0)
{
    ItProcessPlimitsIpc012();
}

/**
* @tc.name: plimits_Ipc_Test_013
* @tc.desc: ipc plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, ItProcessPlimitsIpc013, TestSize.Level0)
{
    ItProcessPlimitsIpc013();
}

/**
* @tc.name: plimits_Sched_Test_001
* @tc.desc: sched plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, IItProcessPlimitsSched001, TestSize.Level0)
{
    ItProcessPlimitsSched001();
}

/**
* @tc.name: plimits_Sched_Test_002
* @tc.desc: sched plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, IItProcessPlimitsSched002, TestSize.Level0)
{
    ItProcessPlimitsSched002();
}

/**
* @tc.name: plimits_Sched_Test_003
* @tc.desc: sched plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, IItProcessPlimitsSched003, TestSize.Level0)
{
    ItProcessPlimitsSched003();
}

/**
* @tc.name: plimits_Sched_Test_004
* @tc.desc: sched plimit function test case
* @tc.require: issueI6GVPL
* @tc.author:
*/
HWTEST_F(ProcessPlimitsTest, IItProcessPlimitsSched004, TestSize.Level0)
{
    ItProcessPlimitsSched004();
}
#endif
} // namespace OHOS


namespace OHOS {
void ProcessPlimitsTest::SetUp()
{
    (void)rmdir("/proc/plimits/test");
}

void ProcessPlimitsTest::TearDown()
{
    (void)rmdir("/proc/plimits/test");
}

bool ProcessPlimitsTest::IsFile(const std::string &file)
{
    struct stat statbuf;
    return (lstat(file.c_str(), &statbuf) == 0) && S_ISREG(statbuf.st_mode);
}

bool ProcessPlimitsTest::IsDir(const std::string &path)
{
    struct stat statbuf;
    return (lstat(path.c_str(), &statbuf) == 0) && S_ISDIR(statbuf.st_mode);
}

bool ProcessPlimitsTest::IsSpecialDir(const std::string &path)
{
    return strcmp(path.c_str(), ".") == 0 || strcmp(path.c_str(), "..") == 0;
}
} // namespace OHOS

int ReadFile(const char *filepath, char *buf)
{
    FILE *fpid = nullptr;
    fpid = fopen(filepath, "r");
    if (fpid == nullptr) {
        return -1;
    }
    size_t trd = fread(buf, 1, 512, fpid);
    (void)fclose(fpid);
    return trd;
}

int WriteFile(const char *filepath, const char *buf)
{
    int fd = open(filepath, O_WRONLY);
    if (fd == -1) {
        return -1;
    }
    size_t twd = write(fd, buf, strlen(buf));
    if (twd == -1) {
        (void)close(fd);
        return -1;
    }
    (void)close(fd);
    return twd;
}

int GetLine(char *buf, int count, int maxLen, char **array)
{
    char *head = buf;
    char *tail = buf;
    char index = 0;
    if ((buf == NULL) || (strlen(buf) == 0)) {
        return 0;
    }
    while (*tail != '\0') {
        if (*tail != '\n') {
            tail++;
            continue;
        }
        if (index >= count) {
            return index + 1;
        }

        array[index] = head;
        index++;
        *tail = '\0';
        if (strlen(head) > maxLen) {
            return index + 1;
        }
        tail++;
        head = tail;
        tail++;
    }
    return (index + 1);
}

int RmdirTest(std::string path)
{
    int ret;
    RmdirControlFile(path);
    ret = rmdir(path.c_str());
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return 0;
}

int WaitForCpupStable(int expectedCpupPercent)
{
    int sleepTime;
    if (expectedCpupPercent >= QUOTA_PERCENT_100) {
        sleepTime = WAIT_CPUP_STABLE_FOR_100;
    } else {
        sleepTime = WAIT_CPUP_STABLE;
    }
    return sleep(sleepTime);
}

static int SampleRound(void)
{
    return STATISTIC_TIMES;
}

static vector<string> GetProcessInfo(pid_t pid)
{
    vector<string> contentArr;
    char buf[TEST_BUFFER_SIZE + 1] = {0};
    string strpid = to_string(pid);

    ifstream infile;
    infile.open("/proc/process");
    while (!infile.eof()) {
        infile.getline(buf, TEST_BUFFER_SIZE);
        regex e("^\\s+"+strpid);
        int matchResult = regex_search(buf, e);
        if (matchResult == 1) {
            istringstream str(buf);
            string out;
            while (str >> out) {
                contentArr.push_back(out);
            }
            break;
        }
        (void)memset_s(buf, TEST_BUFFER_SIZE, 0, TEST_BUFFER_SIZE);
    }
    infile.close();
    (void)memset_s(buf, TEST_BUFFER_SIZE, 0, TEST_BUFFER_SIZE);
    return contentArr;
}


static void SigQuit(int s)
{
    exit(0);
}

static int ChildRunCpup()
{
    struct sigaction act;
    act.sa_handler = SigQuit;
    (void)sigaction(SIGUSR1, &act, NULL);

    unsigned long x = 1;
    unsigned long y = 1;
    while (1) {
        y++;
        x *= y;
    }

    return 0;
}

int ForkChilds(int num, int *pidArray)
{
    pid_t childPid;
    pid_t pidArrayLocal[PROCESS_LIMIT_AMOUNT];

    for (int idx = 0; idx < num; idx++) {
        childPid = fork();
        if (childPid == 0) {
            (void)ChildRunCpup();
        } else if (childPid > 0) {
            pidArrayLocal[idx] = childPid;
            *pidArray = childPid;
            pidArray++;
        } else {
            return -errno;
        }
    }
    return 0;
}

static double GetCpup(pid_t pid)
{
    auto content = GetProcessInfo(pid);
    double cpup10s = atof(content[CPUP10S_INDEX].c_str());
    return cpup10s;
}

static int CollectCpupData(int childAmount, int sampleSeconds, int *pidArray, vector<vector<double>> &cpupValuesArray)
{
    double cpup10s;
    for (int i = 0; i < sampleSeconds; i++) {
        for (int j = 0; j < childAmount;j++) {
            cpup10s = GetCpup(pidArray[j]);
            cpupValuesArray[j].push_back(cpup10s);
        }
        sleep(1);
    }
    return 0;
}

static int CalcAverageCpup(int num, vector<vector<double>> &cpupValuesArray, double *cpupAverageArray)
{
    double cpup10sAverage;
    for (int idx = 0; idx < num; idx++) {
        auto size = cpupValuesArray[idx].size();
        cpup10sAverage = std::accumulate(cpupValuesArray[idx].begin(), cpupValuesArray[idx].end(), 0.0) / size;
        cpupAverageArray[idx] = cpup10sAverage;
    }
    return 0;
}

int CreatePlimitGroup(const char* groupName, char *childPidFiles,
                      unsigned long long periodUs, unsigned long long quotaUs)
{
    int ret;
    mode_t mode = 0777;
    char dirpath[TEST_BUFFER_SIZE];
    char procspath[TEST_BUFFER_SIZE];
    char periodpath[TEST_BUFFER_SIZE];
    char quotapath[TEST_BUFFER_SIZE];
    char periodValue[TEST_BUFFER_SIZE];
    char quotaValue[TEST_BUFFER_SIZE];

    if (sprintf_s(dirpath, TEST_BUFFER_SIZE, "/proc/plimits/%s", groupName) < 0) {
        return -1;
    }
    if (sprintf_s(procspath, TEST_BUFFER_SIZE, "%s/plimits.procs", dirpath) < 0) {
        return -1;
    }
    if (sprintf_s(periodpath, TEST_BUFFER_SIZE, "%s/sched.period", dirpath) < 0) {
        return -1;
    }
    if (sprintf_s(quotapath, TEST_BUFFER_SIZE, "%s/sched.quota", dirpath) < 0) {
        return -1;
    }

    ret = access(dirpath, 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = mkdir(dirpath, mode);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    if (sprintf_s(periodValue, TEST_BUFFER_SIZE, "%llu", periodUs) < 0) {
        return -1;
    }
    if (sprintf_s(quotaValue, TEST_BUFFER_SIZE, "%llu", quotaUs) < 0) {
        return -1;
    }
    ret = WriteFile(periodpath, periodValue);
    if (ret < 0) {
        printf("%s %d\n", __FUNCTION__, __LINE__);
        return ret;
    }
    ret = WriteFile(quotapath, quotaValue);
    if (ret < 0) {
        printf("%s %d\n", __FUNCTION__, __LINE__);
        return ret;
    }
    if (sprintf_s(childPidFiles, TEST_BUFFER_SIZE, "%s", procspath) < 0) {
        return -1;
    }
    return 0;
}

int CreatePlimitGroupWithoutLimit(const char* groupName, char *childPidFiles)
{
    int ret;
    mode_t mode = 0777;
    char dirpath[TEST_BUFFER_SIZE];
    char procspath[TEST_BUFFER_SIZE];

    if (sprintf_s(dirpath, TEST_BUFFER_SIZE, "/proc/plimits/%s", groupName) < 0) {
        return -1;
    }
    if (sprintf_s(procspath, TEST_BUFFER_SIZE, "%s/plimits.procs", dirpath) < 0) {
        return -1;
    }

    ret = access(dirpath, 0);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = mkdir(dirpath, mode);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    if (sprintf_s(childPidFiles, TEST_BUFFER_SIZE, "%s", procspath) < 0) {
        return -1;
    }
    return 0;
}

int TerminateChildProcess(int *childPidArray, int childAmount, int sig)
{
    int idx;
    for (idx = 0; idx < childAmount; idx++) {
        (void)kill(childPidArray[idx], SIGUSR1);

        int status;
        (void)waitpid(childPidArray[idx], &status, 0);
    }

    (void)signal(SIGUSR1, SIG_DFL);
    return 0;
}

double CalcCpupUsage(int childAmount, int *childPidArray, int expectedCpupPercent)
{
    int idx;
    int sampleSeconds = SampleRound();
    vector<vector<double>> cpupValuesArray(PROCESS_LIMIT_AMOUNT);
    (void)CollectCpupData(childAmount, sampleSeconds, &childPidArray[0], cpupValuesArray);

    double actualCpup10sArray[PROCESS_LIMIT_AMOUNT];
    (void)CalcAverageCpup(childAmount, cpupValuesArray, &actualCpup10sArray[0]);

    double sumAllChildsCpup = 0;
    for (idx = 0; idx < childAmount; idx++) {
        sumAllChildsCpup += actualCpup10sArray[idx];
    }
    return sumAllChildsCpup;
}

double CheckCpupUsage(double sumAllChildsCpup, int expectedCpupPercent)
{
    if (expectedCpupPercent <= 0.0) {
        return 500.0; /* 500.0: errno */
    }
    double errorRate = fabs(sumAllChildsCpup / expectedCpupPercent - 1.0);
    return errorRate;
}

int checkCpupUsageGreaterThan(double sumAllChildsCpup, int expectedCpupPercent)
{
    if (sumAllChildsCpup > expectedCpupPercent) {
        return 0;
    } else {
        return -1;
    }
}

double TestCpupInPlimit(int childAmount, const char* groupName,
                        unsigned long long periodUs, unsigned long long quotaUs, int expectedCpupPercent)
{
    char dirpath[TEST_BUFFER_SIZE];
    pid_t childPidArray[PROCESS_LIMIT_AMOUNT];
    char procspath[TEST_BUFFER_SIZE];
    double sumAllChildsCpup = 0;

    int ret = CreatePlimitGroup(groupName, procspath, periodUs, quotaUs);
    if (ret < 0) {
        printf("%s %d, ret=%d\n", __FUNCTION__, __LINE__, ret);
        return 100.0; /* 100.0: errno */
    }
    ret = ForkChilds(childAmount, &childPidArray[0]);
    if (ret != 0) {
        printf("%s %d, ret=%d\n", __FUNCTION__, __LINE__, ret);
        return 200.0; /* 200.0: errno */
    }
    (void)WaitForCpupStable(expectedCpupPercent);
    sumAllChildsCpup = CalcCpupUsage(childAmount, &childPidArray[0], 0);
    double errorRate = CheckCpupUsage(sumAllChildsCpup, expectedCpupPercent);

    (void)TerminateChildProcess(&childPidArray[0], childAmount, SIGUSR1);

    if (sprintf_s(dirpath, TEST_BUFFER_SIZE, "/proc/plimits/%s", groupName) < 0) {
        return 300.0; /* 300.0: errno */
    }
    ret = rmdir(dirpath);
    if (ret != 0) {
        printf("%s %d, ret=%d\n", __FUNCTION__, __LINE__, errno);
        return 400.0; /* 400.0: errno */
    }
    return errorRate;
}
