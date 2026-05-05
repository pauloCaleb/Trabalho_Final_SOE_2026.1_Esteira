@echo off
REM =============================================================================
REM  install.bat — Esteira Separadora · Sistema de Controle v3.0
REM  Windows 10 / 11 (para testes com adaptador USB-Serial)
REM
REM  O que este script faz:
REM    1. Verifica se Python 3 esta disponivel no PATH
REM    2. Cria o ambiente virtual Python em .\venv
REM    3. Atualiza o pip dentro do venv
REM    4. Instala pyserial, opencv-python e Pillow dentro do venv
REM    5. Confirma a instalacao
REM
REM  Uso:
REM    Duplo clique em install.bat
REM    ou execute em um terminal: install.bat
REM =============================================================================

setlocal enabledelayedexpansion
title Esteira Separadora - Instalacao v3.0

echo.
echo ============================================================
echo   ESTEIRA SEPARADORA -- Instalacao do Sistema de Controle
echo   STM32G070 ^<-^> Raspberry Pi 3B ^| SOE 2026.1  ^| v3.0
echo ============================================================
echo.

REM ── Passo 1: Verificar Python 3 ──────────────────────────────────────────────
echo [1/5] Verificando Python 3...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo   ERRO: Python 3 nao encontrado no PATH.
    echo.
    echo   Instale o Python 3 em: https://www.python.org/downloads/
    echo   Durante a instalacao, marque a opcao "Add Python to PATH".
    echo.
    pause
    exit /b 1
)
for /f "tokens=*" %%v in ('python --version 2^>^&1') do set PY_VER=%%v
echo   OK: %PY_VER% encontrado.

REM ── Passo 2: Criar venv ───────────────────────────────────────────────────────
echo.
echo [2/5] Criando ambiente virtual em: %~dp0venv
if exist "%~dp0venv" (
    echo   Aviso: pasta venv ja existe. Recriando...
    rmdir /s /q "%~dp0venv"
)
python -m venv "%~dp0venv"
if %errorlevel% neq 0 (
    echo.
    echo   ERRO: Falha ao criar o ambiente virtual.
    echo   Verifique se o modulo venv esta disponivel para sua versao do Python.
    pause
    exit /b 1
)
echo   OK: Ambiente virtual criado.

REM ── Passo 3: Atualizar pip ────────────────────────────────────────────────────
echo.
echo [3/5] Atualizando pip dentro do venv...
"%~dp0venv\Scripts\python.exe" -m pip install --quiet --upgrade pip
echo   OK: pip atualizado.

REM ── Passo 4: Instalar dependencias ───────────────────────────────────────────
echo.
echo [4/5] Instalando dependencias (pyserial, opencv-python, Pillow)...
echo   Isso pode levar alguns minutos na primeira vez...
echo.

"%~dp0venv\Scripts\pip.exe" install pyserial
if %errorlevel% neq 0 (
    echo.
    echo   ERRO: Falha ao instalar pyserial.
    pause
    exit /b 1
)
for /f "tokens=2" %%v in ('"%~dp0venv\Scripts\pip.exe" show pyserial ^| findstr Version') do set SERIAL_VER=%%v
echo   OK: pyserial %SERIAL_VER% instalado.

"%~dp0venv\Scripts\pip.exe" install opencv-python
if %errorlevel% neq 0 (
    echo.
    echo   AVISO: Falha ao instalar opencv-python.
    echo   O leitor de QR Code nao estara disponivel.
    echo   A GUI funcionara normalmente apenas sem a camera.
    echo.
) else (
    for /f "tokens=2" %%v in ('"%~dp0venv\Scripts\pip.exe" show opencv-python ^| findstr Version') do set CV2_VER=%%v
    echo   OK: opencv-python %CV2_VER% instalado.
)

"%~dp0venv\Scripts\pip.exe" install Pillow
if %errorlevel% neq 0 (
    echo.
    echo   AVISO: Falha ao instalar Pillow.
    echo   O preview da camera nao estara disponivel.
    echo.
) else (
    for /f "tokens=2" %%v in ('"%~dp0venv\Scripts\pip.exe" show Pillow ^| findstr Version') do set PIL_VER=%%v
    echo   OK: Pillow %PIL_VER% instalado.
)

REM ── Passo 5: Verificar arquivo principal ─────────────────────────────────────
echo.
echo [5/5] Verificando arquivo principal...
if exist "%~dp0esteira_control_v3.py" (
    echo   OK: esteira_control_v3.py encontrado.
) else (
    echo   AVISO: esteira_control_v3.py nao encontrado nesta pasta.
    echo   Certifique-se de que o arquivo esta no mesmo diretorio que este script.
)

REM ── Resumo ────────────────────────────────────────────────────────────────────
echo.
echo ============================================================
echo   Instalacao concluida!
echo ============================================================
echo.
echo   Para executar o sistema, abra um terminal nesta pasta:
echo.
echo     venv\Scripts\activate
echo     python esteira_control_v3.py
echo.
echo   Ou diretamente sem ativar o venv:
echo.
echo     venv\Scripts\python.exe esteira_control_v3.py
echo.
echo   NOTAS:
echo     - A porta serial aparece como COM3, COM4, etc.
echo       Selecione a porta correta no combobox da GUI.
echo     - A camera USB aparece como indice 0 (padrao).
echo       Se houver multiplas cameras, teste indices 1, 2...
echo     - A implantacao definitiva e no Raspberry Pi (Linux).
echo.
pause