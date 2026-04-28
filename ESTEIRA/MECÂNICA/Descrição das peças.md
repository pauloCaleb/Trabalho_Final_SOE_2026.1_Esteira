# Descrição das Peças Mecânicas — Esteira Separadora SOE

---

## Peças do Subsistema Estrutural
*Autor: Paulo Caleb Fernandes da Silva*

---

### `Wood_stitches_Esteira_Rolante_SOE.catPART`
**Quantidade:** 3 unidades

Peças plásticas retangulares com dois furos espaçados para fixar duas partes de madeira que compõem a base da esteira. Funcionam como "pontos" que seguram uma madeira na outra e conferem rigidez à base.

---

### `Apoio_Base_Esteira_Rolante_SOE.catPART`
**Quantidade:** 8 unidades

Bases de apoio retangulares com quatro furos para parafusos e um ressalto para servir de pé para a esteira. São utilizadas para duas funções simultaneamente: complementam a costura das peças de madeira e servem de espaçadores da base em relação ao solo.

---

### `Suporte_Motor_Esteira_Rolante_SOE.catPART`
**Quantidade:** 1 unidade

Base de fixação do motor de passo para garantir estabilidade mecânica do motor NEMA 22 e alinhá-lo com o suporte do rolo.

---

### `Trava_Suporte_Motor_Esteira_Rolante_SOE.catPART`
**Quantidade:** 1 unidade

Aro de fixação do motor de passo da esteira. É responsável por auxiliar a fixação do motor em conjunto com o `Suporte_Motor_Esteira_Rolante_SOE`. As duas peças trabalham juntas para manter a fixação do motor de passo.

---

### `Suporte_Parafuso_Esteira_Rolante_SOE.catPART`
**Quantidade:** 3 unidades

Suporte do parafuso que faz o alinhamento do rolo da esteira. O parafuso é fixado ao suporte e cria um ponto de eixo para o rolamento do rolo. É utilizado tanto para o rolo motriz (associado ao motor de passo) quanto para o rolo bobo (utilizando dois suportes para criar os dois pontos de eixo do rolo).

---

### `Tampa_Rolo_Motor_Esteira_Rolante_SOE.catPART`
**Quantidade:** 1 unidade

Tampa do rolo motriz, que associa o eixo do motor de passo à estrutura do rolo. Em conjunto com a `Tampa_Rolo_Rolamento_Esteira_Rolante_SOE` e um cano de PVC de 50 mm, forma o rolo motriz da esteira.

---

### `Tampa_Rolo_Rolamento_esteira_rolante_SOE.catPART`
**Quantidade:** 3 unidades

Tampa do rolo bobo, que associa o eixo do parafuso à estrutura do rolo. O rolo motriz utiliza uma unidade desta peça, enquanto o rolo bobo utiliza duas. A fixação do rolo bobo emprega dois `Suporte_Parafuso_Esteira_Rolante_SOE` em conjunto com dois rolamentos 608 que se apoiam nos parafusos fixados aos suportes.

---

### `Suporte_mesa_Rolante_SOE.catPART`
**Quantidade:** 6 unidades

Suporte para a chapa de PVC que forma a mesa da esteira. Um conjunto dessas peças forma a base da mesa por onde corre o tapete, guiado pelo motor de passo a partir do rolo motriz e do rolo bobo.

---

### `Lateral_esq_rampa_esteira_rolante_SOE.catPART`
**Quantidade:** 1 unidade

Lateral esquerda com alojamento para encaixe de uma chapa de PVC que forma a rampa da saída B da esteira.

---

### `Lateral_dir_rampa_esteira_rolante_SOE.catPART`
**Quantidade:** 1 unidade

Lateral direita com alojamento para encaixe de uma chapa de PVC que forma a rampa da saída B da esteira.

---

## Peças do Subsistema de Sensoriamento, Atuação e Eletrônica
*Autor: Felipe de Castro*

---

### `Cancela_servo.catPART`
**Quantidade:** 1 unidade

Estrutura fixada ao eixo do servo motor responsável por guiar o objeto sobre a esteira para a saída B.

---

### `Suporte_Servo.catPART`
**Quantidade:** 1 unidade

Peça de fixação do servo motor. Este suporte é colado sobre a mesa da esteira e posiciona o servo motor na altura e orientação necessárias para suprir as demandas do escopo do projeto.

---

### `Suporte_STlink.catPART`
**Quantidade:** 1 unidade

Suporte para fixação do ST-Link (programador e debugger do STM32) no corpo da esteira.

---

### `Suporte_laser_c_ajuste_focal.catPART`
**Quantidade:** 3 unidades

Suporte do laser com ajuste focal para o sensor laser. Serve para alinhar o laser ao LDR e deve ser fixado na mesa da esteira com bom alinhamento entre os dois dispositivos. Cada sensor disposto na mesa da esteira possui um suporte de laser.

---

### `Suporte_LDR.catPART`
**Quantidade:** 3 unidades

Suporte do LDR. Serve para alinhar o LDR ao laser e deve ser fixado na mesa da esteira com bom alinhamento entre os dois dispositivos. Cada sensor na mesa possui um suporte de LDR.

---

### `Suporte_laser_Rampa.catPART`
**Quantidade:** 1 unidade

Suporte do laser para a rampa. É colado à lateral da rampa e cria um ponto de fixação por parafuso que prende o conjunto à base da esteira. Deve ser colado alinhado ao suporte do LDR da rampa.

---

### `Suporte_LDR_sensor_Rampa.catPART`
**Quantidade:** 1 unidade

Suporte do LDR para a rampa. É colado à lateral da rampa e cria um ponto de fixação por parafuso que prende o conjunto à base da esteira. Deve ser colado alinhado ao suporte do laser da rampa.

---

### `Suporte_flash_esteira.catPART`
**Quantidade:** 2 unidades

Suporte para a fita de LED que ilumina o objeto na região de leitura do código QR.

---

### `Suporte_poste_camera.catPART`
**Quantidade:** 2 unidades

Suporte plástico que une os dois suportes do flash e forma a estrutura da câmera, fechando a região de leitura da esteira. São utilizadas duas peças para separar igualmente os dois postes do flash.

---

### `Suporte_camera_Esteira_Soe.catPART`
**Quantidade:** 1 unidade

Ponto de fixação da câmera. Deve ser colado sobre os suportes que formam a região de leitura de forma que a câmera fique alinhada com o sensor laser onde o objeto é posicionado sobre o tapete da esteira.

## Case de fixação do Raspberry Pi 3b

*Autor: Nilo - Usuário do MakerWorld*

O case de fixação do RBPi3B utilizado foi obtido através da plataforma Open Source *MakerWorld*, que é um site onde usuários da comunidade de impressão 3D compartilham seus p´roprios desenhos de forma aberta sem fim comercial.

A case foi desenhada inicialmente para o RBPi4 e após impressa foi modificada para abrir os alojamentos das portas USB e HDMI.

O case original pode ser encontrado no seguinte link <https://makerworld.com/pt/models/1500557-case-for-raspberry-pi-4-with-40mm-fan?from=search#profileId-1569716>