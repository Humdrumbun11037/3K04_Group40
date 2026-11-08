@echo off
rem Sets the simulated drawer hardware of the integrated ADC (adc_hms_link):
rem   drawer stuck    the next drawer opened stays open (30 s later: TIMEOUT_FAULT)
rem   drawer jam      the next drawer fails to open
rem   drawer normal   back to normal (frees a stuck drawer)
rem Read by integration\adc_side\hw0_drawer_sim.cpp on every drawer call.
cd /d "%~dp0"
if /i "%1"=="normal" goto set
if /i "%1"=="stuck" goto set
if /i "%1"=="jam" goto set
echo Usage: drawer normal ^| stuck ^| jam
exit /b 1
:set
if not exist run mkdir run
>run\drawer_mode.txt echo %1
echo Drawer hardware mode: %1
