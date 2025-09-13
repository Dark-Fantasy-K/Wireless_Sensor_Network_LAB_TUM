#!/bin/bash

set -e

# Step 1: 查找所有 ttyACM 设备
#ports=($(ls /dev/ttyACM* 2>/dev/null))

#if [ ${#ports[@]} -eq 0 ]; then
#  echo "未找到任何 /dev/ttyACM 设备！"
#  exit 1
#fi
ports=(/dev/ttyACM2 /dev/ttyACM0)
echo "检测到端口: ${ports[@]}"

# Step 2: 对每个端口执行 nrfjprog --recover（不直接用端口名，需要用serial number或自动映射）
for port in "${ports[@]}"; do
  echo "执行 nrfjprog --recover on $port"
  nrfjprog --recover || echo "警告: $port 上 recover 失败，继续执行..."
done

# Step 3: 构造 PORT 参数
# 例如 ttyACM0 ttyACM1 ttyACM2 => tty\ACM0-1-2
port_names=()
for port in "${ports[@]}"; do
  basename=$(basename "$port")         # eg: ttyACM0
  suffix=${basename#"ttyACM"}          # 提取数字 eg: 0
  port_names+=("$suffix")
done

joined_ports=$(IFS=- ; echo "${port_names[*]}")
port_arg="tty\\ACM${joined_ports}"

echo "上传中: make TARGET=nrf52840 simple_packet.upload PORT=$port_arg"
make TARGET=nrf52840 simple_packet.upload PORT="$port_arg"
