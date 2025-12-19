# Gitlite :平面化结构mini git系统

## 1. 核心类设计 (Class Design)

### 1.1 对象模型 (GitLiteObject)
所有可持久化的数据单元均继承自 `GitLiteObject` 基类。

* **`GitLiteObject` (抽象基类)**
    * **作用**: 定义了所有 Git 对象的通用接口，包括序列化 (`serialize`) 和反序列化 (`deserialize`)。
    * **关键变量**: `hashid` (对象的 SHA-1 哈希值)。

* **`Blob`**
    * **作用**: 代表文件内容的快照。Gitlite 保存的是文件的内容，而非文件名。
    * **关键变量**: `content` (文件的原始内容字符串)。

* **`Commit`**
    * **作用**: 代表项目在特定时间点的快照。
    * **成员**:
        * `Commit_Metadata`。
        * `Father_Commit`: `vector<string>`，存储父提交的哈希值
        * `Blobs`: `map<string, string>`，映射 **文件路径** -> **Blob 哈希**。

### 1.2 存储与状态管理

* **`ObjectDatabase`**
    * **作用**: 负责对象的持久化存储和读取。
    * **工作原理**: 采用40-hash寻址存储。（前2位作为目录，后38位作为文件名）。
    * **关键方法**: `writeObject` (写入并返回哈希), `readObject` (根据哈希读取), `copyToRemote`/`copyObjectFromRemote` (负责对象在本地与远程间的物理传输)。

* **`RemoteObjectDatabase`**
  *  与OBJdatabase基本相同

* **`index` (暂存区)**
    * **关键变量**:
        * `entries`: `map<string, string>`，记录 `add` 的文件及其对应的 Blob 哈希。
        * `removed_entries`: `vector<string>`，记录计划被 `rm` 删除的文件。

### 1.3 引用与远程管理

* **`RefManager`**
    * **作用**: 管理分支（Branch）和 HEAD 指针。
    * **工作原理**: 分支本质上只是指向某个 Commit 哈希的文本文件。`HEAD` 是一个指向当前活跃分支或特定 Commit 的指针。
    * **关键功能**: 解析 `HEAD` (resolveHead)，切换分支，更新引用。

* **`RemoteManager`**
    * **作用**: 维护远程仓库的别名映射.
    * **持久化**: 读写 `.gitlite/remotes` 文件。

### 1.4 command实现

* **`Repository`**
    * **作用**: 系统的外观类，执行用户命令（`init`, `add`, `commit`, `merge`, `push` 等）。

---

## 2. 核心算法与工作原理

### 2.1 三向合并 (Three-Way Merge)
`merge` 操作不仅仅是简单的文件覆盖，而是基于** Split Point ** 的智能合并。

1.  **查找公共祖先**: 遍历 Commit 图，找到当前分支 (`Current`) 和目标分支 (`Given`) 的最近公共祖先 (`Split`)。
2.  **文件比对规则**:
    * **新增**: 仅在 Current 或 Given 中存在的文件，将被保留。
    * **修改**:
        * 若 Current 修改（与 Split 不同）且 Given 未修改：**保留 Current**。
        * 若 Given 修改且 Current 未修改：**接受 Given（自动暂存）**。
        * 若两者都修改且内容不同：**冲突 (Conflict)**。
    * **删除**:
        * 若 Split 中存在，Current 未改但 Given 删了：**删除**。
        * 若 Split 中存在，Current 删了但 Given 未改：**保持删除**。
3.  **冲突处理**: 当发生冲突时，Gitlite 将文件内容重写为包含 `<<<<`, `====`, `>>>>` 的冲突格式，并将其放入暂存区等待用户解决。

### 2.2 远程同步 (Push & Pull)

* **Push (推送到远程)**:
    1.  **存在性检查**: 确认本地是否拥有远程 HEAD 的对象（防止 Crash）。
    2.  **Fast-Forward 检查**: 确保远程 HEAD 是本地 HEAD 的祖先。如果不是，拒绝推送并提示 Pull。
    3.  **对象传输**: 使用 `traverseAndCopy` 算法，通过 BFS 找出从远程 HEAD 到本地 HEAD 之间所有新增的 Commit 和 Blob，复制到远程 DB。
    4.  **引用更新**: 更新远程的 Branch 指针。

