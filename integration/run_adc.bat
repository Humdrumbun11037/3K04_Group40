@echo off
rem Opens one more cabinet window (adc_hms_link): it asks which registered ADC
rem it is, so two windows can be two cabinets. hms_live must already be running
rem (run_demo.bat starts it). Build first, from integration\: mingw32-make
cd /d "%~dp0"
if not exist build\adc_hms_link.exe (
    echo The demo programs are not built. In integration\ run: mingw32-make
    pause
    exit /b 1
)
start "ADC - Subgroup A cabinet (adc_hms_link)" build\adc_hms_link.exe
