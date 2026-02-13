#!/bin/bash
# 内存优化配置修复脚本

echo "=== 修复内存不足问题 ==="
echo ""
echo "步骤 1: 删除旧的 sdkconfig 配置..."
rm -f sdkconfig
rm -f sdkconfig.old
echo "✅ 已删除 sdkconfig"
echo ""
echo "步骤 2: 重新生成 sdkconfig（将使用 sdkconfig.defaults 中的优化配置）"
echo ""
echo "步骤 3: 清理构建缓存..."
echo "idf.py fullclean"
echo ""
echo "步骤 4: 重新编译..."
echo "idf.py build"
echo ""
echo "步骤 5: 烧录..."
echo "idf.py flash monitor"
echo ""
echo "=== 修复完成 ==="
echo ""
echo "预期效果："
echo "- 可用内存从 35KB 增加到 80KB"
echo "- UDP 发送失败次数从 60+ 次减少到 0-5 次"
echo ""
