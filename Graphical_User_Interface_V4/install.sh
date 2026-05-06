#!/usr/bin/env bash
# =============================================================================
#  install.sh — Esteira Separadora · Sistema de Controle v4.0
#  Raspberry Pi 3B / Linux (Raspbian Bookworm, Bullseye ou compatível)
#
#  O que este script faz:
#    1. Verifica se Python 3 está disponível
#    2. Instala python3-full e dependências de sistema via apt
#    3. Cria o ambiente virtual Python em ./venv
#    4. Instala pyserial, opencv e Pillow dentro do venv
#    5. Verifica dependências do sistema (tkinter)
#    6. Configura permissões de porta serial e câmera
#    7. Gera o script run.sh para execução fácil
#    8. Confirma a instalação
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
MAIN_FILE="esteira_control_v4.py"

echo ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${CYAN}║   ESTEIRA SEPARADORA — Instalação v4.0                   ║${NC}"
echo -e "${BOLD}${CYAN}║   STM32G070 ↔ Raspberry Pi 3B · SOE 2026.1              ║${NC}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ── Passo 1: Python 3 ─────────────────────────────────────────────────────────
echo -e "${BOLD}[1/8] Verificando Python 3...${NC}"
if ! command -v python3 &>/dev/null; then
    echo -e "${RED}  ✗ Python 3 não encontrado.${NC}"
    echo -e "    Instale com: ${YELLOW}sudo apt install python3${NC}"
    exit 1
fi
PY_VERSION=$(python3 --version 2>&1)
echo -e "${GREEN}  ✓ ${PY_VERSION} encontrado.${NC}"

# ── Passo 2: Dependências de sistema ──────────────────────────────────────────
# libgl1 é obrigatório para opencv-python no Raspberry Pi/Linux.
# Sem ela, o import cv2 falha com erro de libGL.so mesmo após pip install.
echo ""
echo -e "${BOLD}[2/8] Instalando dependências de sistema...${NC}"
PKGS="python3-full python3-tk libgl1 libglib2.0-0"
if sudo apt install -y $PKGS 2>/dev/null; then
    echo -e "${GREEN}  ✓ Pacotes de sistema instalados: ${PKGS}${NC}"
else
    echo -e "${YELLOW}  ⚠ Alguns pacotes não puderam ser instalados via apt.${NC}"
    echo -e "    Verifique sua conexão ou rode: ${YELLOW}sudo apt update${NC}"
fi

# ── Passo 3: Criar venv ───────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[3/8] Criando ambiente virtual em: ${VENV_DIR}${NC}"
if [ -d "$VENV_DIR" ]; then
    echo -e "${YELLOW}  ⚠ Pasta venv já existe. Recriando...${NC}"
    rm -rf "$VENV_DIR"
fi
# --system-site-packages permite que o venv acesse tkinter do sistema,
# que não pode ser instalado via pip no Bookworm.
python3 -m venv --system-site-packages "$VENV_DIR"
echo -e "${GREEN}  ✓ Ambiente virtual criado (com acesso ao site-packages do sistema).${NC}"

# Atualiza pip dentro do venv
"$VENV_DIR/bin/pip" install --quiet --upgrade pip

# ── Passo 4: Instalar dependências Python ─────────────────────────────────────
echo ""
echo -e "${BOLD}[4/8] Instalando dependências Python no venv...${NC}"
echo -e "  Isso pode levar alguns minutos na primeira vez."
echo ""

# pyserial (obrigatório)
echo -e "  Instalando pyserial..."
"$VENV_DIR/bin/pip" install pyserial
SERIAL_VER=$("$VENV_DIR/bin/pip" show pyserial 2>/dev/null | grep Version | awk '{print $2}')
echo -e "${GREEN}  ✓ pyserial ${SERIAL_VER} instalado.${NC}"

# numpy (dependência do OpenCV — instalar antes garante versão compatível no RPi)
echo -e "  Instalando numpy..."
if "$VENV_DIR/bin/pip" install numpy 2>/dev/null; then
    NUMPY_VER=$("$VENV_DIR/bin/pip" show numpy 2>/dev/null | grep Version | awk '{print $2}')
    echo -e "${GREEN}  ✓ numpy ${NUMPY_VER} instalado.${NC}"
else
    echo -e "${YELLOW}  ⚠ Falha ao instalar numpy via pip. O OpenCV pode não funcionar.${NC}"
fi

# opencv — tenta headless primeiro (menor, sem dependência de display, ideal para RPi)
# Se headless falhar, tenta a versão completa.
echo -e "  Instalando opencv (versão headless recomendada para RPi)..."
CV2_OK=false
if "$VENV_DIR/bin/pip" install opencv-python-headless 2>/dev/null; then
    CV2_VER=$("$VENV_DIR/bin/pip" show opencv-python-headless 2>/dev/null | grep Version | awk '{print $2}')
    echo -e "${GREEN}  ✓ opencv-python-headless ${CV2_VER} instalado.${NC}"
    CV2_OK=true
fi

if [ "$CV2_OK" = false ]; then
    echo -e "${YELLOW}  ⚠ Headless falhou. Tentando opencv-python completo...${NC}"
    if "$VENV_DIR/bin/pip" install opencv-python 2>/dev/null; then
        CV2_VER=$("$VENV_DIR/bin/pip" show opencv-python 2>/dev/null | grep Version | awk '{print $2}')
        echo -e "${GREEN}  ✓ opencv-python ${CV2_VER} instalado.${NC}"
        CV2_OK=true
    else
        echo -e "${RED}  ✗ Não foi possível instalar opencv via pip.${NC}"
        echo -e "    Alternativa: ${YELLOW}sudo apt install python3-opencv${NC}"
        echo -e "    (e re-crie o venv com --system-site-packages, o que já foi feito)"
    fi
