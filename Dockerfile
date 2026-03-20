# ==========================================
# 阶段 1：构建阶段 (Builder)
# 这个阶段很胖，包含所有编译工具，但最终会被丢弃
# ==========================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 1. 安装编译工具
RUN apt update && apt install -y \
    build-essential ninja-build wget gnupg ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 2. 安装 CMake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null && \
    apt update && apt install -y cmake

# 3. 拷贝源码到容器内 (这次不是挂载了，是硬拷贝)
WORKDIR /workspace
COPY . .

# 4. 执行 Release 模式编译 (给客户的版本不需要 debug 信息)
RUN ./build.sh release


# ==========================================
# 阶段 2：运行阶段 (Runtime / Production)
# 这个阶段极瘦，只包含最基础的运行环境
# ==========================================
FROM ubuntu:22.04 AS runtime

# 1. 只需要安装最基础的 C++ 运行时库，不需要编译器！
# 如果你的程序依赖了 OpenCV，也要在这里安装 OpenCV 运行库
RUN apt update && apt install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# 2. 核心魔法：从第一阶段 (builder) 把编译好的二进制文件“偷”过来！
WORKDIR /app
COPY --from=builder /workspace/output/bin/edgequant_tool /app/edgequant_tool

# 3. 设置默认执行命令
ENTRYPOINT ["/app/edgequant_tool"]
# 默认参数，允许用户在 docker run 时覆盖
CMD ["--size", "10"]