#!/usr/bin/env bash
# =============================================================================
#  install.sh — Esteira Separadora · Sistema de Controle v3.0
#  Raspberry Pi 3B / Linux (Raspbian Bookworm, Bullseye ou compatível)
#
#  O que este script faz:
#    1. Verifica se Python 3 está disponível
#    2. Instala python3-full via apt (necessário para criar venv no Bookworm)
#    3. Cria o ambiente virtual Python em ./venv
#    4. Instala pyserial, opencv-python e Pillow dentro do venv
#    5. Verifica dependências do sistema (tkinter, libcamera)
#    6. Configura permissões de porta serial e câmera
#    7. Confirma a instalação
#
#  Uso:
#    chmod +x install.sh
#    ./install.sh
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

echo ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║   ESTEIRA SEPARADORA — Instalação v3.0                   ║${NC}"
echo -e "${BOLD}${CYAN}║   STM32G070 ↔ Raspberry Pi 3B · SOE 2026.1              ║${NC}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ── Passo 1: Python 3 ─────────────────────────────────────────────────────────
echo -e "${BOLD}[1/7] Verificando Python 3...${NC}"
if ! command -v python3 &>/dev/null; then
    echo -e "${RED}  ✗ Python 3 não encontrado.${NC}"
    echo -e "    Instale com: ${YELLOW}sudo apt install python3${NC}"
    exit 1
fi
PY_VERSION=$(python3 --version 2>&1)
echo -e "${GREEN}  ✓ ${PY_VERSION} encontrado.${NC}"

# ── Passo 2: python3-full ─────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[2/7] Instalando python3-full (necessário para venv no Bookworm)...${NC}"
if sudo apt install -y python3-full 2>/dev/null; then
    echo -e "${GREEN}  ✓ python3-full instalado.${NC}"
else
    echo -e "${YELLOW}  ⚠ Não foi possível instalar via apt. Tentando prosseguir...${NC}"
fi

# ── Passo 3: Criar venv ───────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[3/7] Criando ambiente virtual em: ${VENV_DIR}${NC}"
if [ -d "$VENV_DIR" ]; then
    echo -e "${YELLOW}  ⚠ Pasta venv já existe. Recriando...${NC}"
    rm -rf "$VENV_DIR"
fi
python3 -m venv "$VENV_DIR"
echo -e "${GREEN}  ✓ Ambiente virtual criado.${NC}"

# Atualiza pip
"$VENV_DIR/bin/pip" install --quiet --upgrade pip

# ── Passo 4: Instalar dependências Python ─────────────────────────────────────
echo ""
echo -e "${BOLD}[4/7] Instalando dependências Python...${NC}"
echo -e "  Isso pode levar alguns minutos na primeira vez."
echo ""

# pyserial (obrigatório)
echo -e "  Instalando pyserial..."
"$VENV_DIR/bin/pip" install pyserial
SERIAL_VER=$("$VENV_DIR/bin/pip" show pyserial 2>/dev/null | grep Version | awk '{print $2}')
echo -e "${GREEN}  ✓ pyserial ${SERIAL_VER} instalado.${NC}"

# opencv-python (leitor de QR)
echo -e "  Instalando opencv-python (leitor de QR Code)..."
if "$VENV_DIR/bin/pip" install opencv-python 2>/dev/null; then
    CV2_VER=$("$VENV_DIR/bin/pip" show opencv-python 2>/dev/null | grep Version | awk '{print $2}')
    echo -e "${GREEN}  ✓ opencv-python ${CV2_VER} instalado.${NC}"
else
    echo -e "${YELLOW}  ⚠ Falha ao instalar opencv-python via pip.${NC}"
    echo -e "    Tentando versão headless (menor, recomendada para RPi)..."
    if "$VENV_DIR/bin/pip" install opencv-python-headless 2>/dev/null; then
        CV2_VER=$("$VENV_DIR/bin/pip" show opencv-python-headless 2>/dev/null | grep Version | awk '{print $2}')
        echo -e "${GREEN}  ✓ opencv-python-headless ${CV2_VER} instalado.${NC}"
    else
        echo -e "${RED}  ✗ Não foi possível instalar opencv. O leitor de QR não estará disponível.${NC}"
        echo -e "    Tente manualmente: ${YELLOW}sudo apt install python3-opencv${NC}"
    fi
