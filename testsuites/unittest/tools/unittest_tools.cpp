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

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <securec.h>

using namespace std;
const static int TEST_PATH_MAX = 255;
const static int TEST_PARAM_NUM = 5;
const static int TEST_CASE_MODE_FULL = 0;
const static int TEST_CASE_MODE_SMOKE = 1;
const static int TEST_DEFAULT_CASE_MAX = 5;
const static int TEST_T_PARAM_LEN = 2;
const static unsigned int TEST_CASE_FLAGS_ALL = 1;
const static unsigned int TEST_CASE_FLAGS_SPECIFY = 2;
const static unsigned int TEST_CASE_FAILED_COUNT_MAX = 500;
static unsigned int g_testsuitesCount = 0;
static unsigned int g_testsuitesFailedCount = 0;

struct TestCase {
    char bin[TEST_PATH_MAX];
    char *param[TEST_PARAM_NUM];
    int caseLen;
};

struct TestsuitesParam {
    char testsuitesBin[TEST_PATH_MAX];
    char testsuitesDir[TEST_PATH_MAX];
    int testsuitesDirLen;
    struct TestCase *testCase[TEST_DEFAULT_CASE_MAX];
    int testCaseNum;
    int testMode;
    int repeat;
};

static struct TestsuitesParam g_param;
static char *g_testsuitesFailedCase[TEST_CASE_FAILED_COUNT_MAX];

static vector<string> GetAllTestsuites(string testsuitesDir)
{
    vector<string> vFiles;
    int suffixLen = strlen(".bin");
    DIR *dir = opendir(testsuitesDir.c_str());
    if (dir == nullptr) {
        cout << "opendir " <<  testsuitesDir << " failed" << endl;
        return vFiles;
    }

    do {
        struct dirent *pDir = readdir(dir);
        if (pDir == nullptr) {
            break;
        }

        if ((strcmp(pDir->d_name, ".") == 0) || (strcmp(pDir->d_name, "..") == 0)) {
            continue;
        }

        if (pDir->d_type == DT_REG) {
            int pathLen = strlen(pDir->d_name);
            if (pathLen <= suffixLen) {
                continue;
            }
            if (strcmp(".bin", (char *)(pDir->d_name + (pathLen - suffixLen))) != 0) {
                continue;
            }

            vFiles.push_back(testsuitesDir + "/" + pDir->d_name);
        }
    } while (1);
    closedir(dir);
    return vFiles;
}

static bool TestsuitesIsSmoke(string testsuite, int fileLen, string smokeTest, int smokeTestLen)
{
    if (fileLen <= smokeTestLen) {
        return false;
    }

    if (strcmp(smokeTest.c_str(), (char *)(testsuite.c_str() + (fileLen - smokeTestLen))) == 0) {
        return true;
    }
    return false;
}

static void RunCase(const char *testCase, char *param[])
{
    int ret;
    int size;
    int status = 0;
    pid_t pid = fork();
    if (pid < 0) {
        cout << "fork failed, " << strerror(errno) << endl;
        return;
    }

    g_testsuitesCount++;
    if (pid == 0) {
        cout << testCase << ":" << endl;
        ret = execve(param[0], param, nullptr);
        if (ret != 0) {
            cout << "execl: " << testCase << " failed, " << strerror(errno) << endl;
            exit(0);
        }
    }

    ret = waitpid(pid, &status, 0);
    if (ret != pid) {
        cout << "waitpid failed, " << strerror(errno) << endl;
        return;
    }

    if (WEXITSTATUS(status) == 0) {
        return;
    }

    if (g_testsuitesFailedCount >= TEST_CASE_FAILED_COUNT_MAX) {
        cout << "[UNITTEST_RUN] Failure cases more than upper limit!\n";
        return;
    }

    size = strlen(testCase) + 1;
    g_testsuitesFailedCase[g_testsuitesFailedCount] = static_cast<char *>(malloc(size));
    if (g_testsuitesFailedCase[g_testsuitesFailedCount] == nullptr) {
        cout << "[UNITTEST_RUN] malloc failed!\n";
        return;
    }

    if (memcpy_s(g_testsuitesFailedCase[g_testsuitesFailedCount], size, testCase, size) != EOK) {
        cout << "[UNITTEST_RUN] memcpy failed!\n";
        return;
    }
    g_testsuitesFailedCount++;
}

