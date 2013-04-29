@echo off

echo.
echo === Building QW ===
qcc -src qw

echo.
echo.
echo === Building QWSP ===
qcc -src qwsp
