#!/usr/bin/env bash
# =============================================================================
# install.sh — Configuração do ambiente virtual e instalação de dependências
# Projeto: Gerador de QR Codes - SOE 2026.1
# Autores: Felipe e Caleb
# =============================================================================

set -e  # Aborta em qualquer erro

VENV_DIR=".venv"
PYTHON_MIN="3.10"

# ── Cores para terminal ──────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RESET='\033[0m'

info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
success() { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[AVISO]${RESET} $*"; }
error()   { echo -e "${RED}[ERRO]${RESET}  $*"; exit 1; }

# ── Verificação do Python ────────────────────────────────────────────────────
info "Verificando versão do Python..."

if ! command -v python3 &>/dev/null; then
    error "Python 3 não encontrado. Instale o Python >= ${PYTHON_MIN} antes de continuar."
fi

PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PYTHON_MAJOR=$(echo "$PYTHON_VERSION" | cut -d. -f1)
PYTHON_MINOR=$(echo "$PYTHON_VERSION" | cut -d. -f2)
MIN_MINOR=$(echo "$PYTHON_MIN" | cut -d. -f2)

if [ "$PYTHON_MAJOR" -lt 3 ] || { [ "$PYTHON_MAJOR" -eq 3 ] && [ "$PYTHON_MINOR" -lt "$MIN_MINOR" ]; }; then
    error "Python ${PYTHON_VERSION} detectado. É necessário Python >= ${PYTHON_MIN}."
fi

success "Python ${PYTHON_VERSION} detectado."

# ── Criação do ambiente virtual ──────────────────────────────────────────────
if [ -d "$VENV_DIR" ]; then
    warn "Ambiente virtual '${VENV_DIR}' já existe. Pulando criação."
else
    info "Criando ambiente virtual em '${VENV_DIR}'..."
    python3 -m venv "$VENV_DIR"
    success "Ambiente virtual criado."
fi

# ── Ativação e instalação ────────────────────────────────────────────────────
info "Ativando ambiente virtual e instalando dependências..."

# shellcheck disable=SC1091
source "${VENV_DIR}/bin/activate"

pip install --upgrade pip --quiet
pip install -r requirements.txt

success "Dependências instaladas com sucesso!"

# ── Instruções finais ────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}============================================================${RESET}"
echo -e "${GREEN}  Instalação concluída!${RESET}"
echo -e "${GREEN}============================================================${RESET}"
echo ""
echo "  Para ativar o ambiente virtual manualmente:"
echo -e "    ${CYAN}source ${VENV_DIR}/bin/activate${RESET}"
echo ""
echo "  Para gerar os QR codes com o arquivo padrão (pecas.json):"
echo -e "    ${CYAN}python gerar_qrcodes.py${RESET}"
echo ""
echo "  Para usar um JSON personalizado:"
echo -e "    ${CYAN}python gerar_qrcodes.py meu_arquivo.json -o minha_pasta/${RESET}"
echo ""
