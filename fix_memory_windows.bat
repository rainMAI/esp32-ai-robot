@echo off
echo === Windows 内存优化修复指南 ===
echo.
echo 当前问题：build 目录不存在，但有权限错误
echo.
echo 解决步骤：
echo.
echo 1. 关闭所有可能占用文件的程序：
echo    - 文件资源管理器（如果打开了 D:\code\eyes 目录）
echo    - 杀毒软件（临时禁用）
echo    - VSCode 或其他 IDE
echo.
echo 2. 然后执行以下命令：
echo.
echo    cd D:\code\eyes
echo    idf.py build
echo.
echo 3. 如果还是报错，重启电脑后再试
echo.
echo 4. 烧录：
echo    idf.py flash monitor
echo.
pause
