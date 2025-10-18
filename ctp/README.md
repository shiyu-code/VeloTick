# CTP 接口目录（预留）

此目录用于存放后续接入中金所/上期所等交易所的 CTP 接口相关文件与库。

目录结构建议：
- include/    放置 CTP 头文件（如 thostmduserapi.h、thosttraderapi.h 等）
- lib/        放置 CTP 库文件（Windows: *.dll/*.lib；Linux: *.so）
- notes/      记录接入说明、版本、依赖等

当前项目尚未链接 CTP 官方库，已在代码中预留 `CtpEngine` 桩类与 CMake 选项，后续可在启用选项后完成链接。

接入步骤简述：
1. 将官方提供的头文件拷贝到 `ctp/include/`。
2. 将官方库文件拷贝到 `ctp/lib/`。
3. 在 CMake 中打开 `VELO_USE_CTP=ON` 并配置正确的库名与路径。
4. 在 `config/velotick.toml` 中设置 `[ctp] use_ctp = true` 并填写连接参数。
5. 运行程序，验证订阅与行情推送。