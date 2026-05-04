@echo off
REM =============================================================================
REM  install.bat — Esteira Separadora · GUI de Controle UART
REM  Windows 10 / 11 (para testes com adaptador USB-Serial)
REM
REM  O que este script faz:
REM    1. Verifica se Python 3 esta disponivel no PATH
REM    2. Cria o ambiente virtual Python em .\venv
REM    3. Atualiza o pip dentro do venv
REM    4. Instala pyserial dentro do venv
REM    5. Confirma a instalacao
REM
REM  Uso:
REM    Duplo clique em install.bat
REM    ou execute em um terminal: install.bat
REM =============================================================================

setlocal enabledelayedexpansion
title Esteira Separadora - Instalacao da GUI

echo.
echo ============================================================
echo   ESTEIRA SEPARADORA -- Instalacao da GUI
echo   STM32G070 ^<-^> Raspberry Pi 3B ^| SOE 2026.1
echo ============================================================
echo.

REM ── Passo 1: Verificar Python 3 ──────────────────────────────────────────────
echo [1/4] Verificando Python 3...
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
echo [2/4] Criando ambiente virtual em: %~dp0venv
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
echo [3/4] Atualizando pip dentro do venv...
"%~dp0venv\Scripts\python.exe" -m pip install --quiet --upgrade pip
echo   OK: pip atualizado.

REM ── Passo 4: Instalar pyserial ────────────────────────────────────────────────
echo.
echo [4/4] Instalando pyserial...
"%~dp0venv\Scripts\pip.exe" install pyserial
if %errorlevel% neq 0 (
    echo.
    echo   ERRO: Falha ao instalar pyserial.
    echo   Verifique sua conexao com a internet e tente novamente.
    pause
    exit /b 1
)
for /f "tokens=2" %%v in ('"%~dp0venv\Scripts\pip.exe" show pyserial ^| findstr Version') do set SERIAL_VER=%%v
echo   OK: pyserial %SERIAL_VER% instalado.

REM ── Resumo ────────────────────────────────────────────────────────────────────
echo.
echo ============================================================
echo   Instalacao concluida com sucesso!
echo ============================================================
echo.
echo   Para executar a GUI, abra um terminal nesta pasta e rode:
echo.
echo     venv\Scripts\activate
echo     python esteira_control.py
echo.
echo   Ou execute diretamente sem ativar o venv:
echo.
echo     venv\Scripts\python.exe esteira_control.py
echo.
echo   NOTA: No Windows, a porta serial aparece como COM3, COM4,
echo   etc. Selecione a porta correta no combobox da GUI.
echo   A implantacao definitiva e no Raspberry Pi (Linux).
echo.
pause
