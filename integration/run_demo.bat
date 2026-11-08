@echo off
rem Starts the integrated D3 demo on this laptop: the HMS (hms_live) and the
rem real Subgroup A cabinet (adc_hms_link) in their own windows, then opens
rem the dashboard. Build first, from integration\: mingw32-make
cd /d "%~dp0"
if not exist build\hms_live.exe goto notbuilt
if not exist build\adc_hms_link.exe goto notbuilt
start "HMS - hms_live (type cut / restore / status)" build\hms_live.exe
rem The HMS creates the shared key on its first start; give it a moment.
timeout /t 3 /nobreak > nul
rem The cabinet window asks which registered ADC it is (demo selection).
start "ADC - Subgroup A cabinet (adc_hms_link)" build\adc_hms_link.exe
start "" http://127.0.0.1:8081
exit /b 0

:notbuilt
echo The demo programs are not built. In integration\ run: mingw32-make
pause
exit /b 1