fi

# Verifica se cv2 importa corretamente no venv (detecta erro de libGL antecipadamente)
echo -e "  Verificando import cv2..."
if "$VENV_DIR/bin/python3" -c "import cv2" 2>/dev/null; then
    echo -e "${GREEN}  ✓ cv2 importa corretamente.${NC}"
else
    echo -e "${RED}  ✗ cv2 falhou ao importar mesmo após instalação.${NC}"
    echo -e "    Solução: ${YELLOW}sudo apt install libgl1 libglib2.0-0${NC} (já tentado acima)"
    echo -e "    Se o erro persistir, use: ${YELLOW}sudo apt install python3-opencv${NC}"
    echo -e "    O programa iniciará, mas a câmera QR estará desativada."
fi

# Pillow (renderização do preview da câmera)
echo -e "  Instalando Pillow..."
if "$VENV_DIR/bin/pip" install Pillow 2>/dev/null; then
    PIL_VER=$("$VENV_DIR/bin/pip" show Pillow 2>/dev/null | grep Version | awk '{print $2}')
    echo -e "${GREEN}  ✓ Pillow ${PIL_VER} instalado.${NC}"
else
    echo -e "${RED}  ✗ Falha ao instalar Pillow. O preview da câmera não estará disponível.${NC}"
fi

# ── Passo 5: Verificar tkinter ────────────────────────────────────────────────
echo ""
echo -e "${BOLD}[5/8] Verificando tkinter (interface gráfica)...${NC}"
# tkinter é verificado no python do VENV (que tem --system-site-packages)
if "$VENV_DIR/bin/python3" -c "import tkinter" 2>/dev/null; then
    echo -e "${GREEN}  ✓ tkinter disponível no venv.${NC}"
else
    echo -e "${RED}  ✗ tkinter não encontrado no venv.${NC}"
    echo -e "    Instale e recrie o venv: ${YELLOW}sudo apt install python3-tk${NC}"
    echo -e "    Depois rode este script novamente."
fi

# ── Passo 6: Dependências de câmera ──────────────────────────────────────────
echo ""
echo -e "${BOLD}[6/8] Verificando suporte a câmera USB (Video4Linux)...${NC}"
if ls /dev/video* &>/dev/null 2>&1; then
    echo -e "${GREEN}  ✓ Dispositivo(s) de câmera detectado(s):${NC}"
    ls /dev/video* 2>/dev/null | while read dev; do echo "    $dev"; done
else
    echo -e "${YELLOW}  ⚠ Nenhuma câmera USB detectada no momento.${NC}"
    echo -e "    Conecte a câmera antes de iniciar a GUI."
fi

if groups "$USER" | grep -qw video; then
    echo -e "${GREEN}  ✓ Usuário '${USER}' já pertence ao grupo 'video'.${NC}"
else
    echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo 'video'...${NC}"
    sudo usermod -aG video "$USER"
    echo -e "${YELLOW}  ⚠ Faça logout e login novamente para o grupo ter efeito.${NC}"
fi

# ── Passo 7: Permissão porta serial ───────────────────────────────────────────
echo ""
echo -e "${BOLD}[7/8] Verificando permissão de acesso à porta serial (UART)...${NC}"
if groups "$USER" | grep -qw dialout; then
    echo -e "${GREEN}  ✓ Usuário '${USER}' já pertence ao grupo 'dialout'.${NC}"
else
    echo -e "${YELLOW}  ⚠ Adicionando '${USER}' ao grupo 'dialout'...${NC}"
    sudo usermod -aG dialout "$USER"
    echo -e "${YELLOW}  ⚠ Faça logout e login novamente para a permissão ter efeito.${NC}"
fi

# ── Passo 8: Gerar script run.sh ──────────────────────────────────────────────
echo ""
echo -e "${BOLD}[8/8] Gerando script de execução run.sh...${NC}"
cat > "$SCRIPT_DIR/run.sh" << EOF
#!/usr/bin/env bash
# Gerado automaticamente pelo install.sh
# Executa a GUI v4 usando o python do venv correto
cd "\$(dirname "\$0")"
exec "\$(dirname "\$0")/venv/bin/python3" ${MAIN_FILE} "\$@"
EOF
chmod +x "$SCRIPT_DIR/run.sh"
echo -e "${GREEN}  ✓ run.sh gerado.${NC}"

# Verifica arquivo principal
if [ -f "$SCRIPT_DIR/$MAIN_FILE" ]; then
    echo -e "${GREEN}  ✓ ${MAIN_FILE} encontrado.${NC}"
else
    echo -e "${YELLOW}  ⚠ ${MAIN_FILE} não encontrado neste diretório.${NC}"
    echo -e "    Certifique-se de que o arquivo está na mesma pasta que este script."
fi

# ── Resumo ────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${GREEN}║   ✓ Instalação concluída!                                ║${NC}"
echo -e "${BOLD}${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BOLD}Para executar o sistema (método recomendado):${NC}"
echo ""
echo -e "  ${CYAN}./run.sh${NC}"
echo ""
echo -e "Ou manualmente, usando o python do venv diretamente:"
echo ""
echo -e "  ${CYAN}venv/bin/python3 ${MAIN_FILE}${NC}"
echo ""
echo -e "${YELLOW}ATENÇÃO: NÃO use 'python3 ${MAIN_FILE}' sem ativar o venv —${NC}"
echo -e "${YELLOW}o python do sistema não enxerga as bibliotecas instaladas.${NC}"
echo ""
if groups "$USER" | grep -qw dialout && groups "$USER" | grep -qw video; then
    true
else
    echo -e "${YELLOW}Grupos foram alterados (dialout/video): faça logout e login novamente.${NC}"
    echo ""
fi