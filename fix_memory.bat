@echo off
REM 内存不足问题修复脚本 - Windows 版本

echo === 修复内存不足问题 ===
echo.
echo 步骤 1: 删除旧的 sdkconfig 配置...
if exist sdkconfig del /F sdkconfig
if exist sdkconfig.old del /F sdkconfig.old
echo ✅ 已删除 sdkconfig
echo.
echo 步骤 2: 清理构建缓存...
if exist build rmdir /S /Q build
echo ✅ 已清理 build 目录
echo.
echo 步骤 3: 重新编译（将使用 sdkconfig.defaults 中的优化配置）
echo 请执行以下命令：
echo   idf.py build
echo.
echo 步骤 4: 烧录
echo 请执行以下命令：
echo   idf.py flash monitor
echo.
echo === 修复完成 ===
echo.
echo 预期效果：
echo - 可用内存从 35KB 增加到 80KB
echo - UDP 发送失败次数从 60+ 次减少到 0-5 次
echo.
pause
