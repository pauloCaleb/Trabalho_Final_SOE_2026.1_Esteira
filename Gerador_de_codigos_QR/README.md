# 🏭 Gerador de QR Codes — Esteira Separadora de Itens

> **Projeto Final SOE 2026.1**  
> Autores: **Felipe De Castro e Paulo Caleb**  
> Descrição: Sistema de geração automática de QR codes para identificação e rastreamento de peças em uma esteira separadora de itens.

---

## 📋 Sumário

1. [Visão Geral](#-visão-geral)
2. [Estrutura do Projeto](#-estrutura-do-projeto)
3. [Pré-requisitos](#-pré-requisitos)
4. [Instalação e Configuração do Ambiente](#-instalação-e-configuração-do-ambiente)
5. [Formato do Arquivo JSON](#-formato-do-arquivo-json)
6. [Geração dos QR Codes](#-geração-dos-qr-codes)
7. [Referência de Argumentos](#-referência-de-argumentos)
8. [Exemplos de Uso](#-exemplos-de-uso)
9. [Conteúdo Codificado no QR Code](#-conteúdo-codificado-no-qr-code)
10. [Dependências](#-dependências)
11. [Solução de Problemas](#-solução-de-problemas)

---

## 🔍 Visão Geral

Este projeto fornece um script Python capaz de ler um arquivo `.json` contendo informações de peças industriais e gerar, para cada entrada, um arquivo de imagem `.jpg` com o respectivo QR code. Cada QR code codifica:

- O **nome da peça** (identificador textual);
- O **destino** da peça na esteira (endereço hexadecimal);
- A **frase de identificação do projeto**.

O sistema foi desenvolvido como parte do Projeto Final da disciplina **Sistemas de Organização Eletrônica (SOE) 2026.1**.

---

## 📁 Estrutura do Projeto

```
qrcode_project/
│
├── gerar_qrcodes.py      # Script principal de geração dos QR codes
├── pecas.json            # Arquivo JSON de exemplo com dados das peças
├── requirements.txt      # Lista de dependências Python
├── install.sh            # Script de instalação automatizada (Linux/macOS)
├── install.bat           # Script de instalação automatizada (Windows)
├── README.md             # Esta documentação
│
└── qrcodes/              # Pasta de saída (criada automaticamente)
    ├── Engrenagem_A1.jpg
    ├── Parafuso_M8.jpg
    └── ...
```

---

## ⚙️ Pré-requisitos

| Requisito | Versão mínima | Observação |
|-----------|--------------|------------|
| Python    | 3.10         | [Download](https://www.python.org/downloads/) |
| pip       | Incluído com Python | Gerenciador de pacotes |

> **Nota:** O projeto não depende de nenhuma biblioteca do sistema operacional além do Python padrão. Todas as dependências são instaladas dentro do ambiente virtual.

---

## 🚀 Instalação e Configuração do Ambiente

### 1. Clone ou baixe o projeto

```bash
# Via Git
git clone <url-do-repositorio>
cd qrcode_project

# Ou extraia o arquivo .zip e acesse a pasta
cd qrcode_project
```

### 2. Instalação automática (recomendado)

#### Linux / macOS

```bash
# Conceder permissão de execução ao script
chmod +x install.sh

# Executar o instalador
./install.sh
```

#### Windows

```cmd
:: Executar no Prompt de Comando (CMD) ou PowerShell
install.bat
```

O script de instalação realiza automaticamente:
- Verificação da versão do Python;
- Criação do ambiente virtual `.venv`;
- Atualização do `pip`;
- Instalação de todas as dependências listadas em `requirements.txt`.

---

### 3. Instalação manual (passo a passo)

Caso prefira realizar a instalação manualmente, siga os passos abaixo:

#### Passo 1 — Criar o ambiente virtual

```bash
# Linux / macOS
python3 -m venv .venv

# Windows
python -m venv .venv
```

#### Passo 2 — Ativar o ambiente virtual

```bash
# Linux / macOS
source .venv/bin/activate

# Windows (CMD)
.venv\Scripts\activate.bat

# Windows (PowerShell)
.venv\Scripts\Activate.ps1
```

> Após a ativação, o prefixo `(.venv)` aparecerá no início do prompt, confirmando que o ambiente está ativo.

#### Passo 3 — Atualizar o pip

```bash
python.exe -m pip install --upgrade pip
```

#### Passo 4 — Instalar as dependências

```bash
pip install -r requirements.txt
```

#### Passo 5 — Verificar a instalação

```bash
pip list
```

A saída deve conter os pacotes `qrcode` e `Pillow` entre os instalados.

---

### 4. Desativar o ambiente virtual

```bash
deactivate
```

---

## 📄 Formato do Arquivo JSON

O arquivo JSON de entrada deve conter uma **lista (array)** de objetos, onde cada objeto representa uma peça. Os campos obrigatórios são:

| Campo        | Tipo             | Descrição                                              | Exemplo        |
|--------------|------------------|--------------------------------------------------------|----------------|
| `nome_peca`  | `string`         | Nome ou código identificador da peça                  | `"Engrenagem_A1"` |
| `destino`    | `string` (hex)   | Endereço hexadecimal do destino na esteira             | `"0x1A"` ou `"1A"` |

### Exemplo de `pecas.json`

```json
[
  {
    "nome_peca": "Engrenagem_A1",
    "destino": "0x1A"
  },
  {
    "nome_peca": "Parafuso_M8",
    "destino": "0x2B"
  },
  {
    "nome_peca": "Mola_Tensora",
    "destino": "0xFF"
  },
  {
    "nome_peca": "Sensor_Optico",
    "destino": "0xC4"
  },
  {
    "nome_peca": "Correia_Industrial",
    "destino": "0x3D"
  }
]
```

> **Observações sobre o campo `destino`:**
> - Valores com prefixo `0x` (ex: `"0x1A"`) e sem prefixo (ex: `"1A"`) são aceitos;
> - Letras maiúsculas e minúsculas são aceitas (ex: `"0xFF"` e `"0Xff"` são equivalentes);
> - O script normaliza automaticamente para o formato `0xXX` na saída.

---

## ▶️ Geração dos QR Codes

### Comando básico

Com o ambiente virtual ativado, execute:

```bash
# Linux / macOS
python gerar_qrcodes.py

# Windows
python gerar_qrcodes.py
```

Este comando lê o arquivo `pecas.json` (padrão) e salva os QR codes na pasta `qrcodes/`.

---

### Especificando um arquivo JSON personalizado

```bash
python gerar_qrcodes.py meu_lote.json
```

---

### Especificando a pasta de saída

```bash
python gerar_qrcodes.py pecas.json -o imagens/lote_01/
```

---

### Modo silencioso (sem log por item)

```bash
python gerar_qrcodes.py -q
```

---

### Ajustar tamanho do QR code

```bash
# --box-size: tamanho de cada célula em pixels (padrão: 10)
# --border: largura da borda em células (padrão: 4)
python gerar_qrcodes.py --box-size 15 --border 6
```

---

## 📌 Referência de Argumentos

```
uso: python gerar_qrcodes.py [JSON] [opções]

Argumentos posicionais:
  JSON                  Caminho para o arquivo JSON (padrão: pecas.json)

Opções:
  -o, --output PASTA    Pasta de saída dos QR codes (padrão: qrcodes/)
  --box-size N          Tamanho de cada célula em pixels (padrão: 10)
  --border N            Largura da borda em células (padrão: 4)
  -q, --quiet           Modo silencioso: suprime log por item
  -h, --help            Exibe esta mensagem de ajuda
```

---

## 💡 Exemplos de Uso

```bash
# 1. Geração padrão
python gerar_qrcodes.py

# 2. JSON personalizado com pasta de saída definida
python gerar_qrcodes.py producao.json -o saida/qrcodes/

# 3. QR codes maiores (box-size 20) com borda mais larga (border 8)
python gerar_qrcodes.py --box-size 20 --border 8

# 4. Processamento em lote sem exibir detalhes por item
python gerar_qrcodes.py lote_grande.json -q -o resultados/

# 5. Ver ajuda completa
python gerar_qrcodes.py --help
```

---

## 🔠 Conteúdo Codificado no QR Code

Cada QR code gerado armazena um bloco de texto no seguinte formato:

```
Nome da Peça: <nome_peca>
Destino: <destino_hex>
Projeto final SOE 2026.1 - Felipe e Caleb - esteira separadora de itens
```

**Exemplo real** (para `CAIXAS DE JÓIAS - Uberlândia` com destino `0xDA`):

```
Nome da Peça: CAIXAS DE JÓIAS - Uberlândia
Destino: 0XDA
Projeto final SOE 2026.1 - Felipe e Caleb - esteira separadora de itens
```

---

## 📦 Dependências

As dependências estão listadas em `requirements.txt` e são instaladas automaticamente:

| Pacote        | Versão  | Descrição                                       |
|---------------|---------|-------------------------------------------------|
| `qrcode[pil]` | 8.1.0   | Geração de QR codes com suporte a imagens PIL   |
| `Pillow`      | 11.2.1  | Processamento e exportação de imagens (JPEG)    |

---

## 🛠️ Solução de Problemas

### `ModuleNotFoundError: No module named 'qrcode'`

O ambiente virtual não está ativo ou as dependências não foram instaladas.

```bash
source .venv/bin/activate       # Linux/macOS
.venv\Scripts\activate.bat      # Windows
pip install -r requirements.txt
```

---

### `FileNotFoundError: Arquivo JSON não encontrado`

Verifique se o arquivo JSON existe no caminho informado:

```bash
ls pecas.json           # Linux/macOS
dir pecas.json          # Windows
```

---

### `ValueError: 'destino' deve ser um número hexadecimal válido`

O campo `destino` contém um valor não hexadecimal. Verifique se os valores no JSON são strings hexadecimais válidas (ex: `"0x1A"`, `"FF"`, `"3d"`).

---

### Imagens corrompidas ou não abrem

Certifique-se de que o `Pillow` está instalado corretamente:

```bash
pip show Pillow
```

Se necessário, reinstale:

```bash
pip install --force-reinstall Pillow
```

---

## 📝 Licença

Projeto acadêmico desenvolvido para a disciplina **SISTEMAS OPERACIONAIS EMBARCADOS - SOE** durante o semestre **2026.1**. Uso restrito ao contexto educacional.

---

*Documentação gerada para o Projeto Final SOE 2026.1 — Autores: Felipe de Castro  e Paulo Caleb*
