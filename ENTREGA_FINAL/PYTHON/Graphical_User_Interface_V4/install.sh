#!/usr/bin/env bash
# =============================================================================
#  install.sh — Esteira Separadora · Sistema de Controle v4.0
#  Suporte: Raspberry Pi 3B (armhf) e Linux x86_64 (Ubuntu, Debian, etc.)
#
#  Estrategia por arquitetura:
#
#  armhf (RPi 3B):
#    - numpy via apt (python3-numpy), pois o numpy do PyPI para ARM32
#      requer libopenblas/libatlas e quebra sem elas
#    - opencv com --no-deps para nao sobrescrever o numpy do sistema
#    - Pillow com --ignore-installed para garantir versao nova no venv,
#      ignorando o Pillow 9.4.0 do sistema que nao tem ImageTk completo
#    - venv com --system-site-packages para enxergar o numpy do sistema
#    - libatlas-base-dev fornece libcblas.so.3, exigida pelo opencv no armhf
#
#  x86_64 (Linux comum):
#    - tudo via pip dentro de um venv isolado (sem --system-site-packages)
#    - numpy, opencv e Pillow instalam sem dependencias de sistema especiais
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
MAIN_FILE="esteira_control_v4.py"
PY="$VENV_DIR/bin/python3"
PIP="$VENV_DIR/bin/pip"
ARCH="$(uname -m)"

echo ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║   ESTEIRA SEPARADORA — Instalação v4.0                   ║${NC}"
echo -e "${BOLD}${CYAN}║   STM32G070 ↔ Raspberry Pi 3B · SOE 2026.1              ║${NC}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Arquitetura detectada: ${BOLD}${ARCH}${NC}"
echo ""

# ── Passo 1: Python 3 ─────────────────────────────────────────────────────────
echo -e "${BOLD}[1/7] Verificando Python 3...${NC}"
if ! command -v python3 &>/dev/null; then
    echo -e "${RED}  ✗ Python 3 não encontrado. Instale com: sudo apt install python3${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ $(python3 --version) encontrado.${NC}"

# ── Passo 2: Dependências de sistema ──────────────────────────────────────────
echo ""
echo -e "${BOLD}[2/7] Instalando dependências de sistema via apt...${NC}"

if [ "$ARCH" = "armv7l" ]; then
    # python3-numpy   → numpy compilado para armhf sem libopenblas
    # libatlas-base-dev → fornece libcblas.so.3, exigida pelo opencv no armhf
    # libgl1 + libglib2.0-0 → runtime do OpenCV
    sudo apt install -y \
        python3-full \
        python3-tk \
        python3-numpy \
        libgl1 \
        libglib2.0-0 \
        libatlas-base-dev
else
    # x86_64: apenas o mínimo — numpy e opencv vêm limpos via pip
    sudo apt install -y \
        python3-full \
        python3-tk \
        libgl1 \
        libglib2.0-0
fi

echo -e "${GREEN}  ✓ Dependências de sistema OK.${NC}"

# ── Passo 3: Criar venv ───────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[3/7] Criando ambiente virtual em: ${VENV_DIR}${NC}"
[ -d "$VENV_DIR" ] && rm -rf "$VENV_DIR"

if [ "$ARCH" = "armv7l" ]; then
    # --system-site-packages expõe o python3-numpy do apt ao venv
    python3 -m venv --system-site-packages "$VENV_DIR"
    echo -e "${GREEN}  ✓ Ambiente virtual criado (com acesso ao numpy do sistema).${NC}"
else
    # Venv isolado: pip instala tudo sem conflito no x86_64
    python3 -m venv "$VENV_DIR"
    echo -e "${GREEN}  ✓ Ambiente virtual isolado criado.${NC}"
fi

"$PIP" install --quiet --upgrade pip

# ── Passo 4: Instalar dependências Python ─────────────────────────────────────
echo ""
echo -e "${BOLD}[4/7] Instalando dependências Python...${NC}"
echo -e "  (pode levar alguns minutos)"
echo ""

echo -e "  → pyserial"
"$PIP" install --quiet pyserial
echo -e "${GREEN}  ✓ pyserial $("$PIP" show pyserial | awk '/^Version/{print $2}')${NC}"

