#!/usr/bin/env bash
# =============================================================================
#  install.sh — Esteira Separadora · GUI de Controle UART
#  Raspberry Pi 3B / Linux (Raspbian Bookworm, Bullseye ou compatível)
#
#  O que este script faz:
#    1. Verifica se Python 3 está disponível
#    2. Instala python3-full via apt (necessário para criar venv no Bookworm)
#    3. Cria o ambiente virtual Python em ./venv
#    4. Instala pyserial dentro do venv
#    5. Verifica se python3-tk está presente (necessário para a GUI)
#    6. Confirma a instalação
#
#  Uso:
#    chmod +x install.sh
#    ./install.sh
# =============================================================================

set -e  # Para na primeira falha

# ── Cores para saída ──────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

echo ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║   ESTEIRA SEPARADORA — Instalação da GUI                 ║${NC}"
echo -e "${BOLD}${CYAN}║   STM32G070 ↔ Raspberry Pi 3B · SOE 2026.1              ║${NC}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ── Passo 1: Python 3 ─────────────────────────────────────────────────────────
echo -e "${BOLD}[1/5] Verificando Python 3...${NC}"
if ! command -v python3 &>/dev/null; then
    echo -e "${RED}  ✗ Python 3 não encontrado.${NC}"
    echo -e "    Instale com: ${YELLOW}sudo apt install python3${NC}"
    exit 1
fi
PY_VERSION=$(python3 --version 2>&1)
echo -e "${GREEN}  ✓ ${PY_VERSION} encontrado.${NC}"

# ── Passo 2: python3-full (venv) ──────────────────────────────────────────────
echo ""
echo -e "${BOLD}[2/5] Instalando python3-full (necessário para criar venv)...${NC}"
if sudo apt install -y python3-full 2>/dev/null; then
    echo -e "${GREEN}  ✓ python3-full instalado.${NC}"
else
    echo -e "${YELLOW}  ⚠ Não foi possível instalar via apt. Tentando prosseguir...${NC}"
fi

# ── Passo 3: Criar venv ───────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[3/5] Criando ambiente virtual em: ${VENV_DIR}${NC}"
if [ -d "$VENV_DIR" ]; then
    echo -e "${YELLOW}  ⚠ Pasta venv já existe. Recriando...${NC}"
    rm -rf "$VENV_DIR"
fi
python3 -m venv "$VENV_DIR"
echo -e "${GREEN}  ✓ Ambiente virtual criado.${NC}"

# ── Passo 4: Instalar pyserial ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[4/5] Instalando pyserial no ambiente virtual...${NC}"
"$VENV_DIR/bin/pip" install --quiet --upgrade pip
"$VENV_DIR/bin/pip" install pyserial
SERIAL_VERSION=$("$VENV_DIR/bin/pip" show pyserial 2>/dev/null | grep Version | awk '{print $2}')
echo -e "${GREEN}  ✓ pyserial ${SERIAL_VERSION} instalado.${NC}"

# ── Passo 5: Verificar tkinter ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[5/5] Verificando tkinter (necessário para a GUI)...${NC}"
if python3 -c "import tkinter" 2>/dev/null; then
    echo -e "${GREEN}  ✓ tkinter disponível.${NC}"
else
    echo -e "${YELLOW}  ⚠ tkinter não encontrado. Instalando python3-tk...${NC}"
    sudo apt install -y python3-tk
    echo -e "${GREEN}  ✓ python3-tk instalado.${NC}"
fi

# ── Verificar permissão na porta serial ───────────────────────────────────────
echo ""
echo -e "${BOLD}[+] Verificando permissão de acesso à porta serial...${NC}"
if groups "$USER" | grep -qw dialout; then
    echo -e "${GREEN}  ✓ Usuário '${USER}' já pertence ao grupo dialout.${NC}"
else
    echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo dialout...${NC}"
    sudo usermod -aG dialout "$USER"
    echo -e "${YELLOW}  ⚠ Faça logout e login novamente para a permissão ter efeito.${NC}"
fi

# ── Resumo ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${GREEN}║   ✓ Instalação concluída com sucesso!                    ║${NC}"
echo -e "${BOLD}${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BOLD}Para executar a GUI:${NC}"
echo ""
echo -e "  ${CYAN}source venv/bin/activate${NC}"
echo -e "  ${CYAN}python3 esteira_control.py${NC}"
echo ""
echo -e "Ou diretamente sem ativar o venv:"
echo ""
echo -e "  ${CYAN}venv/bin/python3 esteira_control.py${NC}"
echo ""