static void RunAllTestCase(vector<string> files, struct TestsuitesParam *param)
{
    const char *testMode = nullptr;
    const char *smokeTest = "door.bin";
    const char *unittestRun = g_param.testsuitesBin;
    int unittestRunLen = strlen(unittestRun);
    int smokeTestLen = strlen(smokeTest);
    char *execParam[TEST_PARAM_NUM];

    if (param->testMode == TEST_CASE_MODE_SMOKE) {
        testMode = "door.bin";
    } else {
        testMode = ".bin";
    }

    for (size_t i = 0; i < files.size(); i++) {
        int fileLen = strlen(files[i].c_str());
        if (fileLen >= unittestRunLen) {
            if (strcmp((char *)(files[i].c_str() + (fileLen - unittestRunLen)), unittestRun) == 0) {
                continue;
            }
        }

        if (strcmp(testMode, smokeTest) == 0) {
            if (!TestsuitesIsSmoke(files[i], fileLen, smokeTest, smokeTestLen)) {
                continue;
            }
        } else {
            if (TestsuitesIsSmoke(files[i], fileLen, smokeTest, smokeTestLen)) {
                continue;
            }
        }

        (void)memset_s(execParam, sizeof(char *) * TEST_PARAM_NUM, 0, sizeof(char *) * TEST_PARAM_NUM);
        execParam[0] = (char *)files[i].c_str();
        RunCase(files[i].c_str(), execParam);
    }
}

static void TestsuitesToolsUsage(void)
{
    cout << "Usage: " << endl;
    cout << "liteos_unittest_run.bin [testsuites_dir] [options]" << endl;
    cout << "options:" << endl;
    cout << " -r [1-1000]             --- The number of repeated runs of the test program." << endl;
    cout << " -m [smoke/full]         --- Run the smoke or full test case in this directory." << endl;
    cout << " -t [case] [args] -t ... --- Runs the specified executable program name." << endl;
}

static int TestsuitesDirFormat(char *testDir, int len)
{
    if (memcpy_s(g_param.testsuitesDir, TEST_PATH_MAX, testDir, len + 1) != EOK) {
        cout << "testsuites dir: " << strerror(ENAMETOOLONG) << endl;
        return -1;
    }

    char *end = g_param.testsuitesDir + len;
    while ((testDir != end) && (*end == '/')) {
        *end = '\0';
        end--;
        len--;
    }

    g_param.testsuitesDirLen = len;
    if (len <= 0) {
        return -1;
    }
    return 0;
}

static int GetTestsuitesToolsName(const char *argv[])
{
    const char *testBin = static_cast<const char *>(argv[0]);
    const char *end = testBin + strlen(testBin);
    while (*end != '/') {
        end--;
    }
    end++;

    if (memcpy_s(g_param.testsuitesBin, TEST_PATH_MAX, end, strlen(end)) != EOK) {
        cout << "testsuites dir: " << strerror(ENAMETOOLONG) << endl;
        return -1;
    }
    return 0;
}

static int ParseTestCaseAndParam(int argc, const char *argv[], int *index)
{
    int j;

    (*index)++;
    if (*index >= argc) {
        return -1;
    }

    if (g_param.testCaseNum >= TEST_DEFAULT_CASE_MAX) {
        return -1;
    }

    g_param.testCase[g_param.testCaseNum] = (struct TestCase *)malloc(sizeof(struct TestCase));
    if (g_param.testCase[g_param.testCaseNum] == nullptr) {
        cout << "test case " << strerror(ENOMEM) << endl;
        return -1;
    }
    (void)memset_s(g_param.testCase[g_param.testCaseNum], sizeof(struct TestCase), 0, sizeof(struct TestCase));
    struct TestCase *testCase = g_param.testCase[g_param.testCaseNum];
    testCase->caseLen = strlen(argv[*index]);
    if (memcpy_s(testCase->bin, TEST_PATH_MAX, argv[*index], testCase->caseLen + 1) != EOK) {
        testCase->caseLen = 0;
        cout << "test case " << strerror(ENAMETOOLONG) << endl;
        return -1;
    }
    testCase->param[0] = testCase->bin;
    (*index)++;
    for (j = 1; (j < TEST_PARAM_NUM) && (*index < argc) && (strncmp("-t", argv[*index], TEST_T_PARAM_LEN) != 0); j++) {
        testCase->param[j] = (char *)argv[*index];
        (*index)++;
    }
    g_param.testCaseNum++;
    if (((*index) < argc) && (strncmp("-t", argv[*index], TEST_T_PARAM_LEN) == 0)) {
        (*index)--;
    }
    return 0;
}