* **Pull (从远程拉取)**:
    1.  **Fetch**: 将远程的 Objects 复制到本地，并更新本地的 `refs/remotes/` 引用。
    2.  **Merge**: 自动调用 `merge` 将远程跟踪分支合并到当前本地分支。

### 2.3 分支切换 (Checkout)
`checkout` 的核心不仅是切换 HEAD 指针，还需要安全地更新工作目录：
1.  **Untracked File Check**: 在覆盖文件前，检查工作目录是否有未被 Gitlite 跟踪的文件会被覆盖。如果有，中止操作以防数据丢失。
2.  **重置暂存区**: 切换分支后，暂存区会被清空，以匹配新的 Commit 状态。

---

## 3. 持久化实现 (Persistence)

Gitlite 的状态完全保存在项目根目录下的 `.gitlite` 隐藏文件夹中。

### 3.1 目录结构
```text
.gitlite/
├── HEAD              # 文本文件，记录当前分支引用 (如 ref: refs/heads/master)
├── index             # 二进制或文本文件，序列化的暂存区状态
├── remotes           # 文本文件，存储远程仓库别名映射
├── objects/          # 对象数据库 (Object Database)
│   ├── ab/           # 哈希前两位作为文件夹名
│   │   └── 1234...   # 哈希后38位作为文件名 (存储序列化后的对象)
│   └── ...
└── refs/             # 引用管理
    ├── heads/        # 本地分支
    │   ├── master    # 文件内容为 Commit Hash
    │   └── other
    └── remotes/      # 远程跟踪分支
        └── origin/
            └── master
 ```
### 3.2 序列化方式 (Serialization)
我们为不同类型的对象设计了专门的序列化协议，以确保数据在磁盘与内存间转换的准确性：

* **Commit 序列化**: 
    采用基于行和特定分隔符的格式。序列化后的数据流包含：
    1.  **类型标识**: 明确标记为 Commit 类型。
    2.  **元数据**: 包含提交信息（Message）和 ISO 格式的时间戳。
    3.  **父节点**: 逐行记录所有父提交的 SHA-1 哈希值（用于支持 Merge 产生的多父节点）。
    4.  **文件映射**: 以 `路径:哈希` 的形式记录该 Commit 跟踪的所有文件快照。
* **Blob 序列化**: 
    为了保证哈希计算的纯粹性，Blob 对象直接存储文件的原始二进制/文本数据，不添加任何额外的包裹字符。
* **Index (暂存区) 序列化**: 
    `index` 文件以纯文本形式存储：
    * 第一部分记录 `entries`（路径与哈希的对应关系）。
    * 第二部分记录 `removed_entries`（计划在下次提交中删除的文件路径列表）。

### 3.3 状态记录机制 (State Management)
Gitlite 通过精确的流程控制来记录和转换程序状态：

* **文件添加 (`add`)**: 
    系统计算目标文件的 SHA-1 值，若内容发生变化，则在 `objects/` 目录下生成新的 Blob 文件，并更新 `index` 文件中的条目。
* **分支与提交**: 
    * **Commit**: 提交操作会将当前 `index` 中的快照持久化为 `Commit` 对象。
    * **引用更新**: 成功提交后，系统会修改 `refs/heads/[当前分支名]` 文件的内容，将其指向最新的 Commit 哈希，实现分支的增长。
* **删除操作 (`rm`)**: 
    执行 `rm` 时，系统不仅会从工作目录删除文件（如果已被跟踪），还会在 `index` 的 `removed_entries` 中添加记录。在下一次提交时，这些路径将不会出现在新 Commit 的 `Blobs` 映射中。
* **HEAD 状态切换**: 
    * **常规状态**: `HEAD` 文件内容为 `ref: refs/heads/[分支名]`。
    * **分离头指针 (Detached HEAD)**: `HEAD` 文件内容直接改为 40 位 Commit 哈希。