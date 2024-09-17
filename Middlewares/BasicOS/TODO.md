# BasicOS TODO
----
1. 在port.s中的拷贝功能，要考虑拷贝中的覆盖问题。现在直接用的从低到高拷贝，这种拷贝是不安全的。
2. 目前的问题描述如下：
当任务切换时，SP指针的值，要在汇编语言实现赋值。还要根据方向进行赋值。这部分内容没有调试清楚。当延时任务，返回到ready状态时，SP指针的计算，不正确。

-------

1. 根据SP寄存器的值，计算move_size和copy_size
1. 根据SP寄存器的值，计算addr_target和addr_source
1. 根据move_size和copy_size计算当前任务的SP、STACK和stack_size。
1. 修改所有中间任务的SP和STACK
1. 根据move_size和copy_size计算下一任务的SP、STACK和stack_size。

PendSV中的伪代码
1. 将当前任务寄存器入栈
1. 拷贝数据
1. 将当前任务寄存器出栈