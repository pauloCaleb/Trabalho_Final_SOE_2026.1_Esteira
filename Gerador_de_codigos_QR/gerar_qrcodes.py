"""
Gerador de QR Codes - Projeto Final SOE 2026.1
Autores: Felipe e Caleb
Descrição: Gera QR codes a partir de um arquivo JSON com informações de peças.
"""

import json
import os
import sys
import argparse
import qrcode
from qrcode.image.pure import PyPNGImage
from PIL import Image


FRASE_PROJETO = "Projeto final SOE 2026.1 - Felipe e Caleb - esteira separadora de itens"


def carregar_json(caminho_json: str) -> list[dict]:
    """Carrega e valida o arquivo JSON com as informações das peças."""
    if not os.path.exists(caminho_json):
        raise FileNotFoundError(f"Arquivo JSON não encontrado: {caminho_json}")

    with open(caminho_json, "r", encoding="utf-8") as f:
        dados = json.load(f)

    if not isinstance(dados, list):
        raise ValueError("O arquivo JSON deve conter uma lista de objetos.")

    for i, item in enumerate(dados):
        if "nome_peca" not in item:
            raise ValueError(f"Item {i} está sem o campo 'nome_peca'.")
        if "destino" not in item:
            raise ValueError(f"Item {i} está sem o campo 'destino'.")
        if not isinstance(item["nome_peca"], str):
            raise ValueError(f"Item {i}: 'nome_peca' deve ser uma string.")
        destino = item["destino"]
        if isinstance(destino, str):
            # Valida se é hexadecimal válido (com ou sem prefixo 0x)
            try:
                int(destino, 16)
            except ValueError:
                raise ValueError(
                    f"Item {i}: 'destino' deve ser um número hexadecimal válido. Recebido: '{destino}'"
                )
        elif isinstance(destino, int):
            # Aceita inteiro e converte para hex
            pass
        else:
            raise ValueError(f"Item {i}: 'destino' deve ser uma string hexadecimal ou inteiro.")

    return dados


def formatar_destino(destino) -> str:
    """Normaliza o destino para string hexadecimal com prefixo 0x."""
    if isinstance(destino, int):
        return hex(destino).upper().replace("X", "x")
    destino_str = str(destino).strip()
    if destino_str.lower().startswith("0x"):
        valor = int(destino_str, 16)
    else:
        valor = int(destino_str, 16)
    return hex(valor).upper().replace("X", "x")


def montar_conteudo_qr(nome_peca: str, destino: str) -> str:
    """Monta a string que será codificada no QR code."""
    destino_fmt = formatar_destino(destino)
    conteudo = (
        f"Nome da Peça: {nome_peca}\n"
        f"Destino: {destino_fmt}\n"
        f"{FRASE_PROJETO}"
    )
    return conteudo


def sanitizar_nome_arquivo(nome: str) -> str:
    """Remove caracteres inválidos para uso em nomes de arquivo."""
    caracteres_invalidos = r'\/:*?"<>|'
    for c in caracteres_invalidos:
        nome = nome.replace(c, "_")
    return nome.strip()


def gerar_qrcode(conteudo: str, caminho_saida: str, tamanho_caixa: int = 10, borda: int = 4):
    """Gera e salva um QR code como imagem .jpg."""
    qr = qrcode.QRCode(
        version=None,
        error_correction=qrcode.constants.ERROR_CORRECT_H,
        box_size=tamanho_caixa,
        border=borda,
    )
    qr.add_data(conteudo)
    qr.make(fit=True)

    img = qr.make_image(fill_color="black", back_color="white").convert("RGB")
    img.save(caminho_saida, format="JPEG", quality=95)


def processar_pecas(
    caminho_json: str,
    pasta_saida: str = "qrcodes",
    tamanho_caixa: int = 10,
    borda: int = 4,
    verbose: bool = True,
):
    """Pipeline principal: carrega JSON, gera e salva os QR codes."""
    pecas = carregar_json(caminho_json)
    os.makedirs(pasta_saida, exist_ok=True)

    gerados = 0
    erros = 0

    for i, peca in enumerate(pecas):
        nome_peca = peca["nome_peca"]
        destino = peca["destino"]

        try:
            conteudo = montar_conteudo_qr(nome_peca, destino)
            nome_arquivo = sanitizar_nome_arquivo(nome_peca)
            caminho_saida = os.path.join(pasta_saida, f"{nome_arquivo}.jpg")

            # Evita sobrescrita: adiciona sufixo numérico se necessário
            contador = 1
            base = caminho_saida.replace(".jpg", "")
            while os.path.exists(caminho_saida):
                caminho_saida = f"{base}_{contador}.jpg"
                contador += 1

            gerar_qrcode(conteudo, caminho_saida, tamanho_caixa, borda)
            gerados += 1

            if verbose:
                print(f"[OK] QR code gerado: {caminho_saida}")
                print(f"     Peça   : {nome_peca}")
                print(f"     Destino: {formatar_destino(destino)}")
                print()

        except Exception as e:
            erros += 1
            print(f"[ERRO] Falha ao processar item {i} ('{nome_peca}'): {e}", file=sys.stderr)

    print(f"Concluído. {gerados} QR code(s) gerado(s) em '{pasta_saida}/'.")
    if erros:
        print(f"Atenção: {erros} item(ns) com erro.", file=sys.stderr)

    return gerados, erros


def main():
    parser = argparse.ArgumentParser(
        description="Gerador de QR Codes para esteira separadora de itens - SOE 2026.1"
    )
    parser.add_argument(
        "json",
        nargs="?",
        default="pecas.json",
        help="Caminho para o arquivo JSON com as peças (padrão: pecas.json)",
    )
    parser.add_argument(
        "-o", "--output",
        default="qrcodes",
        help="Pasta de saída para os QR codes (padrão: qrcodes/)",
    )
    parser.add_argument(
        "--box-size",
        type=int,
        default=10,
        help="Tamanho de cada célula do QR code em pixels (padrão: 10)",
    )
    parser.add_argument(
        "--border",
        type=int,
        default=4,
        help="Largura da borda em células (padrão: 4)",
    )
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Suprime a saída detalhada por item",
    )

    args = parser.parse_args()

    try:
        processar_pecas(
            caminho_json=args.json,
            pasta_saida=args.output,
            tamanho_caixa=args.box_size,
            borda=args.border,
            verbose=not args.quiet,
        )
    except (FileNotFoundError, ValueError) as e:
        print(f"[ERRO] {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
