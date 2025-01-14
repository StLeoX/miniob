## Miniob2022 后记

> 现在是2022-11-05 16:03:48，距离比赛结束还剩大约28h。目前我在排行榜上的score是270，rank是52。
> 
> 但是，我已经决定弃赛了，决定让score永远停留在270。
> 
> 我知道这有违Hacker永不言弃的精神，但是剩下的时间里我是真的卷不动了。
> 
> **我卷输了**。是的，我又一次卷输了。。。

 

### 心得向

    我一向如此看重结果。所以，当初只身参赛时，我就对自己说，“如果进不了前50，那么之前的努力相当于都白费了。”

    但是现在我却有些释然，假装对自己说，“没事的，相当于参加了一次高质量的Hackathon”、“因为个人的力量还是有些单薄”、“对数据库相关的理论以及技术实践不够熟练”。我不想把这些说法当做是参赛总结，也更不想把它们当做是弃赛的借口，因为我知道无论什么理由都不及一个胜利的结果那么振奋人心。

    最终我还是放手了，也像大多数人那样最终学会了放手。



### 技术向

    本人项目：

> ref: https://github.com/stleox/miniob

    主要参考了经验贴：

> ref：https://zhuanlan.zhihu.com/p/455956866
> 
> ref：https://zhuanlan.zhihu.com/p/445201899
> 
> > 或许这些大佬才是在数据库领域内有真才实学的吧。

    不想涉及太多项目细节，想从整体上谈一谈。

    首先，Server 和 Database(SQLParser + Executor + Storage)    是解耦的，所以不需要实现C/S相关的基础设施（乐）。最困难的部分，或者说大部分赛题涉及到的部分，就是为Executor添加新特性。类似于为Compiler编写Codegen模块，写这种多模式的中间层往往是体力活。重体力的部分存在于Where子句、SubSelect子句等。多模式在这里意味着多种模式入口，从PA的角度来看，就是这些部分转换为的数据流图的入度很高。

    但是，学过软件工程的我们都知道所谓“组合复杂性”的概念，也就是俗话说的“屎山”，或者说“代码坏味”。如果前期毫不顾忌地向Executor中插入一些缺少前瞻设计的坏代码（随地拉屎），包括但不限于：

- 过度封装的类、函数；

- 意义不明确或者特设的、调用次数少的子过程；

- 冗余的函数参数列表；

- 字段间强耦合的结构（不合理的初始化会导致coredump的那种）；

- 传递链路超级无敌长的error code（是的，请允许我再骂一遍Go的error chain设计哲学）；

- 拒绝C++14 +新特性的老式写法（这一点有待商榷，因为总有些人就是幻想定长的数组就是比vector性能要好、const char* 就是比 const std::string& 要好。我不否认，但我更倾向于可读性和可维护性。）；

- 等等吧

    后期的改动成本将会组合性地爆炸，然后在冲刺阶段就寄了。~~是的，我现在就是处于这种境地。~~



### 额外的一些意义不明的思考

- 参加比赛才知道，原来有这么多人卷在数据库内核赛道上。当然，这还不算那些能给数据库调优的高手。

- 跟编译相关的前端部分，写起来都是体力活。或许这些内容才是程序员的日常吧，而不是设计一两个与周围系统弱耦合的算法。

- 做题家与程序员几乎是二元对立的思维方式。
  
  >     做题家知道，问题的答案是唯一的，所以自己只需要努力找出来“正确答案”即可，目标很明确。
  > 
  >     但是在编程过程中，大到解决方案、小到函数签名的设计，都绝对不是唯一的，我们只是在努力寻找最优解，寻找BugFree的方案。这只能是一个不断逼近的过程，所以需要不断地修正目标，甚至会对阶段性的目标完全推倒从来。

- "What you see is what you made"的程序写起来才有刺激性，调试Bug本身是乏味的。至少对于我来说，喜欢Develop胜过Debug。所以是时候考虑放弃C++了 ~~转PHP！~~

- ~~下次有类似比赛还要继续参加，不过要拉上几个队友。~~






































