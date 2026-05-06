#!/usr/bin/env bash
# =============================================================================
#  install.sh — Esteira Separadora · Sistema de Controle v4.0
#  Raspberry Pi 3B / Linux (Raspbian Bookworm, Bullseye ou compatível)
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

echo ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║   ESTEIRA SEPARADORA — Instalação v4.0                   ║${NC}"
echo -e "${BOLD}${CYAN}║   STM32G070 ↔ Raspberry Pi 3B · SOE 2026.1              ║${NC}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ── Passo 1: Python 3 ─────────────────────────────────────────────────────────
echo -e "${BOLD}[1/7] Verificando Python 3...${NC}"
if ! command -v python3 &>/dev/null; then
    echo -e "${RED}  ✗ Python 3 não encontrado. Instale com: sudo apt install python3${NC}"
    exit 1
fi
echo -e "${GREEN}  ✓ $(python3 --version) encontrado.${NC}"

# ── Passo 2: Dependências de sistema ──────────────────────────────────────────
# python3-full  → habilita criação de venv no Bookworm
# python3-tk    → tkinter (não existe via pip, precisa vir do sistema)
# libgl1        → obrigatório para cv2 importar no Linux/RPi
# libglib2.0-0  → dependência de runtime do OpenCV
echo ""
echo -e "${BOLD}[2/7] Instalando dependências de sistema via apt...${NC}"
sudo apt install -y python3-full python3-tk libgl1 libglib2.0-0
echo -e "${GREEN}  ✓ Dependências de sistema OK.${NC}"

# ── Passo 3: Criar venv ISOLADO ───────────────────────────────────────────────
# SEM --system-site-packages: evita conflito de versões (ex: numpy 1.x do
# sistema vs numpy 2.x exigido pelo opencv-python-headless mais recente).
# tkinter funciona mesmo assim pois é uma extensão C do python3 do sistema,
# não um pacote pip — o venv a herda automaticamente.
echo ""
echo -e "${BOLD}[3/7] Criando ambiente virtual isolado em: ${VENV_DIR}${NC}"
[ -d "$VENV_DIR" ] && rm -rf "$VENV_DIR"
python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/pip" install --quiet --upgrade pip
echo -e "${GREEN}  ✓ Ambiente virtual criado.${NC}"

# ── Passo 4: Instalar dependências Python ─────────────────────────────────────
echo ""
echo -e "${BOLD}[4/7] Instalando dependências Python...${NC}"
echo -e "  (pode levar alguns minutos)"
echo ""

# pyserial
echo -e "  → pyserial"
"$VENV_DIR/bin/pip" install --quiet pyserial
echo -e "${GREEN}  ✓ pyserial $("$VENV_DIR/bin/pip" show pyserial | awk '/^Version/{print $2}')${NC}"

# numpy — versão compatível com o opencv que será instalado
echo -e "  → numpy"
"$VENV_DIR/bin/pip" install --quiet "numpy>=2.0"
echo -e "${GREEN}  ✓ numpy $("$VENV_DIR/bin/pip" show numpy | awk '/^Version/{print $2}')${NC}"

# opencv-python-headless (menor, sem dependência de display, ideal para RPi)
echo -e "  → opencv-python-headless"
"$VENV_DIR/bin/pip" install --quiet opencv-python-headless
echo -e "${GREEN}  ✓ opencv-python-headless $("$VENV_DIR/bin/pip" show opencv-python-headless | awk '/^Version/{print $2}')${NC}"

# Pillow
echo -e "  → Pillow"
"$VENV_DIR/bin/pip" install --quiet Pillow
echo -e "${GREEN}  ✓ Pillow $("$VENV_DIR/bin/pip" show Pillow | awk '/^Version/{print $2}')${NC}"

# ── Passo 5: Verificar imports ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[5/7] Verificando imports no venv...${NC}"

check_import() {
    local mod="$1"
    if "$VENV_DIR/bin/python3" -c "import $mod" 2>/dev/null; then
        echo -e "${GREEN}  ✓ $mod OK${NC}"
    else
        echo -e "${RED}  ✗ $mod FALHOU — verifique os erros acima${NC}"
    fi
}

check_import tkinter
check_import serial
check_import cv2
check_import PIL

# ── Passo 6: Permissões ───────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[6/7] Verificando permissões...${NC}"

for grp in dialout video; do
    if groups "$USER" | grep -qw "$grp"; then
        echo -e "${GREEN}  ✓ Usuário '${USER}' já pertence ao grupo '${grp}'.${NC}"
    else
        echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo '${grp}'...${NC}"
        sudo usermod -aG "$grp" "$USER"
        echo -e "${YELLOW}    → Faça logout/login para o grupo ter efeito.${NC}"
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
echo ""