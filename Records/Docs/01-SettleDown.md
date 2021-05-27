# 2021-01-13 Settle Down

> 竞赛官网：https://os.educg.net/
>
> 功能赛道项目选择：https://github.com/oscomp

[TOC]



## 题目选择

**[proj32-NorFlash-FileSystem-on-SylixOS](https://github.com/oscomp/proj32-NorFlash-FileSystem-on-SylixOS)**



## 为什么？

1. proj32是翼辉公司发起的，完成本项目可能对日后的工作有帮助；
2. 由于本学期实验涉及到fs，这使得我们有坚实的基础来完成本项目；
3. 夏老师对存储方面有较深的研究，可以起到很好地指导作用；
4. 在完成基本任务后，我们可以着眼于fs的优化、安全工作，这对于我们学习、理解fs将是一次不错的机会。



## 需要做什么工作？

1. 研究支持Norflash的基本文件系统；

2. 研究spiffs算法；

3. 研究jffs2算法；

4. 研究磨损平衡算法；

5. 设计文件系统架构；

6. 攥写清晰的、可操作的、可复现、具有指导意义的文档；

   

## 导师的答复

> 同学，你好。
>
> 很高兴能有机会一起学习。在项目开始前，我建议可以先了解一下SylixOS，你可以在官网申请一个体验版开发套件，开发套件中包括了IDE、SylixOS源码（里面有SylixOS当前支持的几个文件系统代码可供参考）、模拟器、应用开发手册、驱动开发手册。
>
> 此外，需要去了解一下linux或其它系统下的一些norflash文件系统的实现。选题后我们公司会有专门的同事组织基础方面的**培训**，各个课题也会建沟通群。有问题可以随时可以沟通。



## 准备开工啦！

1. 申请SylixOS开发套件；

   > [SylixOS 开发套件下载](https://sylixos.com/)

2. [了解NorFlash文件系统在**其他系统**下的实现（这里是Linux源码阅读器）;](https://elixir.bootlin.com/linux/v5.11-rc3/source/kernel)

3. spiffs、jffs2、磨损平衡算法；



## 补充

> 老师们给了一些Hints供我们参考，可能后续工作可以围绕这部分开展

1. 文件系统的基本结构是？各子模块的功能分别是？
2. NOR Flash存储介质的特点是？
3. 目前有哪些面向NOR Flash的FS？试阅读相关的原始文献，列举并对比分析各自的优缺点；
4. 考虑嵌入式系统的特点和NOR Flash的特性，所设计的文件系统与常用的桌面级FS（如NTFS、EXT4等）相比，存在哪些差异？针对这些差异，如何进行适应性的优化？
5. FS有哪些性能指标？一般可从哪些方面优化FS的性能？
6. 为什么需要写平衡？有哪些常用的写平衡算法？
7. 实现掉电安全的常用方法有哪些？TPSFS是如何实现掉电安全的？
