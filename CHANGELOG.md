#  (2022-03-30)


### Bug Fixes


* **arm-virt:** HW_RANDOM_ENABLE配置支持arm virt平台 ([68f9f49](https://gitee.com/openharmony/kernel_liteos_a/commits/68f9f49c2a62d3271db14ccb896c9f9fc78a60e4))
* A核代码静态告警定期清理 ([9ba725c](https://gitee.com/openharmony/kernel_liteos_a/commits/9ba725c3d486dd28fe9b2489b0f95a65354d7d86)), closes [#I4I0O8](https://gitee.com/openharmony/kernel_liteos_a/issues/I4I0O8)
* change default permission of procfs to 0550 ([a776c04](https://gitee.com/openharmony/kernel_liteos_a/commits/a776c04a3da414f73ef7136a543c029cc6dd75be)), closes [#I4NY49](https://gitee.com/openharmony/kernel_liteos_a/issues/I4NY49)
* change the execFile field in TCB to execVnode ([e4a0662](https://gitee.com/openharmony/kernel_liteos_a/commits/e4a06623ceb49b5bead60d45c0534db88b9c666f)), closes [#I4CLL9](https://gitee.com/openharmony/kernel_liteos_a/issues/I4CLL9)
* close file when process interpretor failed ([a375bf5](https://gitee.com/openharmony/kernel_liteos_a/commits/a375bf5668a5e86e082d0e124b538e423023a259)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* codex 清理 ([9ab3e35](https://gitee.com/openharmony/kernel_liteos_a/commits/9ab3e351d38cdae2ec083048a50a253bc2a3b604)), closes [#I4BL3](https://gitee.com/openharmony/kernel_liteos_a/issues/I4BL3)
* dyload open close failed ([5e87d8c](https://gitee.com/openharmony/kernel_liteos_a/commits/5e87d8c183471166294e2caa041ab4da8570c6a1)), closes [#I452Z7](https://gitee.com/openharmony/kernel_liteos_a/issues/I452Z7)
* fix ppoll ([a55f68f](https://gitee.com/openharmony/kernel_liteos_a/commits/a55f68f957e9f8ad74bd9e0c1b3d27775e0f8c75))
* fix some function declarations ([63fd8bc](https://gitee.com/openharmony/kernel_liteos_a/commits/63fd8bc39b21fffb6990f74e879eefecafad6c88))
* implicit declaration of function 'syscall' in apps/shell ([bd0c083](https://gitee.com/openharmony/kernel_liteos_a/commits/bd0c0835fc58ed5f941dbbc9adfac74253eeb874))
* LOS_Panic和魔法键功能中的使用PRINTK打印，依赖任务调度，特殊情况下存在打印不出来的问题 ([53addea](https://gitee.com/openharmony/kernel_liteos_a/commits/53addea304de09e0df457b690403ac652bbcea72)), closes [#I4NOC7](https://gitee.com/openharmony/kernel_liteos_a/issues/I4NOC7)
* los_stat_pri.h中缺少依赖的头文件 ([2cd03c5](https://gitee.com/openharmony/kernel_liteos_a/commits/2cd03c55b7a614c648adc965ebfe494d491fe20f)), closes [#I4KEZ1](https://gitee.com/openharmony/kernel_liteos_a/issues/I4KEZ1)
* los_trace.h接口注释错误修正 ([6d24961](https://gitee.com/openharmony/kernel_liteos_a/commits/6d249618aecc216388f9b1a2b48fe0ac6dd19ff2)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* MMU竞态问题修复 ([748e0d8](https://gitee.com/openharmony/kernel_liteos_a/commits/748e0d8ffb6ee9c8757ed056f575e3abc6c10702)), closes [#I2](https://gitee.com/openharmony/kernel_liteos_a/issues/I2)
* **mtd:** 去除mtd对hisilicon驱动的依赖 ([f7d010d](https://gitee.com/openharmony/kernel_liteos_a/commits/f7d010dfa4cb825096267528e131a9e2735d7505)), closes [#I49](https://gitee.com/openharmony/kernel_liteos_a/issues/I49)
* OsFutexWaitParamCheck函数中absTime为0时，直接返回，不需要打印 ([3f71be7](https://gitee.com/openharmony/kernel_liteos_a/commits/3f71be75355f11037d9de80cc4d7da0f01905003)), closes [#I4D67](https://gitee.com/openharmony/kernel_liteos_a/issues/I4D67)
* OsLockDepCheckIn异常处理中存在g_lockdepAvailable锁嵌套调用, ([bf030b6](https://gitee.com/openharmony/kernel_liteos_a/commits/bf030b6bb5843151a5b8b8736246a1376a5fb9d0)), closes [#I457](https://gitee.com/openharmony/kernel_liteos_a/issues/I457)
* pr模板补充说明 ([e3cd485](https://gitee.com/openharmony/kernel_liteos_a/commits/e3cd485db528490a16a8932d734faab263b44bc9))
* same file mode for procfs files ([c79bcd0](https://gitee.com/openharmony/kernel_liteos_a/commits/c79bcd028e1be34b45cba000077230fa2ef95e68)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* shell支持exit退出，完善帮助信息，特殊处理不可见字符 ([cc6e112](https://gitee.com/openharmony/kernel_liteos_a/commits/cc6e11281e63b6bdc9be8e5d3c39f1258eb2ceaa))
* smp初始化中副核冗余的启动框架调用 ([5ce70a5](https://gitee.com/openharmony/kernel_liteos_a/commits/5ce70a50c3733b6ec8cc4b444837e366ec837f69)), closes [#I4F8A5](https://gitee.com/openharmony/kernel_liteos_a/issues/I4F8A5)
* solve SIGCHLD ignored in sigsuspend() ([5a80d4e](https://gitee.com/openharmony/kernel_liteos_a/commits/5a80d4e1a34c94204a0bb01443bf25a4fdb12750)), closes [#I47](https://gitee.com/openharmony/kernel_liteos_a/issues/I47)
* syscall review bugfix ([214f44e](https://gitee.com/openharmony/kernel_liteos_a/commits/214f44e935277c29d347c50b553a31ea7df36448)), closes [#149](https://gitee.com/openharmony/kernel_liteos_a/issues/149)
* **test:** misc09用例因依赖hosts文件而失败 ([f2f5c5f](https://gitee.com/openharmony/kernel_liteos_a/commits/f2f5c5fdc3202610de173e7046adab4df5e59142)), closes [re#I48IZ0](https://gitee.com/re/issues/I48IZ0)
* **test:** 修复sys部分用例因依赖passwd、group文件而失败 ([614cdcc](https://gitee.com/openharmony/kernel_liteos_a/commits/614cdccf91bd2d220c4c76418b53400ce714c6cb)), closes [re#I48](https://gitee.com/re/issues/I48)
* 中断中调用PRINTK概率卡死，导致系统不能正常响应中断 ([9726ba1](https://gitee.com/openharmony/kernel_liteos_a/commits/9726ba11a79f3d2d1e616e12ef0bb44e4fc5cd20)), closes [#I4C9](https://gitee.com/openharmony/kernel_liteos_a/issues/I4C9)
* 临终遗言重定向内容缺失task相关信息，对应的shell命令中申请的内存需要cacheline对齐 ([48ca854](https://gitee.com/openharmony/kernel_liteos_a/commits/48ca854bf07f8dcda9657f950601043a485a1b33)), closes [#I482S5](https://gitee.com/openharmony/kernel_liteos_a/issues/I482S5)
* 优化liteipc任务状态，删除功能重复字段 ([5004ef4](https://gitee.com/openharmony/kernel_liteos_a/commits/5004ef4d87b54fb6d7f748ca8212ae155bcefac5)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 优化trace buffer初始化，删除swtmr 桩中的无效参数 ([b551270](https://gitee.com/openharmony/kernel_liteos_a/commits/b551270ef50cb206360e2eee3dd20ace5cecccb7)), closes [#I4DQ1](https://gitee.com/openharmony/kernel_liteos_a/issues/I4DQ1)
* 修复 virpart.c 不适配的格式化打印问题 ([de29140](https://gitee.com/openharmony/kernel_liteos_a/commits/de29140edf2567f4847876cb1ed5e0b6857420f3)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 修复A核文档失效的问题 ([456d255](https://gitee.com/openharmony/kernel_liteos_a/commits/456d255a81c2031be8ebecc2bf897af80c3d3c7a)), closes [#I4U7](https://gitee.com/openharmony/kernel_liteos_a/issues/I4U7)
* 修复A核测试用例失败的问题 ([59329ce](https://gitee.com/openharmony/kernel_liteos_a/commits/59329ce7c6b6a00084df427748e6283287a773c0)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 修复A核测试用例失败的问题 ([be68dc8](https://gitee.com/openharmony/kernel_liteos_a/commits/be68dc8bcaf8d965039ae1d792775f00a08adfac)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 修复dispatch单词拼写错误。 ([9b07aec](https://gitee.com/openharmony/kernel_liteos_a/commits/9b07aece2dfa3494cf35e8b388410341508d6224)), closes [#I4BLE8](https://gitee.com/openharmony/kernel_liteos_a/issues/I4BLE8)
* 修复futime提示错误22的BUG ([f2861dd](https://gitee.com/openharmony/kernel_liteos_a/commits/f2861ddfb424af7b99c278273601ce0fab1f37e6))
* 修复jffs2适配层错误释放锁的BUG ([011a55f](https://gitee.com/openharmony/kernel_liteos_a/commits/011a55ff21d95f969abac60bcff96f4c4d7a326d)), closes [#I4FH9](https://gitee.com/openharmony/kernel_liteos_a/issues/I4FH9)
* 修复los_vm_scan.c中内部函数OsInactiveListIsLow冗余代码 ([bc32a1e](https://gitee.com/openharmony/kernel_liteos_a/commits/bc32a1ec0fa5d19c6d2672bcf4a01de5e1be3afb)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 修复LOSCFG_FS_FAT_CACHE宏关闭后编译失败的BUG ([63e71fe](https://gitee.com/openharmony/kernel_liteos_a/commits/63e71feca05a8d46a49822c713258738740f0712)), closes [#I3T3N0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3T3N0)
* 修复OsVmPhysFreeListAdd和OsVmPhysFreeListAddUnsafe函数内容重复 ([6827bd2](https://gitee.com/openharmony/kernel_liteos_a/commits/6827bd2a22b78aa05e20d6460412fc7b2d738929)), closes [#I4FL95](https://gitee.com/openharmony/kernel_liteos_a/issues/I4FL95)
* 修复PR520缺陷 ([4033891](https://gitee.com/openharmony/kernel_liteos_a/commits/40338918d9132399ee0494d331930a05b7a13c67)), closes [re#I4DEG5](https://gitee.com/re/issues/I4DEG5)
* 修复shcmd.h需要用宏包起来的问题 ([6c4e4b1](https://gitee.com/openharmony/kernel_liteos_a/commits/6c4e4b16abe9c68fea43d40b2d39b4f0ed4bfc9c)), closes [#I4N50](https://gitee.com/openharmony/kernel_liteos_a/issues/I4N50)
* 修复xts权限用例压测异常问题 ([b0d31cb](https://gitee.com/openharmony/kernel_liteos_a/commits/b0d31cb43f5a8d1c3da574b2b957e3b0e98b3067)), closes [#I3ZJ1](https://gitee.com/openharmony/kernel_liteos_a/issues/I3ZJ1)
* 修复硬随机不可用时，地址随机化不可用问题 ([665c152](https://gitee.com/openharmony/kernel_liteos_a/commits/665c152c27bb86395ddd0395279255f6cdaf7255)), closes [#I4D4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4D4)
* 修复进程用例导致门禁概率失败 ([1ed28b4](https://gitee.com/openharmony/kernel_liteos_a/commits/1ed28b4c80cfd222be08b0c2e71e6287e52bb276)), closes [#I4FO0](https://gitee.com/openharmony/kernel_liteos_a/issues/I4FO0)
* 修复进程线程不稳定用例 ([f6ac03d](https://gitee.com/openharmony/kernel_liteos_a/commits/f6ac03d3e3c56236adc5734d4c059f1fbcc9e0c1)), closes [#I4F1](https://gitee.com/openharmony/kernel_liteos_a/issues/I4F1)
* 修复重复执行内存用例导致系统卡死问题 ([6c2b163](https://gitee.com/openharmony/kernel_liteos_a/commits/6c2b163c7d7c696ef89b17a0275f3cddb3d7cefb)), closes [#I4F7](https://gitee.com/openharmony/kernel_liteos_a/issues/I4F7)
* 修改MMU模块的注释错误 ([1a8e22d](https://gitee.com/openharmony/kernel_liteos_a/commits/1a8e22dcf15944153e013d004fd7bbf24557a8c7)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 共享内存问题修复 ([9fdb80f](https://gitee.com/openharmony/kernel_liteos_a/commits/9fdb80f85f92d0167a0456455a94fc6f679797ce)), closes [#I47X2](https://gitee.com/openharmony/kernel_liteos_a/issues/I47X2)
* 内核ERR打印，无进程和线程信息，不方便问题定位。 ([cb423f8](https://gitee.com/openharmony/kernel_liteos_a/commits/cb423f845462b8cc474c3cba261dadf3943a08ef)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 内核ERR级别及以上的打印输出当前进程和线程名 ([540b201](https://gitee.com/openharmony/kernel_liteos_a/commits/540b2017c5460e300365d2039a08abd5945cec6b))
* 内源检视测试用例问题修复 ([a6ac759](https://gitee.com/openharmony/kernel_liteos_a/commits/a6ac7597f85043ba6de3a1b395ca676d85c65ea7))
* 删除冗余的头文件 ([8e614bb](https://gitee.com/openharmony/kernel_liteos_a/commits/8e614bb1616b75bc89eee7ad7da49b7a9c285b47)), closes [#I4KN63](https://gitee.com/openharmony/kernel_liteos_a/issues/I4KN63)
* 增加pselect SYSCALL函数及测试用例 ([f601c16](https://gitee.com/openharmony/kernel_liteos_a/commits/f601c16b9e67d531dda51fc18389a53db4360b7b)), closes [#I45](https://gitee.com/openharmony/kernel_liteos_a/issues/I45)
* 增加内核epoll系统调用 ([2251b8a](https://gitee.com/openharmony/kernel_liteos_a/commits/2251b8a2d1f649422dd67f8551b085a7e0c63ec7)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 实现了musl库net模块中的一些函数接口和相应的测试用例 ([3d00a7d](https://gitee.com/openharmony/kernel_liteos_a/commits/3d00a7d23a96f29c138cfc1672825b90b9e0c05e)), closes [#I4JQI1](https://gitee.com/openharmony/kernel_liteos_a/issues/I4JQI1)
* 添加进程线程冒烟用例 ([2be5968](https://gitee.com/openharmony/kernel_liteos_a/commits/2be59680f2fb0801b43522cd38cc387c8ff38766)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 用户态进程主线程退出时，其他子线程刚好进入异常处理流程会导致系统卡死 ([d955790](https://gitee.com/openharmony/kernel_liteos_a/commits/d955790a44a679421798ec1ac2900b4d75dd75a4)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 编码规范修改 ([d161a0b](https://gitee.com/openharmony/kernel_liteos_a/commits/d161a0b03de046c05fff45a2b625631b4e45a347))
* 编码规范问题修复 ([f60bc94](https://gitee.com/openharmony/kernel_liteos_a/commits/f60bc94cf231bc615ff6603ca0393b8fe33a8c47))
* 编译框架在做编译入口的统一 ([bdb9864](https://gitee.com/openharmony/kernel_liteos_a/commits/bdb9864436a6f128a4c3891bbd63e3c60352689f)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 解决dmesg -s参数double lock问题 ([e151256](https://gitee.com/openharmony/kernel_liteos_a/commits/e1512566e322eb1fbc8f5d5997f9bfcd022feac7)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 进程退出前自己回收vmspace中的所有region ([298ccea](https://gitee.com/openharmony/kernel_liteos_a/commits/298ccea3fedaccc651b38973f0455fa1ce12e516)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 通过g_uart_fputc_en关闭打印后，shell进程不能正常启动 ([d21b05c](https://gitee.com/openharmony/kernel_liteos_a/commits/d21b05c0f69877130366ad37b852a0f30c11809d)), closes [#I4CTY2](https://gitee.com/openharmony/kernel_liteos_a/issues/I4CTY2)
* 针对pr是否同步至release分支，增加原因说明规则 ([b37a7b7](https://gitee.com/openharmony/kernel_liteos_a/commits/b37a7b79292d93dae6c4914952b5f3bb509e8721))
* 非当前进程销毁时，销毁liteipc时错误的销毁了当前进程的liteipc资源 ([0f0e85b](https://gitee.com/openharmony/kernel_liteos_a/commits/0f0e85b7a6bf76d540925fbf661c483c8dba1cba)), closes [#I4FSA7](https://gitee.com/openharmony/kernel_liteos_a/issues/I4FSA7)


### Code Refactoring

* los_cir_buf.c中接口整合 ([0d325c5](https://gitee.com/openharmony/kernel_liteos_a/commits/0d325c56a1053043db05d53a6c8083f4d35f116b)), closes [#I4MC13](https://gitee.com/openharmony/kernel_liteos_a/issues/I4MC13)


### Features


* add option SIOCGIFBRDADDR for ioctl ([4ecc473](https://gitee.com/openharmony/kernel_liteos_a/commits/4ecc473843207d259613d26b8ee176d75e7f00fd)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* add sync() to vfs ([f67c4da](https://gitee.com/openharmony/kernel_liteos_a/commits/f67c4dae5141914df2e069dce0196b14780649d8)), closes [#I480](https://gitee.com/openharmony/kernel_liteos_a/issues/I480)
* **build:** support gcc toolchain ([6e886d4](https://gitee.com/openharmony/kernel_liteos_a/commits/6e886d4233dec3b82a27642f174b920e5f98f6aa))
* L0-L1 支持Perf ([6e0a3f1](https://gitee.com/openharmony/kernel_liteos_a/commits/6e0a3f10bbbfe29d110c050da927684b6d77b961)), closes [#I47I9](https://gitee.com/openharmony/kernel_liteos_a/issues/I47I9)
* L0~L1 支持Lms ([e748fdb](https://gitee.com/openharmony/kernel_liteos_a/commits/e748fdbe578a1ddd8eb10b2e207042676231ba26)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* liteipc 静态内存优化 ([5237924](https://gitee.com/openharmony/kernel_liteos_a/commits/52379242c109e0cf442784dbe811ff9d42d5f33a)), closes [#I4G4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4G4)
* page cache backed by vnode instead of filep ([38a6b80](https://gitee.com/openharmony/kernel_liteos_a/commits/38a6b804e9291d2fdbd189825ebd7d56165ec51c)), closes [#I44](https://gitee.com/openharmony/kernel_liteos_a/issues/I44)
* 提供低功耗默认处理框架 ([212d1bd](https://gitee.com/openharmony/kernel_liteos_a/commits/212d1bd1e806530fe7e7a16ea986cb2c6fb084ed)), closes [#I4KBG9](https://gitee.com/openharmony/kernel_liteos_a/issues/I4KBG9)
* 支持AT_RANDOM以增强用户态栈保护能力 ([06ea037](https://gitee.com/openharmony/kernel_liteos_a/commits/06ea03715f0cfb8728fadd0d9c19a403dc8f6028)), closes [#I4CB8](https://gitee.com/openharmony/kernel_liteos_a/issues/I4CB8)
* 支持L1 低功耗框架 ([64e49ab](https://gitee.com/openharmony/kernel_liteos_a/commits/64e49aba7c9c7d2280c5b3f29f04b17b63209855)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 支持LOS_TaskJoin 和 LOS_TaskDetach ([37bc11f](https://gitee.com/openharmony/kernel_liteos_a/commits/37bc11fa8837a3019a0a56702f401ec1845f6547)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 新增解析异常和backtrace信息脚本 ([7019fdf](https://gitee.com/openharmony/kernel_liteos_a/commits/7019fdfcbb33c660e8aa9fd399d5fccbd7e23b49)), closes [#I47](https://gitee.com/openharmony/kernel_liteos_a/issues/I47)
* 调度tick响应时间计算优化 ([f47da44](https://gitee.com/openharmony/kernel_liteos_a/commits/f47da44b39be7fa3e9b5031d7b79b9bef1fd4fbc))
* 调度去进程化，优化进程线程依赖关系 ([dc479fb](https://gitee.com/openharmony/kernel_liteos_a/commits/dc479fb7bd9cb8441e4e47d44b42110ea07d76a2))
* 调度相关模块间依赖优化 ([0e3936c](https://gitee.com/openharmony/kernel_liteos_a/commits/0e3936c4f8b8bcfc48d283a6d53413e0fc0619b3)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 进程cpup占用率结构优化为动态分配 ([f06e090](https://gitee.com/openharmony/kernel_liteos_a/commits/f06e090a1085a654fd726fbc3c3a1c2bc703d663)), closes [#I4](https://gitee.com/openharmony/kernel_liteos_a/issues/I4)
* 进程rlimit修改为动态分配,减少静态内存占用 ([cf8446c](https://gitee.com/openharmony/kernel_liteos_a/commits/cf8446c94112ed6993a2e6e71e793d83a72689d5)), closes [#I4EZY5](https://gitee.com/openharmony/kernel_liteos_a/issues/I4EZY5)

### BREAKING CHANGES

* 1. 删除 LOS_CirBufLock()，LOS_CirBufUnlock()内核对外接口
2. LOS_CirBufWrite()，LOS_CirBufRead()由原先内部不进行上/解锁操作，变为默认已包含上/解锁操作。
* 新增支持API:

LOS_LmsCheckPoolAdd使能检测指定内存池
LOS_LmsCheckPoolDel不检测指定内存池
LOS_LmsAddrProtect为指定内存段上锁，不允许访问
LOS_LmsAddrDisableProtect去能指定内存段的访问保护
* 1.新增一系列perf的对外API，位于los_perf.h中.
    LOS_PerfInit配置采样数据缓冲区
    LOS_PerfStart开启Perf采样
    LOS_PerfStop停止Perf采样
    LOS_PerfConfig配置Perf采样事件
    LOS_PerfDataRead读取采样数据
    LOS_PerfNotifyHookReg 注册采样数据缓冲区的钩子函数
    LOS_PerfFlushHookReg 注册缓冲区刷cache的钩子

2. 用户态新增perf命令
【Usage】:
./perf [start] /[start id] Start perf.
./perf [stop] Stop perf.
./perf [read nBytes] Read nBytes raw data from perf buffer and print out.
./perf [list] List events to be used in -e.
./perf [stat] or [record] <option> <command>
         -e, event selector. use './perf list' to list available events.
         -p, event period.
         -o, perf data output filename.
         -t, taskId filter(whiltelist), if not set perf will sample all tasks.
         -s, type of data to sample defined in PerfSampleType los_perf.h.
         -P, processId filter(whiltelist), if not set perf will sample all processes.
         -d, whether to prescaler (once every 64 counts), which only take effect on cpu cycle hardware event.


#  (2021-09-30)


### Bug Fixes

*  A核代码告警清零 ([698756d](https://gitee.com/openharmony/kernel_liteos_a/commits/698756d1e6dfb11b9f874c02ce6a1496646f2c27)), closes [#I4378](https://gitee.com/openharmony/kernel_liteos_a/issues/I4378)
*  A核告警消除 ([d16bfd0](https://gitee.com/openharmony/kernel_liteos_a/commits/d16bfd005a6dae78df2df3ad55cf21e0716dfae9)), closes [#I46KF6](https://gitee.com/openharmony/kernel_liteos_a/issues/I46KF6)
* 3518平台, 异常测试进程无法正常退出 ([23937a2](https://gitee.com/openharmony/kernel_liteos_a/commits/23937a239f50d1487e89f3184e4f4b933d6842c6)), closes [#I3V8R5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3V8R5)
* add (void) to GetFileMappingList method ([56b8eca](https://gitee.com/openharmony/kernel_liteos_a/commits/56b8ecaf171671c0fe97f1dd1f191bebb7812a51)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* add capability and amend smoke testcase ([c9d69e2](https://gitee.com/openharmony/kernel_liteos_a/commits/c9d69e2d1bde57c67068cc36f6a79327d5db5daf))
* add fchdir api ([e828cbd](https://gitee.com/openharmony/kernel_liteos_a/commits/e828cbdeac7d60bfd6ed98c75a9958ee8ec7b672))
* add fchmod api ([2f214bf](https://gitee.com/openharmony/kernel_liteos_a/commits/2f214bf4de6c31875c77e84b1763e96bc5ceb669))
* add fststfs api and unitest ([4c57aa2](https://gitee.com/openharmony/kernel_liteos_a/commits/4c57aa26ad3644ce2429effc8e8fe7fd3c52ebc2))
* add product_path parameter for driver build ([88fe4eb](https://gitee.com/openharmony/kernel_liteos_a/commits/88fe4eb3e1eafa6d1e32c7b96579a07f5a8c6e35)), closes [#I3PQ10](https://gitee.com/openharmony/kernel_liteos_a/issues/I3PQ10)
* add syscall for ppoll & add 2 testcases ([defedb6](https://gitee.com/openharmony/kernel_liteos_a/commits/defedb6fdf0806da4a39e22408544cdfb405f2ff))
* avoid compile warning ignored ([eca711b](https://gitee.com/openharmony/kernel_liteos_a/commits/eca711bb64a7c806ae34c8483cac73fe22b0104f)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* A核codex修改 ([ec977a1](https://gitee.com/openharmony/kernel_liteos_a/commits/ec977a1c7e9ba4ea6621ceb1d0d10e586148208d)), closes [#I40B1](https://gitee.com/openharmony/kernel_liteos_a/issues/I40B1)
* BBOX使用预留物理内存缓存故障日志 ([2ad176e](https://gitee.com/openharmony/kernel_liteos_a/commits/2ad176e587d5ed7106831a25d386a88e26192e2b)), closes [#I41](https://gitee.com/openharmony/kernel_liteos_a/issues/I41)
* bootargs解析与rootfs挂载解耦，并支持自定义bootargs参数 ([80473f0](https://gitee.com/openharmony/kernel_liteos_a/commits/80473f0975fe6fd4c4eae3a8a473674ad2dd1293)), closes [#I41CL8](https://gitee.com/openharmony/kernel_liteos_a/issues/I41CL8)
* **build:** clang10.0.1支持lto，去掉冗余判断 ([73223ae](https://gitee.com/openharmony/kernel_liteos_a/commits/73223ae7e652b7cd9f1e1848c32b2b2573d07b2a))
* **build:** fix rootfsdir.sh target directory not exist ([21bf2b8](https://gitee.com/openharmony/kernel_liteos_a/commits/21bf2b889931787fb18ca7ea238291fd57e76d51)), closes [#I3NHQ0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3NHQ0)
* **build:** 去除冗余单板相关的宏配置 ([471de36](https://gitee.com/openharmony/kernel_liteos_a/commits/471de3663ea8b63f7448162edc0293b8723dcefc))
* clang10 lld could not recognized Oz ([87a0006](https://gitee.com/openharmony/kernel_liteos_a/commits/87a0006d5e1976fdf5efe4be383921610eb04ec6))
* clang相关编译选项隔离 ([77dcef4](https://gitee.com/openharmony/kernel_liteos_a/commits/77dcef4bc0c92d0141f7a3be0addd4d5cb533b0b)), closes [#I444](https://gitee.com/openharmony/kernel_liteos_a/issues/I444)
* codex ([101a55d](https://gitee.com/openharmony/kernel_liteos_a/commits/101a55d1199530d621f809976f0024aa8295cce8))
* codex clean ([b5370af](https://gitee.com/openharmony/kernel_liteos_a/commits/b5370af822c8e81ed2248ac8aa3bb479a0db9dae)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* compile error when fatfs disabled ([fd3f407](https://gitee.com/openharmony/kernel_liteos_a/commits/fd3f4072b5c3dff710013ccdaddf114b0e13f435))
* console compile bug fix ([f8441a0](https://gitee.com/openharmony/kernel_liteos_a/commits/f8441a0cdea301449416b0c0477dc48a05b6ab4e))
* correct spelling ([c66fe03](https://gitee.com/openharmony/kernel_liteos_a/commits/c66fe0313f76ba41992b4864c93cf82934378ad3))
* Correctly handle the return value of LOS_EventRead in QuickstartListen. ([0e0ab81](https://gitee.com/openharmony/kernel_liteos_a/commits/0e0ab81ff9474cffd5e60a8ee741840eac5d737b)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Ctrl-C move out of LOS_MAGIC_KEY_ENABLE ([40f239a](https://gitee.com/openharmony/kernel_liteos_a/commits/40f239a7d4673dc740c853b1011b5072e48385b7))
* Delete useless 'exc interaction' macros and function declarations. ([d2f2679](https://gitee.com/openharmony/kernel_liteos_a/commits/d2f26790716c9f76589c4a7d6fa5518f9ccd743c)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* dereference NULL point bug fix ([deaa564](https://gitee.com/openharmony/kernel_liteos_a/commits/deaa564a66b83c9fe19a37b8a061ec9064ece354))
* do not build toybox temporarily ([044f2c7](https://gitee.com/openharmony/kernel_liteos_a/commits/044f2c7c17c68e10aeccc3af0c222cfc722d194c))
* do not override existing libs ([8118408](https://gitee.com/openharmony/kernel_liteos_a/commits/8118408123e75f7f5a4ab9101e388be6bba0dcda))
* **doc:** 修复README_zh-HK.md的链接错误 ([f1b4c87](https://gitee.com/openharmony/kernel_liteos_a/commits/f1b4c87bc44650348504bc00b3f6f94491c341a0))
* fatfs memory leak ([fbfd71d](https://gitee.com/openharmony/kernel_liteos_a/commits/fbfd71dfe3b5555ef09e542d96a630dfba636daa))
* fix function name OsSchedSetIdleTaskSchedParam ([8f8c038](https://gitee.com/openharmony/kernel_liteos_a/commits/8f8c038b217d5ba2518413bfc1ba857587d47442))
* Fix kernel page fault exception handling causing system exception. ([a89fb57](https://gitee.com/openharmony/kernel_liteos_a/commits/a89fb57f5795236b93fd19ee2d7a059cae1b2b1b)), closes [#I3RAN4](https://gitee.com/openharmony/kernel_liteos_a/issues/I3RAN4)
* fix length typo ([12d98b1](https://gitee.com/openharmony/kernel_liteos_a/commits/12d98b144b70e726d3fd31ddc9d17ed855d0ec1b))
* fix mq function by enable mq_notify api ([4427142](https://gitee.com/openharmony/kernel_liteos_a/commits/4427142d734df8f95f4599093392904abacc5c93))
* Fix quickstart codingstyle ([2e011b6](https://gitee.com/openharmony/kernel_liteos_a/commits/2e011b672f6b96ea7e0db066a07e3f9fb6ba5df3)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* fix stored typo ([d25560f](https://gitee.com/openharmony/kernel_liteos_a/commits/d25560f8ac3216670d198cbbb25891c394e0b3c2))
* fix syscall faccessat,fstatfs,fstatat & add 6 testcases ([aa1cd24](https://gitee.com/openharmony/kernel_liteos_a/commits/aa1cd245a5711e5b53c328b688da5ddc2e783d5f))
* fix the inappropriate errno in FatFs ([af61187](https://gitee.com/openharmony/kernel_liteos_a/commits/af61187587411dca38ff62e720451f08f19c1c82)), closes [#I3O8](https://gitee.com/openharmony/kernel_liteos_a/issues/I3O8)
* fix typo in quickstart ([3b2ff4b](https://gitee.com/openharmony/kernel_liteos_a/commits/3b2ff4b71fc5ae1dc74180c33fd66f69da0059d1)), closes [#I3M1S8](https://gitee.com/openharmony/kernel_liteos_a/issues/I3M1S8)
* Fix wrong judgment in los_trace.c to avoid null pointer access. ([4d863e9](https://gitee.com/openharmony/kernel_liteos_a/commits/4d863e985b6372f7895db57b602b662dba5cadd0)), closes [#I3RT9](https://gitee.com/openharmony/kernel_liteos_a/issues/I3RT9)
* Identical condition 'ret<0', second condition is always false. :bug: ([1348809](https://gitee.com/openharmony/kernel_liteos_a/commits/1348809807548dd481286a55a6677a9169fcd705))
* init进程收到子进程退出信号后，调用fork重新拉起进程，会导致系统卡死 ([35a2f3a](https://gitee.com/openharmony/kernel_liteos_a/commits/35a2f3af33a5dba1e0ba36587efced8fead97223)), closes [#I41](https://gitee.com/openharmony/kernel_liteos_a/issues/I41)
* kernel crashed after rmdir the umounted folder ([ac0d083](https://gitee.com/openharmony/kernel_liteos_a/commits/ac0d083b1c89c4ad3fdaab0347d11f174d0185cb))
* kernel crashed when delete a umounted folder ([f305d1f](https://gitee.com/openharmony/kernel_liteos_a/commits/f305d1f702a770010125a0f4f0152ffefeeec5c4))
* kernel crashed when delete a umounted folder ([c6e9212](https://gitee.com/openharmony/kernel_liteos_a/commits/c6e921241b1cf26bcbfeea44e20f6b596512e259))
* **kernel_test:** 内核mem/shm冒烟用例重复运行失败 ([0676578](https://gitee.com/openharmony/kernel_liteos_a/commits/0676578aae09c101d79e7a276823e849df293e29)), closes [#I3TH4](https://gitee.com/openharmony/kernel_liteos_a/issues/I3TH4)
* L1 toybox 命令功能实现 ([2ff44c4](https://gitee.com/openharmony/kernel_liteos_a/commits/2ff44c4938d2e66ce60e9497c505eb6e97efc8c2)), closes [#I41N2](https://gitee.com/openharmony/kernel_liteos_a/issues/I41N2)
* liteipc max data size too small ([4dc421e](https://gitee.com/openharmony/kernel_liteos_a/commits/4dc421e3decdaae60219d533c4a93dbc9334e35f))
* LiteOS_A BBOX Codex整改 ([6a5a032](https://gitee.com/openharmony/kernel_liteos_a/commits/6a5a0326d2b18ffacce5d38fb351530973c2f245)), closes [#I43](https://gitee.com/openharmony/kernel_liteos_a/issues/I43)
* Liteos-a创建的文件夹在Ubuntu中不可见 ([a9fc1e0](https://gitee.com/openharmony/kernel_liteos_a/commits/a9fc1e0e5d3e5eb8a34c568a8d147320a9afebe1)), closes [#I3XMY6](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XMY6)
* lookup new vnode may cause parent vnode freeing ([902a11d](https://gitee.com/openharmony/kernel_liteos_a/commits/902a11de9a4b08e799820fb87942cb2171b9e095)), closes [#I3MYP4](https://gitee.com/openharmony/kernel_liteos_a/issues/I3MYP4)
* **mini:** fix compile error in mini liteos_a ([e13cb3b](https://gitee.com/openharmony/kernel_liteos_a/commits/e13cb3bcc4e74d4c49c31d290d686f99a4e81e3f))
* minimal compile ([ac8c2c6](https://gitee.com/openharmony/kernel_liteos_a/commits/ac8c2c6d5b9ede0e46b48212f0b3447605a613c4))
* misspell ([08980ea](https://gitee.com/openharmony/kernel_liteos_a/commits/08980eac3c249ec6a6717d8a04fb0be7716053a1))
* mkdir -p is more robust ([e38f9a9](https://gitee.com/openharmony/kernel_liteos_a/commits/e38f9a98c9d373db8381af707c5996d1fefef64e))
* mksh compile bug fix ([d8263b1](https://gitee.com/openharmony/kernel_liteos_a/commits/d8263b1e91d76706d52df321ef642c1c8297e8e4)), closes [#I3ZMR7](https://gitee.com/openharmony/kernel_liteos_a/issues/I3ZMR7)
* modify event API description ([937734b](https://gitee.com/openharmony/kernel_liteos_a/commits/937734b1e9f5fc52e0ac727549bd2e4b4c864e3c))
* move nuttx head file back ([4c8a86e](https://gitee.com/openharmony/kernel_liteos_a/commits/4c8a86ece7cafa8f74441bacc74c7788c0a1b780)), closes [#I4443](https://gitee.com/openharmony/kernel_liteos_a/issues/I4443)
* nanosleep 接口的rmtp参数被错误清零 ([9458de9](https://gitee.com/openharmony/kernel_liteos_a/commits/9458de9ac664cd75540c8b638b9a6caf82812fc8)), closes [#I41U0](https://gitee.com/openharmony/kernel_liteos_a/issues/I41U0)
* no exception information output, when the system is abnormal ([5bf4d1c](https://gitee.com/openharmony/kernel_liteos_a/commits/5bf4d1c7128e6718611bbec23cb4373bcb3bfe55))
* no exception information output, when the system is abnormal ([28aa777](https://gitee.com/openharmony/kernel_liteos_a/commits/28aa777191e7b41e4f1f346ab79f8688edc59c6d))
* no exception information output, when the system is abnormal after holding ([0ed8adf](https://gitee.com/openharmony/kernel_liteos_a/commits/0ed8adfe3ae59672e284331d40ef3b9e0bc8e094))
* Optimiz macro of quickstart cmd ([cb140a4](https://gitee.com/openharmony/kernel_liteos_a/commits/cb140a4442c02dd0b1925cc58deecbf4da36dcea)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Optimize /dev/quickstart permissions. ([4e24b57](https://gitee.com/openharmony/kernel_liteos_a/commits/4e24b572897c33e1b2723122c0e13d3931096bae)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Optimize quickstart node implementation ([4abd2e0](https://gitee.com/openharmony/kernel_liteos_a/commits/4abd2e0247b14718bb21756fb446142140d30a88)), closes [#I3R8O8](https://gitee.com/openharmony/kernel_liteos_a/issues/I3R8O8)
* OsGerCurrSchedTimeCycle 函数存在拼写错误 ([53ced1a](https://gitee.com/openharmony/kernel_liteos_a/commits/53ced1a85edc3d06230225ced3916772f432243f)), closes [#I446](https://gitee.com/openharmony/kernel_liteos_a/issues/I446)
* OsGetArgsAddr声明所在头文件不正确 ([14bd753](https://gitee.com/openharmony/kernel_liteos_a/commits/14bd753aa8beb49af887c8d0aedaefa446409760)), closes [#I41](https://gitee.com/openharmony/kernel_liteos_a/issues/I41)
* parent point of vnode found by VfsHashGet should be updated ([f32caa5](https://gitee.com/openharmony/kernel_liteos_a/commits/f32caa52c67eca20d58676be4e0439acbb4c5ac3)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* PathCacheFree try to free name field in struct PathCache ([9f47faf](https://gitee.com/openharmony/kernel_liteos_a/commits/9f47faff732609cf2249f104b94ed55f3ef133bb)), closes [#I3NTH9](https://gitee.com/openharmony/kernel_liteos_a/issues/I3NTH9)
* PathCacheMemoryDump miscalculate the RAM usage of PathCache ([87da7c7](https://gitee.com/openharmony/kernel_liteos_a/commits/87da7c794c9bc8e311ae5242820fbf8aefd8d558)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Provide a separate configuration macro for boot environment in RAM. ([e28e06b](https://gitee.com/openharmony/kernel_liteos_a/commits/e28e06b08f5b28091153e584cb9d93014a9f2b10))
* Provide a separate configuration macro for boot environment in RAM. ([bc67393](https://gitee.com/openharmony/kernel_liteos_a/commits/bc67393a67f714e0a8165b6103e53699c676341c))
* Provide a separate configuration macro for boot environment in RAM. ([f13b90e](https://gitee.com/openharmony/kernel_liteos_a/commits/f13b90e4308ba3d60683799b9ce62fb3705ac277)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* race condition in liteipc ([7e2aef2](https://gitee.com/openharmony/kernel_liteos_a/commits/7e2aef2480b15f71ee0e089cef4443f578e03dbb)), closes [#I3PW5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3PW5)
* rebuild mksh depends on rebuild.sh script ([bef4ab9](https://gitee.com/openharmony/kernel_liteos_a/commits/bef4ab9835dec865898c74337dfb7154718b94aa))
* remove redundant headfile ([73a7777](https://gitee.com/openharmony/kernel_liteos_a/commits/73a777777e1b834192f6bb2c0e8bd03c69765c11)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Remove redundant invalid codes ([fe05df4](https://gitee.com/openharmony/kernel_liteos_a/commits/fe05df40ae710632adb1bd54fb04ad4a8e0c1429)), closes [#I3R70](https://gitee.com/openharmony/kernel_liteos_a/issues/I3R70)
* Show conflicting files for git apply or patch -p command ([35f7957](https://gitee.com/openharmony/kernel_liteos_a/commits/35f79579758644cbcac6005f894be21f34f42b99)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* SIOCGIFCONF ioctl malloc size error in kernel ([bfd27e7](https://gitee.com/openharmony/kernel_liteos_a/commits/bfd27e78b259eedbd4c30548e80c9b4d789d26e1)), closes [#I3XEZ3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XEZ3)
* smp启动代码解耦及内存映射关系解耦 ([3bb3173](https://gitee.com/openharmony/kernel_liteos_a/commits/3bb3173604dc4dea09301f61da402974185dc112)), closes [#I41P8](https://gitee.com/openharmony/kernel_liteos_a/issues/I41P8)
* some branch in format does not release the vnode mutex properly ([9b2b700](https://gitee.com/openharmony/kernel_liteos_a/commits/9b2b700fa0e3a8edadb38f09c917a3e3f013347f)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Sortlink, the response time to insert the node should be inserted into the back of the existing node. ([1323874](https://gitee.com/openharmony/kernel_liteos_a/commits/1323874389030c55fb053fead12ca1c6654d8851)), closes [#I3PSJ8](https://gitee.com/openharmony/kernel_liteos_a/issues/I3PSJ8)
* statfs can't get f_bfree and f_bavail of a FAT12/FAT16 partition ([9503c4a](https://gitee.com/openharmony/kernel_liteos_a/commits/9503c4a9fc30aaa8ea2031d4fb794436f3c98055)), closes [#I3Q0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Q0)
* SysOpenat返回值应该为进程fd，而非系统全局fd。 ([3457c0b](https://gitee.com/openharmony/kernel_liteos_a/commits/3457c0b11debb2cf5d4abe325c2510a750654512)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* the total used vnode number not increased after unlink a file under ([4f514a1](https://gitee.com/openharmony/kernel_liteos_a/commits/4f514a16af5a3f3b9c4ea182de774297afb17f66)), closes [#I3TS1](https://gitee.com/openharmony/kernel_liteos_a/issues/I3TS1)
* tick 动态化计算优化，消除中断执行时间对系统总体时间的影响，保证软件定时器的响应精度。 ([8df3e8c](https://gitee.com/openharmony/kernel_liteos_a/commits/8df3e8c96515a1907455a9bbc61426f43c803c62)), closes [#I43](https://gitee.com/openharmony/kernel_liteos_a/issues/I43)
* toybox update ([c3245b3](https://gitee.com/openharmony/kernel_liteos_a/commits/c3245b3ce317eb58c8dad58fdfc7f094b9b9794b))
* toybox命令升级 ([76f45b3](https://gitee.com/openharmony/kernel_liteos_a/commits/76f45b3fb2783d1cab45a6ba08787c29dfa2c948)), closes [#I41N2](https://gitee.com/openharmony/kernel_liteos_a/issues/I41N2)
* update LOS_DL_LIST_IS_END comment ([900269b](https://gitee.com/openharmony/kernel_liteos_a/commits/900269bd4645252d898f6d3b73de01d8548c960c))
* userfs分区的起始地址与大小改为通过bootargs配置 ([2e2b142](https://gitee.com/openharmony/kernel_liteos_a/commits/2e2b14205f4b81c7483ab42d62f144487097e9a4)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* VnodeInUseIter and VnodeFreeAll used to be recursive ([5f6656c](https://gitee.com/openharmony/kernel_liteos_a/commits/5f6656cb36a2ffb0b290aeb53981aba7501e80b2)), closes [#I3NN3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3NN3)
* 以g_sysSchedStartTime是否为0判断时间轴是否生效存在极限场景导致调度时间不生效 ([67ac8c4](https://gitee.com/openharmony/kernel_liteos_a/commits/67ac8c4c586adc371ece56700fd42371ca7fc885)), closes [#I45HP5](https://gitee.com/openharmony/kernel_liteos_a/issues/I45HP5)
* 修复FATFS中不同内部接口不支持FAT12/FAT16 FAT表结束标志 ([33f5c70](https://gitee.com/openharmony/kernel_liteos_a/commits/33f5c70e6c8781758d2bfde0f60ca3d6ec7d543f)), closes [#I409R6](https://gitee.com/openharmony/kernel_liteos_a/issues/I409R6)
* 修复kill进程时，因liteipc阻塞的进程概率无法退出问题 ([7de43bb](https://gitee.com/openharmony/kernel_liteos_a/commits/7de43bb004a12f926550f0b36ca3c2fa610feb27)), closes [#I3XX7](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XX7)
* 修复MagicKey数组越界访问 ([071cd62](https://gitee.com/openharmony/kernel_liteos_a/commits/071cd6268aeaaef702387dacaf4ef1ffbb7618fc)), closes [#I3U4N9](https://gitee.com/openharmony/kernel_liteos_a/issues/I3U4N9)
* 修复mprotect修改地址区间的权限后，相应的区间名丢失问题 ([e425187](https://gitee.com/openharmony/kernel_liteos_a/commits/e425187d176a5e09bc5253faefc9cfc8454da0a3)), closes [#I43R40](https://gitee.com/openharmony/kernel_liteos_a/issues/I43R40)
* 修复mq_close关闭后仍然占用文件描述符的问题 ([590c7b4](https://gitee.com/openharmony/kernel_liteos_a/commits/590c7b4e22662781d05ad035a785e985d573ec33)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* 修复mqueue问题 ([26ee8b8](https://gitee.com/openharmony/kernel_liteos_a/commits/26ee8b836e346e641de4ecd72284f5d377bca6f8)), closes [#I43P4](https://gitee.com/openharmony/kernel_liteos_a/issues/I43P4)
* 修复sigwait等待到的信号值与获取的siginfo中的值不一致 ([ed7defb](https://gitee.com/openharmony/kernel_liteos_a/commits/ed7defbd439c345a07159d40eb04433afd530004)), closes [#I3M12](https://gitee.com/openharmony/kernel_liteos_a/issues/I3M12)
* 修复了LOSCFG_FS_FAT_VIRTUAL_PARTITION宏开关错误作用域引起的功能错误 ([acda419](https://gitee.com/openharmony/kernel_liteos_a/commits/acda419a2d773ea890052a16ee66b4872f941fbb)), closes [#I3W1](https://gitee.com/openharmony/kernel_liteos_a/issues/I3W1)
* 修复了内核的VFAT测试用例 ([a8384b5](https://gitee.com/openharmony/kernel_liteos_a/commits/a8384b5db2274d50a350910d7cfd3fc36b4f4103)), closes [#I3XF3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XF3)
* 修复内核access chmod chown接口 ([56a95b9](https://gitee.com/openharmony/kernel_liteos_a/commits/56a95b9ec903f815f9199ac65ca318e00a83b2ed)), closes [#I3Z5L6](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Z5L6)
* 修复内核c库的makefile中被优化函数替换的高频函数依然参与了编译的问题 ([6f6dc4f](https://gitee.com/openharmony/kernel_liteos_a/commits/6f6dc4f24c8ab05a6d42da46a9fa41d74a477bd7)), closes [#I3XGM8](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XGM8)
* 修复内核堆完整性检查逻辑中访问非法指针导致系统异常问题。 ([30f5ab8](https://gitee.com/openharmony/kernel_liteos_a/commits/30f5ab89b72d45f596b15af82a8e8e01add7977d)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* 修复启动框架debug模式下-Werror=maybe-uninitialized告警以及符号链接不进镜像的隐患 ([4c02415](https://gitee.com/openharmony/kernel_liteos_a/commits/4c024159a9600745863e49ee955308470a5b657b)), closes [#I3T5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3T5)
* 修复开机概率挂死 ([2e82c36](https://gitee.com/openharmony/kernel_liteos_a/commits/2e82c361f73b2119613c3571c48faae8ec41d4eb)), closes [#I3SWY2](https://gitee.com/openharmony/kernel_liteos_a/issues/I3SWY2)
* 修复文档链接失效问题 ([42a3a6c](https://gitee.com/openharmony/kernel_liteos_a/commits/42a3a6c51bfca4ade8ce94084ea2f8eb1c86a137)), closes [#I45297](https://gitee.com/openharmony/kernel_liteos_a/issues/I45297)
* 修改/proc/mounts显示格式 ([6860246](https://gitee.com/openharmony/kernel_liteos_a/commits/6860246cfaac6f540665e79bbc4c3d54c419b092)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* 修改clock_gettime接口适配posix标准测试用例011输入clk_id错误时返回值ESRCH为EINVAL. ([f8cf6e6](https://gitee.com/openharmony/kernel_liteos_a/commits/f8cf6e6439ee017fe8e0d4ecfc9949c28fa6775f)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* 修改DoNanoSleep 以纳秒为单位 ([6917e08](https://gitee.com/openharmony/kernel_liteos_a/commits/6917e08431689ccbd5a30e9a39b67016ffb64d0a)), closes [#I3Z9](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Z9)
* 修改某些平台保存bbox日志失败的问题 ([8f6a1dd](https://gitee.com/openharmony/kernel_liteos_a/commits/8f6a1dd33c59164070e8d2fb5abbd3e76ef8ac8e)), closes [#I41](https://gitee.com/openharmony/kernel_liteos_a/issues/I41)
* 修改默认窗口宽度到400 ([09c491c](https://gitee.com/openharmony/kernel_liteos_a/commits/09c491ca1fd6d3110eafb0d07e45b25bdc3bd5d0)), closes [#I40](https://gitee.com/openharmony/kernel_liteos_a/issues/I40)
* 关闭jffs和fat后，rootfs配置也被相应关闭，导致nand介质的rootfs不可用。 ([0ea476b](https://gitee.com/openharmony/kernel_liteos_a/commits/0ea476b9741fabd3c255c2770688f587920cf509)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* 内核态在console初始化完成后，使用printf无法正常打印 ([44ce696](https://gitee.com/openharmony/kernel_liteos_a/commits/44ce6969048c2e252f3f9d66fa485c77bdab8ae7)), closes [#I3UG00](https://gitee.com/openharmony/kernel_liteos_a/issues/I3UG00)
* 去掉冗余的strip操作 ([7819d15](https://gitee.com/openharmony/kernel_liteos_a/commits/7819d15b3619681ae37a1349a6bec2440c829ae9)), closes [#I43767](https://gitee.com/openharmony/kernel_liteos_a/issues/I43767)
* 合并进程栈两个地址连续的region ([42f374d](https://gitee.com/openharmony/kernel_liteos_a/commits/42f374dd7a353f1c8227e92fa92827c1c7b32424)), closes [#I43](https://gitee.com/openharmony/kernel_liteos_a/issues/I43)
* 在内核提示No idle TCB时，增加打印当前系统任务信息，以方便问题定位。 ([11a9b00](https://gitee.com/openharmony/kernel_liteos_a/commits/11a9b00d43a6dbe9fba9f6eb07d78a9d226c28f8)), closes [#I434](https://gitee.com/openharmony/kernel_liteos_a/issues/I434)
* 增加表头，内容以制表符分栏。 ([e9ad6b7](https://gitee.com/openharmony/kernel_liteos_a/commits/e9ad6b71c3b1136414672cd21b339ded7a02dfb9)), closes [#I3W2M9](https://gitee.com/openharmony/kernel_liteos_a/issues/I3W2M9)
* 将用户态内存调测解析脚本移至tools目录下 ([85b4cb7](https://gitee.com/openharmony/kernel_liteos_a/commits/85b4cb7a67b717984d3ebf68115c7139cc61d690)), closes [#I42T9](https://gitee.com/openharmony/kernel_liteos_a/issues/I42T9)
* 恢复了FATFS设置卷标的功能 ([9515d53](https://gitee.com/openharmony/kernel_liteos_a/commits/9515d53dccc9a6458bfbaf8e15143928f05cb660)), closes [#I3Y5G8](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Y5G8)
* 解决kill进程时无法保证进程的已持有的内核资源合理释放. ([cf89f01](https://gitee.com/openharmony/kernel_liteos_a/commits/cf89f016e93f28f8d72e6f5de08640d08a155908)), closes [#I3S0N0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3S0N0)
* 解决不同环境下计算的rootfs的size偏小，导致mcopy造成的disk full错误 ([c54879b](https://gitee.com/openharmony/kernel_liteos_a/commits/c54879b54875abece53b1f64072cfacbd2e1970e)), closes [#I3IA06](https://gitee.com/openharmony/kernel_liteos_a/issues/I3IA06)
* 设置qemu默认userfs大小/修改qemu驱动目录 ([1d952a2](https://gitee.com/openharmony/kernel_liteos_a/commits/1d952a254a6e7ec2f70f7112eb27355cfd01ba4d)), closes [#I3XW96](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XW96) [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)


### Features

* active mksh & toybox ([cacb4f0](https://gitee.com/openharmony/kernel_liteos_a/commits/cacb4f0103ecb2d1b7af54d84957f88d3e9443d2)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* Add /dev/quickstart to support synchronous communication between processes in user mode startup. ([46b63f7](https://gitee.com/openharmony/kernel_liteos_a/commits/46b63f71537789743ed2f3f337c1809ee4900d71)), closes [#I3OHO5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3OHO5)
* add /proc/fd file to dump the pid/fd/path information ([600dded](https://gitee.com/openharmony/kernel_liteos_a/commits/600dded31e80df275ed08c326c0ce3d1857d8dd8)), closes [#I3WB5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3WB5)
* add /proc/fs_cache to display cache info ([231cb6b](https://gitee.com/openharmony/kernel_liteos_a/commits/231cb6be27380948bc48136ae2e02c28450f47a8)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* add /proc/fs_cache to display cache info ([53c6d96](https://gitee.com/openharmony/kernel_liteos_a/commits/53c6d96c6f97cb006216769f8b73f50fba3bc325)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* add and fix some syscall ([ce849f2](https://gitee.com/openharmony/kernel_liteos_a/commits/ce849f2145e1569517c5244c9ffcaebed53f3a66))
* add blackbox for liteos_a ([a195aac](https://gitee.com/openharmony/kernel_liteos_a/commits/a195aac9fb60b0163f9eebb29084287d226bd14c)), closes [#I406](https://gitee.com/openharmony/kernel_liteos_a/issues/I406)
* add blackbox for liteos_a ([425975e](https://gitee.com/openharmony/kernel_liteos_a/commits/425975e4811023e31a520979c20ea2562224d9d0)), closes [#I3NN7](https://gitee.com/openharmony/kernel_liteos_a/issues/I3NN7)
* add clear cache cmd to /proc/fs_cache ([3d1cf68](https://gitee.com/openharmony/kernel_liteos_a/commits/3d1cf683f37a6f75e9db2ce1f11ec93159b77663)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* add liteos patch ability ([98ca844](https://gitee.com/openharmony/kernel_liteos_a/commits/98ca8441fe75ee1c3318a6a09376fd72f5a51350))
* add support for gn build system ([a8805a6](https://gitee.com/openharmony/kernel_liteos_a/commits/a8805a65aab9109b915364fccf36c8328f636e48))
* add uid/gid for ProcFs ([6780659](https://gitee.com/openharmony/kernel_liteos_a/commits/67806596a3c0df9a54893d8c74ebe9da98d7fb06)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* A核proc文件系统在echo模式下新增write的功能 ([f10dd7c](https://gitee.com/openharmony/kernel_liteos_a/commits/f10dd7c135d37a5fb0d78610d5ece5807d01eb1f)), closes [#I3T6](https://gitee.com/openharmony/kernel_liteos_a/issues/I3T6)
* build OHOS_Image from kernel ([abf4d8f](https://gitee.com/openharmony/kernel_liteos_a/commits/abf4d8fb252631aca9d46211762880a23be2a0b1))
* **build:** make liteos_config_file available as gn argument ([f9a9077](https://gitee.com/openharmony/kernel_liteos_a/commits/f9a907772f506ca7e2d84e96d4d17cffc7dae50f))
* **build:** 使用xts -notest选项时，内核用例不参与编译构建和用例编译配置方式调整 ([9bdf716](https://gitee.com/openharmony/kernel_liteos_a/commits/9bdf716407c125c9877b6f49c2133112c18cdabc))
* don't apply module_swith on configs of config ([f3beb4b](https://gitee.com/openharmony/kernel_liteos_a/commits/f3beb4b273c9a76fbfaffe57302f27d2a74cd2ba))
* enable gn build for toybox and mksh ([c54bfa1](https://gitee.com/openharmony/kernel_liteos_a/commits/c54bfa16e3907482c252e9738192df869c63326e))
* fatfs支持符号链接功能 ([e50cf0b](https://gitee.com/openharmony/kernel_liteos_a/commits/e50cf0be6f6d118022b6d79c5177aece574ba150)), closes [#I3V8D1](https://gitee.com/openharmony/kernel_liteos_a/issues/I3V8D1)
* **file system:** add memory-based romfs ([c4595d2](https://gitee.com/openharmony/kernel_liteos_a/commits/c4595d25042b188570d991a815009764c37f3cb3)), closes [#I3S0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3S0)
* introduce mksh toybox to rootfs ([41c7689](https://gitee.com/openharmony/kernel_liteos_a/commits/41c7689dfa49c52fb15705ce15ef22a278fbf2ef))
* L0-L1 支持Trace ([dc9ec68](https://gitee.com/openharmony/kernel_liteos_a/commits/dc9ec6856fe14aa604f037f55f17a3ba2bafb098)), closes [#I46WA0](https://gitee.com/openharmony/kernel_liteos_a/issues/I46WA0)
* L1支持低功耗投票框架 ([21d8ac8](https://gitee.com/openharmony/kernel_liteos_a/commits/21d8ac8752469f6e0dc2d50d28b75421f665385a)), closes [#I3VS5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3VS5)
* **libc:** upgrade optimized-routines to v21.02 ([1ec8d5a](https://gitee.com/openharmony/kernel_liteos_a/commits/1ec8d5a454658e95c4acb821d0878f357f4e82ad))
* **make:** optimize makefiles and remove some unused files ([0e26094](https://gitee.com/openharmony/kernel_liteos_a/commits/0e260949c962158a263d4b0ad45fe9f6843d6e30))
* Open macro for ADC moudule ([c71ec9d](https://gitee.com/openharmony/kernel_liteos_a/commits/c71ec9d7e8574078a9dc2a8463f3141cca3d625a))
* **QEMU_ARM_VIRT_CA7:** 使能FAT文件系统编译选项 ([49856dc](https://gitee.com/openharmony/kernel_liteos_a/commits/49856dc1e0de1c5e92346c83eda641dfb1bb16be))
* support .mkshrc ([51a50c9](https://gitee.com/openharmony/kernel_liteos_a/commits/51a50c95b4cd47b6931523926b728d1c0ff8988b)), closes [#I3Y5](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Y5)
* support link/symlink/readlink ([6eddc86](https://gitee.com/openharmony/kernel_liteos_a/commits/6eddc869d349be59860bfd84ff10b7579a9b00a9)), closes [#I3Q0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Q0)
* support toybox in qemu ([5618319](https://gitee.com/openharmony/kernel_liteos_a/commits/561831928bb1ad1871b2d60d2bba2d67488cea5f)), closes [#I3V17](https://gitee.com/openharmony/kernel_liteos_a/issues/I3V17)
* timer_create支持以SIGEV_THREAD方式创建定时器 ([e5f6bf0](https://gitee.com/openharmony/kernel_liteos_a/commits/e5f6bf05567c2d193f3746caca502199e7b81e92)), closes [#I3](https://gitee.com/openharmony/kernel_liteos_a/issues/I3)
* using kconfiglib instead of kconfig ([8784694](https://gitee.com/openharmony/kernel_liteos_a/commits/878469468647a19f704e7ee2af696b0ba8ab775a))
* vfs support sdcard hotplug ([2db80ec](https://gitee.com/openharmony/kernel_liteos_a/commits/2db80ecb389176b53c77807567895470bb180a06)), closes [#I44WH1](https://gitee.com/openharmony/kernel_liteos_a/issues/I44WH1)
* **vfs:** vfs支持FD_CLOEXEC标记 ([27dca4d](https://gitee.com/openharmony/kernel_liteos_a/commits/27dca4d857ef8de6b4bb9302e0dd435be7e3284f)), closes [#I3U81](https://gitee.com/openharmony/kernel_liteos_a/issues/I3U81)
* 删除zpfs冗余代码 ([3393479](https://gitee.com/openharmony/kernel_liteos_a/commits/3393479c52f4a9dfe2394bb32beb4b98fca8e171))
* 基于汇编实现内核对用户态内存清零的功能 ([9db3407](https://gitee.com/openharmony/kernel_liteos_a/commits/9db3407589bb0b2d4a5772faac3130032bc0b8a9)), closes [#I3XXT0](https://gitee.com/openharmony/kernel_liteos_a/issues/I3XXT0)
* 增加mount的MS_RDONLY标志的支持 ([8729f6e](https://gitee.com/openharmony/kernel_liteos_a/commits/8729f6ee57ea57bc664d076ff112c8726fedded9)), closes [#I3Z1W6](https://gitee.com/openharmony/kernel_liteos_a/issues/I3Z1W6)
* 支持killpg和waitid ([dc3cc09](https://gitee.com/openharmony/kernel_liteos_a/commits/dc3cc094a75c6da65d87522930afea06106d3933))
* 新增toybox reboot命令 ([e567a10](https://gitee.com/openharmony/kernel_liteos_a/commits/e567a10d021c3e0fe02d6911dbe8687255c67ffa)), closes [#I3YQ7](https://gitee.com/openharmony/kernel_liteos_a/issues/I3YQ7)
* 给开发者提供系统信息导出Hidumper工具。 ([cb17fa5](https://gitee.com/openharmony/kernel_liteos_a/commits/cb17fa50ed28e6e8f59b7d68ede13c759b983311)), closes [#I3NN7](https://gitee.com/openharmony/kernel_liteos_a/issues/I3NN7)
* 自研shell命令回补 ([7bc68f4](https://gitee.com/openharmony/kernel_liteos_a/commits/7bc68f454ff5fbf415712c9b8479b8c832f68417)), closes [#I44U0](https://gitee.com/openharmony/kernel_liteos_a/issues/I44U0)


### Performance Improvements

* assign '-1' to uninitialized variable: ret ([e0a27ba](https://gitee.com/openharmony/kernel_liteos_a/commits/e0a27badde20868cf3a5b4a44305b90af00e4f3f))


### Reverts

* Revert "fix: 修改默认窗口宽度到400" ([1878849](https://gitee.com/openharmony/kernel_liteos_a/commits/187884937cd087e39a28c033c41033163d6b4766)), closes [#I42X9](https://gitee.com/openharmony/kernel_liteos_a/issues/I42X9)


### BREAKING CHANGES

* 1.新增一系列trace的对外API，位于los_trace.h中.
        LOS_TRACE_EASY简易插桩
        LOS_TRACE标准插桩
        LOS_TraceInit配置Trace缓冲区的地址和大小
        LOS_TraceStart开启事件记录
        LOS_TraceStop停止事件记录
        LOS_TraceRecordDump输出Trace缓冲区数据
        LOS_TraceRecordGet获取Trace缓冲区的首地址
        LOS_TraceReset清除Trace缓冲区中的事件
        LOS_TraceEventMaskSet设置事件掩码，仅记录某些模块的事件
        LOS_TraceHwiFilterHookReg注册过滤特定中断号事件的钩子函数
* **vfs:** 执行exec类函数后，进程拥有的文件描述符情况发生变化：修改前，默认关闭所有的进程文件描述符，0，1，2重新打开；修改后，除非文件描述符拥有FD_CLOEXEC标记，否则该描述符不会被关闭。



