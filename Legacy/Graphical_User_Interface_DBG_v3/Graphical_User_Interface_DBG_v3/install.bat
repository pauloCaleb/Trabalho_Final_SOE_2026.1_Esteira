@echo off
REM =============================================================================
REM  install.bat — Esteira Separadora · GUI de Controle UART
REM  Windows 10 / 11 (para testes com adaptador USB-Serial)
REM =============================================================================

setlocal enabledelayedexpansion
title Esteira Separadora - Instalacao da GUI

set "VENV_DIR=%~dp0venv"
set "PYTHON_EXE=%~dp0venv\Scripts\python.exe"
set "PIP_EXE=%~dp0venv\Scripts\pip.exe"

echo.
echo ============================================================
echo   ESTEIRA SEPARADORA -- Instalacao da GUI
echo   STM32G070 ^<-^> Raspberry Pi 3B ^| SOE 2026.1
echo ============================================================
echo.

REM ── Passo 1: Verificar Python 3 ──────────────────────────────────────────────
echo [1/4] Verificando Python 3...
python --version >nul 2>&1
if %errorlevel% neq 0 goto erro_python
for /f "tokens=*" %%v in ('python --version 2^>^&1') do set PY_VER=%%v
echo   OK: %PY_VER% encontrado.

REM ── Passo 2: Criar venv ───────────────────────────────────────────────────────
echo.
echo [2/4] Criando ambiente virtual em: %VENV_DIR%
if exist "%VENV_DIR%" (
    echo   Aviso: pasta venv ja existe. Recriando...
    rmdir /s /q "%VENV_DIR%"
)
python -m venv "%VENV_DIR%" --without-pip
if %errorlevel% neq 0 goto erro_venv
echo   OK: Ambiente virtual criado.

REM ── Passo 3: Instalar pip via ensurepip (local, sem rede) ────────────────────
echo.
echo [3/4] Instalando pip no venv via ensurepip...
"%PYTHON_EXE%" -m ensurepip --upgrade >nul 2>&1
if %errorlevel% neq 0 goto erro_ensurepip
"%PYTHON_EXE%" -m pip install --quiet --upgrade pip
if %errorlevel% neq 0 goto erro_pip
echo   OK: pip instalado e atualizado.

REM ── Passo 4: Instalar pyserial ────────────────────────────────────────────────
echo.
echo [4/4] Instalando pyserial...
"%PIP_EXE%" install pyserial
if %errorlevel% neq 0 goto erro_pyserial
for /f "tokens=2" %%v in ('"%PIP_EXE%" show pyserial ^| findstr Version') do set SERIAL_VER=%%v
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
goto fim

REM ── Tratamento de erros ───────────────────────────────────────────────────────
:erro_python
echo.
echo   ERRO: Python 3 nao encontrado no PATH.
echo   Instale em: https://www.python.org/downloads/
echo   Marque "Add Python to PATH" durante a instalacao.
echo.
pause
exit /b 1

:erro_venv
echo.
echo   ERRO: Falha ao criar o ambiente virtual.
echo   Verifique se o modulo venv esta disponivel para sua versao do Python.
echo.
pause
exit /b 1

:erro_ensurepip
echo.
echo   ERRO: Falha ao instalar pip via ensurepip.
echo   Verifique se o Python esta completo (nao e uma instalacao minima).
echo.
pause
exit /b 1

:erro_pip
echo.
echo   ERRO: Falha ao atualizar pip.
echo.
pause
exit /b 1

:erro_pyserial
echo.
echo   ERRO: Falha ao instalar pyserial.
echo   Verifique sua conexao com a internet e tente novamente.
echo.
pause
exit /b 1

:fim
endlocal