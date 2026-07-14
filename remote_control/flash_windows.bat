@echo off
:: ═══════════════════════════════════════════════════════════════
:: flash_windows.bat – Build và nạp code Remote Control ESP32
::                     Dùng trên Windows (CMD / ESP-IDF CMD)
::
:: Cách dùng:
::   flash_windows.bat          → nạp vào COM3 (mặc định)
::   flash_windows.bat COM5     → chỉ định cổng COM
::
:: Yêu cầu: ESP-IDF Windows Installer đã cài
::          Mở "ESP-IDF Command Prompt" (không dùng CMD thường)
:: ═══════════════════════════════════════════════════════════════

setlocal

set PORT=%1
if "%PORT%"=="" set PORT=COM3

echo ================================================
echo  Remote Control ESP32 – Build and Flash
echo ================================================
echo   Port  : %PORT%
echo   Chip  : esp32
echo   Baud  : 921600
echo.

echo [1/3] Set target esp32...
idf.py set-target esp32
if errorlevel 1 goto :error

echo [2/3] Build firmware...
idf.py build
if errorlevel 1 goto :error

echo [3/3] Flash to ESP32 (%PORT%)...
echo       Giu nut BOOT neu khong tu dong vao che do nap.
idf.py -p %PORT% -b 921600 flash monitor
if errorlevel 1 goto :error

goto :end

:error
echo.
echo [LOI] Build hoac Flash that bai. Kiem tra lai ket noi USB va IDF_PATH.
pause
exit /b 1

:end
endlocal