static int TestsuitesParamCheck(int argc, const char *argv[])
{
    int ret;
    unsigned int mask = 0;
    g_param.testMode = TEST_CASE_MODE_FULL;
    g_param.repeat = 1;
    ret = TestsuitesDirFormat((char *)argv[1], strlen(argv[1]));
    if (ret < 0) {
        return -1;
    }

    for (int i = 2; i < argc; i++) { /* 2: param index */
        if (strcmp("-m", argv[i]) == 0) {
            i++;
            if (i >= argc) {
                return -1;
            }
            mask |= TEST_CASE_FLAGS_ALL;
            if (strcmp("smoke", argv[i]) == 0) {
                g_param.testMode = TEST_CASE_MODE_SMOKE;
            } else if (strcmp("full", argv[i]) == 0) {
                g_param.testMode = TEST_CASE_MODE_FULL;
            } else {
                return -1;
            }
        } else if (strcmp("-t", argv[i]) == 0) {
            mask |= TEST_CASE_FLAGS_SPECIFY;
            ret = ParseTestCaseAndParam(argc, argv, &i);
            if (ret < 0) {
                return ret;
            }
        } else if (strcmp("-r", argv[i]) == 0) {
            i++;
            if (i >= argc) {
                return -1;
            }
            g_param.repeat = atoi(argv[i]);
            if ((g_param.repeat <= 0) || (g_param.repeat > 1000)) { /* 1000: repeat limit */
                return -1;
            }
        }
    }

    if (((mask & TEST_CASE_FLAGS_ALL) != 0) && ((mask & TEST_CASE_FLAGS_SPECIFY) != 0)) {
        cout << "Invalid parameter combination" << endl;
        return -1;
    }
    return 0;
}

static void IsCase(vector<string> files, struct TestCase *testCase)
{
    for (size_t i = 0; i < files.size(); i++) {
        string file = files[i];
        int fileLen = strlen(file.c_str());
        if (fileLen <= testCase->caseLen) {
            continue;
        }

        const string &suffix = file.c_str() + (fileLen - testCase->caseLen);
        if (strcmp(suffix.c_str(), testCase->bin) != 0) {
            continue;
        }

        if (memcpy_s(testCase->bin, TEST_PATH_MAX, file.c_str(), fileLen + 1) != EOK) {
            testCase->caseLen = 0;
            return;
        }
        testCase->caseLen = fileLen;
        g_param.testCaseNum++;
        return;
    }
    cout << "liteos_unittest_run.bin: not find test case: " << testCase->bin << endl;
    return;
}

static int FindTestCase(vector<string> files)
{
    int count = g_param.testCaseNum;
    g_param.testCaseNum = 0;

    for (int i = 0; i < count; i++) {
        IsCase(files, g_param.testCase[i]);
    }

    if (g_param.testCaseNum == 0) {
        cout << "Not find test case !" << endl;
        return -1;
    }
    return 0;
}

static void FreeTestCaseMem(void)
{
    for (int index = 0; index < g_param.testCaseNum; index++) {
        free(g_param.testCase[index]);
        g_param.testCase[index] = nullptr;
    }
}

static void ShowTestLog(int count)
{
    cout << "[UNITTEST_RUN] Repeats: " << count << " Succeed count: "
        << g_testsuitesCount  - g_testsuitesFailedCount
        << " Failed count: " << g_testsuitesFailedCount << endl;

    if (g_testsuitesFailedCount == 0) {
        return;
    }
    cout << "[UNITTEST_RUN] Failed testcase: " << endl;
    for (int i = 0; i < g_testsuitesFailedCount; i++) {
        cout << "[" << i << "] -> " << g_testsuitesFailedCase[i] << endl;
        free(g_testsuitesFailedCase[i]);
        g_testsuitesFailedCase[i] = nullptr;
    }
}

int main(int argc, const char *argv[])
{
    int ret;
    int count = 0;

    if ((argc < 2) || (argv == nullptr)) { /* 2: param index */
        cout << argv[0] << ": " << strerror(EINVAL) << endl;
        return -1;
    }

    if ((strcmp("--h", argv[1]) == 0) || (strcmp("--help", argv[1]) == 0)) {
        TestsuitesToolsUsage();
        return 0;
    }

    (void)memset_s(&g_param, sizeof(struct TestsuitesParam), 0, sizeof(struct TestsuitesParam));

    ret = GetTestsuitesToolsName(argv);
    if (ret < 0) {
        return -1;
    }

    ret = TestsuitesParamCheck(argc, argv);
    if (ret < 0) {
        cout << strerror(EINVAL) << endl;
        FreeTestCaseMem();
        return -1;
    }

    vector<string> files = GetAllTestsuites(g_param.testsuitesDir);

    if (g_param.testCaseNum != 0) {
        ret = FindTestCase(files);
        if (ret < 0) {
            files.clear();
            FreeTestCaseMem();
            return -1;
        }
    }

    while (count < g_param.repeat) {
        if (g_param.testCaseNum == 0) {
            RunAllTestCase(files, &g_param);
        } else {
            for (int index = 0; index < g_param.testCaseNum; index++) {
                RunCase(g_param.testCase[index]->bin, g_param.testCase[index]->param);
            }
        }
        count++;
    }
    files.clear();
    FreeTestCaseMem();
    ShowTestLog(count);
    return 0;
}
