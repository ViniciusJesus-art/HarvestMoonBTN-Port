# PF-06 — Refeições e bebidas da hospedaria

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). O balcão original de Doug abre
os cardápios diurno/noturno. A refeição é paga e consumida no local pela ANI
original, e o registro que estava na mão é restaurado ao terminar. O fluxo
reutiliza modal, economia, mensagens, inventário e atributos existentes.

## Referência C e BIN

`people/player.c`: `player_act_zone_front`, `player_deliver_to_villager`,
`player_receive_item`, `player_entry_refresh`, término dos modos e desenho.
A exceção de Doug é o **NPC 6** no **local 27 / 0x1b**, numa zona de entrega
**36 / 0x24**. Não depende de uma opção nova na conversa: com o alvo e a face
corretos, solicita diretamente `SHOP_DIALOGUE_ITEM_16` (**22 / 0x16**).
A mão ocupada não impede esse atendimento.

`menu/shop_dialogues.c`: `shop_menu_setup_food_or_drinks`, `shop_goods_run`,
`shop_menu_open_choose` e `shop_menu_message_select`. O cardápio é escolhido
na abertura: seis opções entre **06:00 e 17:59**, quatro fora desse intervalo.
Água aparece em ambos. A presença de Doug na posição original é obrigatória;
não foram acrescentados horários de negócio ou acesso sem o vendedor.

Cabeçalhos/zonas do `00_gf.bin` instalado conferem a porta da área **48**
(local 26), evento **34**, para a hospedaria **204** (local 27). No interior,
a zona **2**, tipo **0x4000/36**, alvo **6**, face **2**, aponta para Doug em
**(3588, 0, 3099)**. O alcance frontal e o teste do alvo são os já existentes
em `hmbtn_village_zone_target`; aproximar-se de outro morador ou falar com
Doug fora dessa zona continua pelos caminhos normais.

