@echo off
:: Force UTF-8 encoding
chcp 65001 >nul
setlocal

:: =================================================================
:: Configuration Area
:: =================================================================

:: 1. Docker Image
set DOCKER_IMAGE=my-esp-npm-env:v5.5

:: 2. Web Partition Size (2MB)
set PARTITION_SIZE=0x200000

:: 3. Build Output Directory
set DIST_DIR=dist

:: 4. Base Filename Prefix (Final name will be prefix_version.bin)
set BIN_PREFIX=www

:: 5. Filename Length Limit (Matches sdkconfig)
set OBJ_NAME_LEN=64

:: =================================================================

echo [1/2] Starting Docker environment...
echo       Target Directory: %DIST_DIR%
echo.

:: -----------------------------------------------------------------
:: Core Optimization Logic with Versioning:
:: 1. Install dependencies (skip if exists).
:: 2. Build frontend (generates dist/version.txt).
:: 3. EXTRACT VERSION: Read dist/version.txt directly.
:: 4. Pack Bin with dynamic filename: www_[version].bin
:: -----------------------------------------------------------------

set "CMD_SCRIPT=. /opt/esp/idf/export.sh && "
set "CMD_SCRIPT=%CMD_SCRIPT% if [ ! -d 'node_modules' ]; then echo '--- Step 1: Installing dependencies... ---'; npm install; else echo '--- Step 1: Dependencies exist, skipping install ---'; fi && "
set "CMD_SCRIPT=%CMD_SCRIPT% echo '--- Step 2: Building frontend... ---' && npm run build && "
set "CMD_SCRIPT=%CMD_SCRIPT% echo '--- Step 3: Detecting version... ---' && "
:: Try to read version from src/util/const.ts
set "CMD_SCRIPT=%CMD_SCRIPT% VER='latest'; "
set "CMD_SCRIPT=%CMD_SCRIPT% if [ -f 'src/util/const.ts' ]; then "
set "CMD_SCRIPT=%CMD_SCRIPT%   VER=$(grep 'const WEB_VERSION =' src/util/const.ts | cut -d\"'\" -f2 | tr -d '\r' | tr ' ' '_'); "
set "CMD_SCRIPT=%CMD_SCRIPT%   echo 'Found version from const.ts: '$VER; "
set "CMD_SCRIPT=%CMD_SCRIPT% else "
set "CMD_SCRIPT=%CMD_SCRIPT%   echo 'Warning: src/util/const.ts not found, using default.'; "
set "CMD_SCRIPT=%CMD_SCRIPT% fi; "
set "CMD_SCRIPT=%CMD_SCRIPT% OUTPUT_NAME='%BIN_PREFIX%_'$VER'.bin'; "
set "CMD_SCRIPT=%CMD_SCRIPT% echo 'Target Firmware Name: '$OUTPUT_NAME && "
set "CMD_SCRIPT=%CMD_SCRIPT% echo '--- Step 4: Packing Bin... ---' && python $IDF_PATH/components/spiffs/spiffsgen.py %PARTITION_SIZE% %DIST_DIR% $OUTPUT_NAME --obj-name-len %OBJ_NAME_LEN%"

docker run --rm -v "%CD%:/project" -w /project %DOCKER_IMAGE% /bin/bash -c "%CMD_SCRIPT%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo [BUILD SUCCESS]
    echo Firmware generated in current directory:
    dir /b %BIN_PREFIX%_*.bin
    echo ========================================================
) else (
    echo.
    echo [BUILD FAILED] Please check logs above.
)

pause