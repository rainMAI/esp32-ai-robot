@echo off
echo ========================================
echo Fixing PSRAM DMA Pool Configuration
echo ========================================
echo.
echo Problem: sdkconfig has CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536 (64KB)
echo         This overrides sdkconfig.defaults which has 32768 (32KB)
echo         The extra 32KB reduces internal SRAM, causing errno=12 errors
echo.
echo Solution: Delete sdkconfig and rebuild to regenerate with correct settings
echo.
echo Step 1: Backing up sdkconfig...
if exist sdkconfig (
    copy /Y sdkconfig sdkconfig.backup
    echo Backed up sdkconfig to sdkconfig.backup
) else (
    echo No sdkconfig file to backup
)
echo.
echo Step 2: Deleting sdkconfig and sdkconfig.old...
if exist sdkconfig del /F sdkconfig
if exist sdkconfig.old del /F sdkconfig.old
echo Deleted.
echo.
echo Step 3: Rebuilding project...
echo This will regenerate sdkconfig with correct settings from sdkconfig.defaults
echo.
call idf.py build
echo.
echo ========================================
echo Build complete!
echo ========================================
echo.
echo Please check the new sdkconfig was generated with:
echo   CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
echo.
echo You can verify with: grep CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL sdkconfig
echo.
pause
