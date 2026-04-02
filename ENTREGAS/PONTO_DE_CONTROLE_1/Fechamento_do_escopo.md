## Fechamento do Escopo – Projeto Final SOE

### 1. Contexto de Aplicação

Sistemas de logística interna em centros de distribuição utilizam linhas automatizadas de transporte e separação de itens para preparação de pedidos. Esses sistemas são compostos por módulos de esteiras interconectados, capazes de identificar e direcionar produtos conforme seu destino.

---

### 2. Definição do Problema

Dado um fluxo contínuo de itens previamente identificados, é necessário desenvolver um módulo de esteira automatizado capaz de:

* Permitir a identificação de cada item;
* Determinar o destino de cada item;
* Direcioná-lo corretamente dentro de uma linha de separação modular.

---

### 3. Objetivo do Projeto

Desenvolver um sistema embarcado para controle de um módulo de esteira automatizada com capacidade de identificação e desvio de itens de acordo com o destino previamente associado ao objeto, com ênfase na integração entre controle embarcado e processamento em sistema Linux embarcado.

#### Objetivos Específicos

- Implementar controle de movimentação da esteira;
- Desenvolver mecanismo de desvio de itens;
- Integrar comunicação com sistema externo;
- Validar o funcionamento do sistema experimentalmente;
---

### 4. Escopo do Sistema

O sistema consiste em um módulo de esteira com:

* 1 entrada de itens;
* 2 saídas (desvio local ou encaminhamento para próximo módulo);

Capaz de operar com objetos que atendam às seguintes restrições:

* Massa máxima: 250 g
* Volume máximo: 125 cm³
* Altura: entre 2 cm e 5 cm

---

### 5. Funcionalidades do Sistema

O sistema embarcado deverá:

1. Detectar a presença de objetos na esteira;
2. Controlar o deslocamento do item até a zona de leitura;
3. Solicitar ao servidor, via interface de comunicação, a identificação do item;
4. Receber do servidor a decisão de destino;
5. Executar o desvio do item para uma das saídas;

---

### 6. Interface com o servidor (ex.: Raspberry Pi)

O módulo deverá possuir interface de comunicação com um servidor que será responsável por:

* Captura de imagem do item na zona de leitura;
* Processamento e decodificação do código QR;
* Determinação do destino do item;
* Envio de comandos ao módulo da esteira;
* Monitoramento do estado do sistema;
* Registro de eventos de operação;

---

### 7. Limitações do Projeto

Este projeto não contempla:

* Integração com sistemas reais de estoque;
* Implementação de múltiplos módulos interconectados;
* Processamento de múltiplos itens simultaneamente;
* Sistemas avançados de visão computacional.

---

### 8. Resultados Esperados

* Protótipo funcional de um módulo de esteira automatizado validado experimentalmente;
* Sistema embarcado operando integrado a um ambiente Linux embarcado;
* Capacidade de identificação e separação de itens conforme o destino definido;  
* Documentação técnica completa do sistema;

---

### 9. Aplicação

O sistema proposto se aplica a processos de automação logística em centros de distribuição, podendo ser utilizado como base para sistemas modulares escaláveis de separação de pedidos. A arquitetura proposta segue o modelo de processamento distribuído, separando tarefas de tempo real e processamento de alto nível.

---

### 10. Referências Bibliográficas

[1] ENGENHARIA HÍBRIDA. Conheça o processo inovador de gestão de estoques da Amazon. Engenharia Híbrida, [s.d.]. Disponível em: https://www.engenhariahibrida.com.br/post/conheca-processo-inovador-gestao-de-estoques-amazon. Acesso em: 30 mar. 2026.

[2] EQUIPE TOTVS. Logística do Mercado Livre: entenda os pilares da eficiência de distribuição da empresa. TOTVS Blog, 25 dez. 2023. Disponível em: https://www.totvs.com/blog/gestao-logistica/logistica-mercado-livre/. Acesso em: 30 mar. 2026.

[3] EQUIPE TOTVS. Logística da Amazon: entenda os pilares da empresa líder do varejo mundial. TOTVS Blog, 16 abr. 2024. Disponível em: https://www.totvs.com/blog/gestao-logistica/logistica-amazon/. Acesso em: 30 mar. 2026.

[4] MERCADO LIVRE. Mercado Envios Full. Mercado Livre, [s.d.]. Disponível em: https://www.mercadolivre.com.br/l/envios-full. Acesso em: 30 mar. 2026.

[5] AMAZON STAFF. Amazon unveils the next generation of fulfillment centers powered by AI and 10 times more robotics. About Amazon, [s.d.]. Disponível em: https://www.aboutamazon.com/news/operations/amazon-fulfillment-center-robotics-ai. Acesso em: 30 mar. 2026.

[6] YOUTUBE. [Vídeo – referência de automação logística]. Disponível em: https://www.youtube.com/watch?v=cRC4iktG-kE. Acesso em: 30 mar. 2026.

[7] YOUTUBE. [Vídeo – escaneamento e automação de itens em esteira]. Disponível em: https://youtu.be/iumMexYY1hg?t=111. Acesso em: 30 mar. 2026.

[8] YOUTUBE. [Vídeo – sistema de separação e desvio de itens]. Disponível em: https://www.youtube.com/watch?v=7AHLNGUSeGQ&t=8s. Acesso em: 30 mar. 2026.

[9] INSTAGRAM. [Reel – demonstração de sistema automatizado de esteira]. Disponível em: https://www.instagram.com/reels/DQKF8Hbja6q/. Acesso em: 30 mar. 2026.

---