Auditoria LE em `A_FILE/11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Conferência |
|---|---|
| **0x800c8808** | `player_deliver_to_villager`; verifica NPC 6/local 0x1b e solicita a loja 0x16. |
| **0x8011775c** | `shop_menu_setup_food_or_drinks`; tabelas, duas mensagens de abertura e sequência de resultado. |
| **0x80117bb4** | `shop_goods_run`; débito, mensagens e chamada de recebimento com estado 2/3 conforme flag 0x400. |
| **0x800cbb9c** | `player_receive_item`; snapshot da mão e flags 0x1200/0x1600 forçam comer/beber. |
| **0x800cbd08..0x800cbd54** | Comida: modo 0x31/ANI 5/som 0x4a. Bebida: modo 0x4c/range 0/som 0x4b. |
| **0x801231fc** | `player_anim_banks`: entrada **17 = 360**, usada por modo **0x4c - 0x3b**; a entrada 14 continua sendo a bebida carregada. |

O catálogo de `disc/SLUS_011.15`, endereço **0x8004b1a4**, passo **40 bytes**,
confere categoria **3**, flags **0x1200** (comida) ou **0x1600** (bebida),
callback **0x800d622c** (`farm_clamp_status`), preços e efeitos abaixo. Os
endereços servem à auditoria; callbacks PS1 não viram ponteiros host.

| Opção | ID | Preço | Stamina | Fadiga | Menu |
|---|---|---|---|---|---|
| Refeição | 168 | 500 G | +50 | -10 | Diurno |
| Salada | 304 | 300 G | +10 | -20 | Diurno |
| Torta de maçã | 305 | 300 G | +30 | -1 | Diurno |
| Cheesecake | 306 | 250 G | +20 | -1 | Diurno |
| Cookie | 307 | 200 G | +10 | -1 | Diurno |
| Licor de uva | 169 | 500 G | 0 | -20 | Noturno |
| Suco de abacaxi | 308 | 300 G | 0 | -15 | Noturno |
| Leite | 170 | 200 G | 0 | -10 | Noturno |
| Água | 171 | 0 G | +1 | 0 | Ambos |

Felicidade não muda nessas opções. Água gratuita/repetível mantém a regra do
original. Não é o leite carregável da fazenda nem o vinho/suco da vinícola.

## Adaptação

`generate_farm_tables.py` extrai nomes, IDs, descrições, ordem e mensagens para
`inn_menu.inc`. `feed_shop` conserva a escolha do menu durante o atendimento,
usa o painel/cursor comum e adapta `shop_goods_run`:

- Abertura **2:0x10b/0x10c**, prompt **2:0x46**, informações por Triangle e
  cancelamento **2:0x116** sem alteração de inventário/saldo.
- Sem saldo: **2:0x4f**, **2:0x117**, retorno à seleção sem debitar.
- Seleção pagável: débito imediato e único, **2:0x118**, **2:0x119**, depois
  recebimento/consumo automático. Água não aumenta o livro de despesas.

`event_item_receive` adapta a parte de `player_receive_item` para refeições
com callback de atributos: guarda o registro completo da mão, coloca
`{id,0,1}` temporariamente e inicia comida ANI **5** ou bebida de ação **17**.
O modo de bebida é reconhecido pelo mesmo helper que restaura o banco base
no fim; o consumo de item carregado continua no banco **14**. A comida oculta
a figura a partir do frame **2**; a bebida não desenha uma figura carregada
extra, pois o recipiente está na ANI.

No fim da animação, o callback comum limita stamina/fadiga/felicidade e o
registro anterior retorna com **ID, modificador e quantidade** preservados.
Não há item da refeição para colocar na mochila ou geladeira. Mochila cheia
não impede o serviço. O original não solicita fade nem avanço explícito do
relógio nesse fluxo; a adaptação também não simula essas operações.

O codec v19 existente é suficiente; gravação continua exigindo estado estável.
Em `runtime.c`, a única diferença deste recorte é reconhecer também a bebida
servida ao restaurar o banco de animação. Trechos de portas e as alterações do
usuário foram preservados. Não houve edição de `src/`, SDK, recursos ou fluxo
CMake PS1. O build host não substitui toolchain/verificação SHA-1 PS1.

## Verificação funcional

`inn_shop_test` cobre zona/face/alvo/presença, recusa de outros locais,
fronteiras **00/05/06/17/18/23 h** e menu fixado na abertura, todas as dez
linhas dos dois cardápios, preços/descrições, cancelamento, falta de saldo,
débito/consumo únicos e água sem débito. Exercita quatro direções das ANIs,
mochila cheia, restauração de registro com modificador/quantidade, efeitos
adiados e retomada. Recebimentos fora da categoria/flags admitidas são recusados.

`inn_shop_playthrough` usa recursos instalados e controles reais:

1. Coleta erva azul num registro de coleta original, entra pela porta da
   hospedaria e abre o atendimento na zona de Doug com essa erva na mão.
2. Cancela, tenta comprar sem saldo, confirma que nada foi perdido e compra/
   consome as seis opções diurnas, preservando o inventário após cada ANI.
3. Reabre um checkpoint, verifica o menu diurno às **17:59** e o noturno às
   **18:00**, e compra/consome as quatro opções noturnas, inclusive água.
4. Verifica bloqueio de outros controles durante o serviço, figuras/ocultação,
   banco de bebida, efeitos/pagamento únicos, relógio sem avanço artificial e
   erva na mão depois de todos os consumos. Sai pela porta e retoma novamente.

Saldo **3.000 G** e atributos iniciais são fixtures. A erva é coletada; todas
as refeições vêm do balcão. Após gastar **2.550 G**, termina com saldo **450 G**,
stamina **122/200**, fadiga **22**, felicidade **12** e a mesma erva carregada.
Checkpoints diurno, noturno e após sair são abertos em processos novos do app.
Capturas locais conferem menus, cursor, mensagens, comida oculta e bebida;
não entram no Git.

Configure/build Debug e Release aprovados. Debug **15/15** selecionados,
incluindo regras/percurso da hospedaria, consumo direto com compras/receita e
retomadas, clínica/vinícola, inventário/comandos, lojas/saves e regressões de
chuva/portas/fidelidade. Release **4/4** selecionados, incluindo hospedaria/
percurso/três retomadas. Build Windows MinGW aprovado em contêiner local sem
rede; Wine aprovou unidade, percurso completo e três retomadas no aplicativo
Windows. Todas as execuções finais terminaram com saída **0**.

**46 processos de teste**: dois avulsos iniciais, 25 da seleção Debug, nove da
seleção Release e dez Wine/targets. Conta CTest, wrappers, produtores, filhos
e lançadores; teto de **50** respeitado, sem suíte inteira. Não houve tentativa
de teste que falhou neste recorte. Avisos locais de Mesa/EDID do Wine não
impediram a validação. Avisos antigos de `const` nos testes de mundo continuam
fora do recorte. `git diff --check` aprovado; comparação de `runtime.c` confirma
somente a troca do predicado de término de bebida, mantendo as portas intactas.

## Limites

O atendimento de Doug está entregue neste recorte. Restaurante sazonal de
Kai, hospedagem, eventos restantes dos moradores, família e festivais seguem
em PF-06. A adaptação não declara o ciclo completo da hospedaria concluído.
O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.
Wine não equivale à validação numa máquina Windows física.
