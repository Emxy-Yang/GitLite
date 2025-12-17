### 项目设计： 
* 对象层 (Objects): 不可变的数据。包括 Blob (文件), Commit (提交)。
* 引用层 (Refs): 可变的指针。包括 Branch (分支), HEAD (当前指针)。
* 仓库层 (Repository): 管理上述两者的容器，以及暂存区 (Staging Area / Index)。