if [ "$ARCH" = "armv7l" ]; then
    # numpy NÃO é instalado via pip no RPi — vem do sistema (apt)
    # --no-deps evita que o pip tente puxar numpy por cima do sistema
    echo -e "  → opencv-python-headless  (numpy via apt)"
    "$PIP" install --quiet --no-deps opencv-python-headless
    echo -e "${GREEN}  ✓ opencv-python-headless $("$PIP" show opencv-python-headless | awk '/^Version/{print $2}')${NC}"

    # --ignore-installed garante que o Pillow 12.x fique dentro do venv,
    # sobrepondo o Pillow 9.4.0 do sistema que nao possui ImageTk completo
    echo -e "  → Pillow"
    "$PIP" install --quiet --ignore-installed Pillow
else
    # x86_64: instala numpy + opencv normalmente via pip
    echo -e "  → numpy"
    "$PIP" install --quiet numpy
    echo -e "${GREEN}  ✓ numpy $("$PIP" show numpy | awk '/^Version/{print $2}')${NC}"

    echo -e "  → opencv-python-headless"
    "$PIP" install --quiet opencv-python-headless
    echo -e "${GREEN}  ✓ opencv-python-headless $("$PIP" show opencv-python-headless | awk '/^Version/{print $2}')${NC}"

    echo -e "  → Pillow"
    "$PIP" install --quiet Pillow
fi

echo -e "${GREEN}  ✓ Pillow $("$PIP" show Pillow | awk '/^Version/{print $2}')${NC}"

# ── Passo 5: Verificar imports ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[5/7] Verificando imports no venv...${NC}"

ALL_OK=true
check_import() {
    local mod="$1"
    local test="${2:-import $1}"
    if "$PY" -c "$test" 2>/dev/null; then
        echo -e "${GREEN}  ✓ $mod OK${NC}"
    else
        echo -e "${RED}  ✗ $mod FALHOU${NC}"
        ALL_OK=false
    fi
}

check_import tkinter
check_import serial
check_import numpy
check_import cv2
# Testa ImageTk explicitamente, pois e o componente que falhou no Pillow do sistema
check_import "PIL (ImageTk)" "from PIL import Image, ImageTk"

if [ "$ALL_OK" = false ]; then
    echo ""
    echo -e "${RED}  ✗ Um ou mais imports falharam. Verifique os erros acima.${NC}"
fi

# ── Passo 6: Permissões ───────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[6/7] Verificando permissões...${NC}"

NEEDS_RELOGIN=false
for grp in dialout video; do
    if groups "$USER" | grep -qw "$grp"; then
        echo -e "${GREEN}  ✓ '${USER}' já pertence ao grupo '${grp}'.${NC}"
    else
        echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo '${grp}'...${NC}"
        sudo usermod -aG "$grp" "$USER"
        NEEDS_RELOGIN=true
    fi
done

# ── Passo 7: Gerar run.sh ─────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[7/7] Gerando run.sh...${NC}"
cat > "$SCRIPT_DIR/run.sh" << EOF
#!/usr/bin/env bash
cd "\$(dirname "\$0")"
exec "\$(dirname "\$0")/venv/bin/python3" ${MAIN_FILE} "\$@"
EOF
chmod +x "$SCRIPT_DIR/run.sh"

if [ -f "$SCRIPT_DIR/$MAIN_FILE" ]; then
    echo -e "${GREEN}  ✓ run.sh gerado. ${MAIN_FILE} encontrado.${NC}"
else
    echo -e "${YELLOW}  ⚠ run.sh gerado, mas ${MAIN_FILE} não está nesta pasta.${NC}"
fi

# ── Resumo ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${GREEN}║   ✓ Instalação concluída!                                ║${NC}"
echo -e "${BOLD}${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Para executar:  ${CYAN}./run.sh${NC}"
if [ "$NEEDS_RELOGIN" = true ]; then
    echo ""
    echo -e "${YELLOW}  ⚠ Faça logout e login novamente para os grupos terem efeito.${NC}"
fi
echo ""