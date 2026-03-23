
# EdgeAIquant_toolkit 
## 边缘端 AI 模型量化工具链 (全链路工程化脚手架)

本仓库是一个针对边缘计算场景设计的 C++ 量化工具，旨在展示从底层算法开发、CMake 自动化构建到 Docker 容器化交付的完整工业级流程。

## 🌟 核心特性
- 工程化构建：使用 CMake 配合 Presets 机制，支持 Debug/Release 多模式构建。
- 环境隔离：提供基于 Docker 的开发环境，支持 GDB 远程调试及权限对齐，解决 UID 冲突。
- 极致瘦身：采用 Docker 多阶段构建 (Multi-stage Build)，镜像体积从 800MB+ 缩减至约 120MB。
- 硬件适配：预留硬件加速接口。

## 🛠️ 快速上手
### 1. 环境准备 (Docker)
```bash
# 进入开发容器 (推荐)
./docker_run.sh  # 脚本已封装 -u $(id -u) 权限处理
2. 项目构建
bash
运行
./build.sh debug    # 构建调试版本
./build.sh release  # 构建生产版本
3. 运行量化工具
bash
运行
./output/bin/edgequant_tool --size 20
📦 部署与交付
本项目支持离线镜像交付。通过以下命令导入已打包的镜像：
bash
运行
docker load -i edgequant_delivery_v1.tar
docker run --rm -v $(pwd):/output/data edgequant:prod --size 15
📂 项目架构
src/: 量化核心算法与硬件适配层。
third_party/: 包含 CLI11 等第三方高效解析库及硬件 Mock。
Dockerfile: 生产级多阶段镜像配置。
