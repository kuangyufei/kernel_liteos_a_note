- liteos_m易用性
	- [ ] liteos_m开发者手册
	- [ ] 低功耗框架及实现（含投票机制）
	- [ ] dump增强（配套解析工具）
	- [ ] 全内存dump（配套解析工具）
	- [ ] 实现完整L0 OHOS的qemu仿真
	- [ ] Shell + AT

- liteos_m能力增强
	- [ ] liteos_m支持elf动态加载
	- [ ] 基于MPU/PMP的多任务简化隔离
	- [ ] 基于MPU/PMP的多BIN隔离及灌段隔离
	- [ ] 扩展支持中断嵌套
	- [ ] 支持arm9架构
	- [ ] 支持xtensa架构

- 文件系统增强
	- [ ] 提供一种好用且开源的NandFlash文件系统
	- [ ] 面向fat32、jffs2持续性能优化，做到极致
	- [ ] fat32支持fast seek（现有fast seek限制文件扩展，需要改造）
	- [ ] 支持软链接
	- [ ] 头文件引用关系整理
	- [ ] 接口层去nuttx
	- [ ] 文件系统去大锁

- liteos_a支持三方芯片易移植性
	- [ ] 启动框架重构
	- [ ] musl库归一化
	- [ ] 去C库预编译
	- [ ] 宏配置依赖关系整理
	- [ ] 典型商用配置场景整理并导入门禁（1V1映射，去缺页，去隔离，单进程等等）
	- [ ] 编译框架整合到gn
	- [ ] 实现基于python的kconfig可视化配置

- liteos_a易用性
	- [ ] 实现L1 LiteOS-A及上层鸿蒙组件的qemu仿真
	- [ ] liteos_a开发者手册
	- [ ] mksh移植
	- [ ] toybox命令集
	- [ ] trace、backtrace、dump解析工具等
	- procfs适配
		- [ ] ifconfig、fd、free等

- liteos_a能力增强
	- [ ] 单链表整改（SMP多核性能）
	- [ ] 快启
	- [ ] 典型高频函数C库性能优化
	- [ ] rwlock

- liteos_a三方库移植
	- [ ] libuv、dlna、benchmark、iperf、perf、tcpdump 等等
	-  C库能力补全
		- [ ] epoll实现 

- 测试验证
	- [ ] syzkaller、difuze等

- 探索性课题
	- [ ] 基于rust重写liteos_m基础内核
	- [ ] 用户态驱动（对比业界并增强）
	- [ ] 用户态文件系统