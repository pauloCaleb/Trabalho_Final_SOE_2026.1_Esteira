@echo off
REM =============================================================================
REM install.bat — Configuração do ambiente virtual e instalação de dependências
REM Projeto: Gerador de QR Codes - SOE 2026.1
REM Autores: Felipe e Caleb
REM =============================================================================

setlocal EnableDelayedExpansion

set VENV_DIR=.venv

echo.
echo [INFO]  Verificando Python...
python --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERRO]  Python nao encontrado. Instale o Python ^>= 3.10 e adicione ao PATH.
    pause
    exit /b 1
)

for /f "tokens=2 delims= " %%v in ('python --version 2^>^&1') do set PYVER=%%v
echo [OK]    Python %PYVER% detectado.

REM ── Ambiente virtual ────────────────────────────────────────────────────────
if exist "%VENV_DIR%\" (
    echo [AVISO] Ambiente virtual '%VENV_DIR%' ja existe. Pulando criacao.
) else (
    echo [INFO]  Criando ambiente virtual em '%VENV_DIR%'...
    python -m venv %VENV_DIR%
    echo [OK]    Ambiente virtual criado.
)

REM ── Ativação e instalação ────────────────────────────────────────────────────
echo [INFO]  Ativando ambiente e instalando dependencias...
call %VENV_DIR%\Scripts\activate.bat

python -m pip install --upgrade pip --quiet
pip install -r requirements.txt

echo.
echo ============================================================
echo   Instalacao concluida com sucesso!
echo ============================================================
echo.
echo   Para ativar o ambiente virtual manualmente:
echo     %VENV_DIR%\Scripts\activate
echo.
echo   Para gerar os QR codes com o arquivo padrao (pecas.json):
echo     python gerar_qrcodes.py
echo.
echo   Para usar um JSON personalizado:
echo     python gerar_qrcodes.py meu_arquivo.json -o minha_pasta\
echo.
pause
