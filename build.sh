#!/bin/bash

# 创建 build 目录
mkdir -p build

# 进入 build 目录
cd build

# 清空 build 目录
rm -rf *

# 运行 cmake
cmake ..

# 编译项目
make
