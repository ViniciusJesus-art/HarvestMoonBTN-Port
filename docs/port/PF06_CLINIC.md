# PF-06 — Clínica e remédios

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). O médico abre o atendimento
pelo bytecode original da conversa. Exame e farmácia usam o modal de lojas,
a economia, o inventário e os atributos existentes; remédios são consumidos
pelo fluxo de bebidas já entregue. Medicina animal continua separada.

## Referência C e BIN instalado

- `shop_medicine_menu_setup`, `shop_doctor_step`, `shop_condition_message` e
  `shop_item_carry_step`, em `menu/shop_dialogues.c`.
- `event_cmd_dispatch`, em `event/event_commands.c`: `(8,5,0,0)` solicita
  `SHOP_DIALOGUE_DOCTOR` (20). O nome antigo do enum `EVENT_SHOP_PLOT_UPGRADE`
  não descreve essa funcionalidade; o despacho real é para o médico.
- Conversa do **NPC 8** em `12_event_s.bin`, intervalo **0x74c800..0x74d800**,
  entrada marcada 0. Instruções 2..7 exigem mão vazia, dia da semana diferente
  de **3 (quarta-feira)** e horário **09:00..15:59**; iniciam a loja e aguardam
  seu fechamento. Presença, alvo e acesso continuam nas rotas/zonas originais.
- A porta da área **40** (local 29) leva à clínica **188** (local 32), evento
  **27**, condicionado à presença do médico e às regras de acesso existentes.
  A área 192 é o andar superior. Não foi criado um balcão ou teleporte novo.
- `milestone_check_blue_grass_shipped` e `milestone_check_green_grass_shipped`
  recalculam os bits 1/2 após **51 remessas** das ervas **93/95**. O host lê os
  mesmos contadores persistidos, sem outro estado de desbloqueio.

