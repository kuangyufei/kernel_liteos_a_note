# LiteOS Unittest tools

## 介绍

可执行程序 ***liteos_unittest_run.bin*** 是为了提升liteos unittest 测试效率的工具。

## 使用介绍

### 1.使用帮助

```
OHOS # ./liteos_unittest_run.bin --help
Usage:
liteos_unittest_run.bin [testsuites_dir] [options]
options:
 -r [1-1000]             --- The number of repeated runs of the test program.
 -m [smoke/full]         --- Run the smoke or full test case in this directory.
 -t [case] [args] -t ... --- Runs the specified executable program name.
```
- testsuites_dir: unittest 用例所在的绝对路径

### 2.常见测试场景举例

假设单板上单元测试用例位于路径 ***/usr/bin/unittest*** 下。

#### 2.1 运行全量用例或smoke用例

- smoke命令
```
./liteos_unittest_run.bin /usr/bin/unittest -r 10 -m smoke
```
注: -r 10 表示: smoke用例运行10次， 一般用于压测

- 全量命令
```
./liteos_unittest_run.bin /usr/bin/unittest -r 10 -m full
```

注: -r 10 表示: 全量用例运行10次， 一般用于压测

#### 2.2 单或多用例组合运行

- 单用例执行命令
```
./liteos_unittest_run.bin /usr/bin/unittest -r 10 -t liteos_a_basic_unittest.bin
```
注: 只运行liteos_a_basic_unittest.bin 用例, 重复10次

```
./liteos_unittest_run.bin /usr/bin/unittest -r 10 -t liteos_a_basic_unittest.bin --gtest_filter=ExcTest.ItTestExc002
```
注: 只运行liteos_a_basic_unittest.bin 用例中的ItTestExc002，重复10次

- 多用例组合命令
```
./liteos_unittest_run.bin /usr/bin/unittest -r 10 -t liteos_a_basic_unittest.bin -t liteos_a_process_basic_pthread_unittest_door.bin
```
注: 先运行liteos_a_basic_unittest.bin再运行liteos_a_process_basic_pthread_unittest_door.bin， 重复10次。-t 最多5个。

```
./liteos_unittest_run.bin /usr/bin/unittest -r 10 -t liteos_a_basic_unittest.bin --gtest_filter=ExcTest.ItTestExc002 -t liteos_a_process_basic_pthread_unittest_door.bin --gtest_filter=ProcessPthreadTest.ItTestPthread003
```
注:先运行liteos_a_basic_unittest.bin的ItTestExc002，再运行liteos_a_process_basic_pthread_unittest_door.bin的ItTestPthread003，重复10次。-t最多5个。