fi

# Pillow (renderização do preview)
echo -e "  Instalando Pillow (preview da câmera)..."
if "$VENV_DIR/bin/pip" install Pillow 2>/dev/null; then
    PIL_VER=$("$VENV_DIR/bin/pip" show Pillow 2>/dev/null | grep Version | awk '{print $2}')
    echo -e "${GREEN}  ✓ Pillow ${PIL_VER} instalado.${NC}"
else
    echo -e "${RED}  ✗ Falha ao instalar Pillow. O preview da câmera não estará disponível.${NC}"
fi

# ── Passo 5: Verificar tkinter ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[5/7] Verificando tkinter (interface gráfica)...${NC}"
if python3 -c "import tkinter" 2>/dev/null; then
    echo -e "${GREEN}  ✓ tkinter disponível.${NC}"
else
    echo -e "${YELLOW}  ⚠ tkinter não encontrado. Instalando python3-tk...${NC}"
    sudo apt install -y python3-tk
    echo -e "${GREEN}  ✓ python3-tk instalado.${NC}"
fi

# ── Passo 6: Dependências de câmera ──────────────────────────────────────────
echo ""
echo -e "${BOLD}[6/7] Verificando suporte a câmera USB (Video4Linux)...${NC}"
if ls /dev/video* &>/dev/null; then
    echo -e "${GREEN}  ✓ Dispositivo(s) de câmera detectado(s):${NC}"
    ls /dev/video* 2>/dev/null | while read dev; do echo "    $dev"; done
else
    echo -e "${YELLOW}  ⚠ Nenhuma câmera USB detectada no momento.${NC}"
    echo -e "    Conecte a câmera e reinicie o script, ou conecte-a antes de iniciar a GUI."
fi

# Garante que o usuário está no grupo video (necessário para câmera no RPi)
if groups "$USER" | grep -qw video; then
    echo -e "${GREEN}  ✓ Usuário '${USER}' já pertence ao grupo 'video'.${NC}"
else
    echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo 'video' (acesso à câmera)...${NC}"
    sudo usermod -aG video "$USER"
    echo -e "${YELLOW}  ⚠ Faça logout e login novamente para o grupo ter efeito.${NC}"
fi

# ── Passo 7: Permissão porta serial ───────────────────────────────────────────
echo ""
echo -e "${BOLD}[7/7] Verificando permissão de acesso à porta serial (UART)...${NC}"
if groups "$USER" | grep -qw dialout; then
    echo -e "${GREEN}  ✓ Usuário '${USER}' já pertence ao grupo 'dialout'.${NC}"
else
    echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo 'dialout'...${NC}"
    sudo usermod -aG dialout "$USER"
    echo -e "${YELLOW}  ⚠ Faça logout e login novamente para a permissão ter efeito.${NC}"
fi

# ── Verificar arquivo principal ───────────────────────────────────────────────
echo ""
if [ -f "$SCRIPT_DIR/esteira_control_v3.py" ]; then
    echo -e "${GREEN}  ✓ esteira_control_v3.py encontrado.${NC}"
else
    echo -e "${YELLOW}  ⚠ esteira_control_v3.py não encontrado neste diretório.${NC}"
    echo -e "    Certifique-se de que o arquivo está na mesma pasta que este script."
fi

# ── Resumo ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${GREEN}║   ✓ Instalação concluída!                                ║${NC}"
echo -e "${BOLD}${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BOLD}Para executar o sistema:${NC}"
echo ""
echo -e "  ${CYAN}source venv/bin/activate${NC}"
echo -e "  ${CYAN}python3 esteira_control_v3.py${NC}"
echo ""
echo -e "Ou diretamente sem ativar o venv:"
echo ""
echo -e "  ${CYAN}venv/bin/python3 esteira_control_v3.py${NC}"
echo ""
echo -e "${YELLOW}Se grupos foram alterados (dialout / video), faça logout e login novamente.${NC}"
echo ""