Auditoria LE local em `A_FILE/11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Conferência |
|---|---|
| **0x80114194** | Início de `shop_medicine_menu_setup`, conforme símbolos do objeto PS1. |
| **0x80112440 / 0x80112494** | Marcos azul/verde: IDs **0x5d/0x5f**, comparação com **0x33 (51)** e bits **1/2**. |
| **0x80115fe0** | `shop_doctor_step`; imediatos do exame: custo **10**, fades **20**, mensagens **0xe5/e7/e9/ea**; compra **0xfb**, retorno **0xfc**. |
| **0x80128214** | Matriz LE de 20 diagnósticos, incluindo **0x1d9..0x1dc** para stamina de 5% a menos de 20%. |

O catálogo de `disc/SLUS_011.15`, endereço **0x8004b1a4**, passo **40 bytes**,
confere preços, figuras **41..44**, flags **0x9400**, classe **0xc0000000** e
callback **0x800d622c** (`farm_clamp_status`). Endereços servem à auditoria;
o host usa registros/IDs e tabelas geradas, sem executar ponteiros PS1.

| Ordem do menu | ID | Preço | Stamina | Fadiga | Disponibilidade |
|---|---|---|---|---|---|
| Bodigizer | 155 | 500 G | +50 | -1 | Base |
| Turbojolt | 157 | 1.000 G | 0 | -20 | Base |
| Bodigizer XL | 156 | 1.000 G | +100 | -1 | 51 ervas azuis remetidas |
| Turbojolt XL | 158 | 2.000 G | 0 | -50 | 51 ervas verdes remetidas |

## Comportamento adaptado

`generate_farm_tables.py` extrai nomes, ordem, IDs, descrições e matriz de
resultados do C original. Linhas XL ocultas não mudam a identidade da descrição
da linha visível. O menu inicial oferece exame/remédios; Triangle explica,
Circle volta/cancela e Cross seleciona. A pergunta do exame usa Yes/No e o
cursor comum. O prompt de seleção é **2:0x46**, preservando a correção dos menus.

O exame pede **10 G**. Recusa ou falta de saldo volta ao menu sem cobrança.
A aceitação debita uma única vez, antes de **2:0xe7**; depois do fechamento
ocorrem `fade_start_out(20,1,2)`, mensagem **2:0xe9** sobre fundo preto, fade in,
**2:0xea** e diagnóstico. A sombra avança no tick do jogo e fica abaixo da
mensagem, como no OT original. A duração considera `length=21` e fim em 22 no
fade out, e nível inicial 19 até zero no fade in.

O diagnóstico usa porcentagens de stamina **70/50/20/5** e fadiga **50/70/90**.
**O exame não cura nem avança o relógio no código original**: a adaptação também
não altera stamina/fadiga/felicidade, não cobra novamente e não concede itens.
MaxStamina zero, fora do estado normal, recebe a faixa inferior sem divisão por
zero; o cálculo host usa largura explícita para evitar overflow.

Selecionar um remédio acessível e pagável debita o preço imediatamente, espera
**2:0xfb**, executa a apresentação ANI e entrega **um** registro na mão. Mochila
cheia não impede essa entrega; a conversa exige mão vazia. Saldo insuficiente
produz **2:0x4f**, **2:0xfc** e volta ao menu principal. O item pode ser guardado
na mochila/geladeira e bebido por Square, com som **75**, banco de ação **14** e
remoção/efeitos únicos no fim da ANI, sujeitos aos limites de atributos.

Não há coleção paralela, estoque finito ou flag de compra única. Não foi preciso
alterar o codec v19 existente. As animações de portas do usuário e `runtime.c`
permanecem idênticos ao início deste recorte. `src/`, SDK, disco e fluxo PS1
não foram editados; a validação host não substitui toolchain/SHA-1 PS1.

## Verificação

`clinic_test` cobre 80 combinações de fronteiras do diagnóstico, matriz e
porcentagens, os quatro estados de desbloqueio XL (50/51), descrições,
cancelamentos, recusa e saldo insuficiente, momento da cobrança e ausência de
duplicação, fades, mão vazia/mochila cheia, quatro entregas/consumos e retomada.

`clinic_playthrough` usa os recursos instalados e comandos reais: entra pela
porta da clínica, conversa com o médico, recusa e aceita o exame, confere
fade/mensagem preta/diagnóstico sem cura e compra os quatro produtos. Coleta e
remete a **51ª erva azul** na primavera e a **51ª verde** no outono; os 50 envios
anteriores são preparação explícita de progresso, sem flag XL artificial.
Guarda os quatro medicamentos pela UI da geladeira, grava/reabre o estado,
retira e bebe cada um pela ANI real, terminando com stamina **100** e fadiga **0**.
Saldo inicial, casa ampliada e atributos também são fixtures declaradas;
nenhum remédio é concedido pelo teste. Checkpoints após compras, com remédio
na mão e após consumos são abertos por três processos novos do aplicativo.
Capturas locais conferem menu, pergunta, fade, diagnóstico, entrega e bebida;
não são adicionadas ao Git.

Configure/build Debug e Release aprovados. Debug **11/11** selecionados,
incluindo clínica, consumo, inventário em eventos, lojas, saves, conversa,
portas e os percursos de chuva/fidelidade. Release **4/4** selecionados.
Os dois incluem o percurso clínico e três retomadas em processos novos.
Build Windows MinGW aprovado em contêiner local sem rede; Wine aprovou
unidade, percurso e as três retomadas pelo aplicativo Windows. Todas as
execuções finais terminaram com saída **0**.

**42 processos de teste**: sete avulsos iniciais, 16 da seleção Debug, nove da
seleção Release e dez Wine/targets. Inclui cinco tentativas iniciais com
preparação incorreta do teste (área de acesso, dia de fechamento em duas
execuções, navegação de mochila ampliada e estação da erva verde),
corrigidas pelo bytecode, zonas e tabelas originais; essas tentativas não
exigiram alteração de gameplay. Conta CTest, wrappers, filhos e lançadores;
teto de **50** respeitado, sem suíte inteira. Avisos locais de Mesa/EDID do
Wine não impediram a validação. Avisos antigos de `const` nos testes de mundo
continuam fora deste recorte. `git diff --check` aprovado e comparação do
`runtime.c` com o início do recorte confirma preservação integral.

## Limites

Este recorte entrega o atendimento voluntário e os remédios humanos. Colapso,
internação e outros eventos ainda não portados não são declarados completos.
Vinícola, refeições da hospedaria, festivais e demais progressões seguem na
fila PF-06. O bootstrap `--diagnostics` não executa o jogo; o runtime permanece
parcial. Wine não equivale a uma validação em máquina Windows física.
