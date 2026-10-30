# PF-06 — Máquinas de Saibara

Recorte de 2026-10-29. Adaptação em `port/` de encomenda, prazo, entrega e
produção, conforme a fila do [ROADMAP.md](ROADMAP.md). Reutiliza a loja modal,
script instalado, inventário, economia, relógio, ANI, mapas e remessa existentes.

## Referência C e BIN

- `menu/shop_dialogues.c`: `shop_tool_upgrade_menu_setup`, `shop_stock_step` e
  `shop_item_menu_step`. Itens **0x13f/0x140/0x141**, custo **20.000 G** e uma
  adamantita **0x62** por encomenda. Texto **2:0x1e2**, segunda confirmação
  **2:0xa5**, recusa/material/saldo **2:0xa4**, cancelamento **2:0x93**,
  aceitação **2:0x9f**. As duas confirmações antecedem os efeitos.
- `event/milestones.c`: `milestone_set_coop_barn_bits`, `milestone_set_timer`,
  `milestone_tick`, `milestone_status_query`. O galinheiro ampliado libera
  maionese; queijo exige celeiro ampliado e maionese adquirida; fio exige queijo.
  A oferta segue essa ordem, com apenas uma encomenda Saibara pendente.
- BIN instalado, overlay `00_gf.bin`: `milestone_set_timer` **0x80111e68**,
  `milestone_tick` **0x80111f88**, `milestone_status_query` **0x80112160**.
  O contador começa em **4**; decrementar 1→0 conserva a flag **0x37**.
  A chamada seguinte, com zero, limpa a flag: **cinco viradas de dia**, não quatro.
  `world/clock_events.c` chama essa rotina no trabalho noturno.
- Script instalado **354**, entrada principal (conversa de Saibara), passos
  **11..32**: comando **81/20** consulta encomenda, lê o item em slot 7 e aplica
  a entrega depois do texto **2:0xa7/0xa8/0xa9**. Não há entrega automática de
  máquina na casa do jogador nem item fictício na mochila.
- `world/scene.c:scene_resolve_selection`: níveis lógicos de máquina selecionam
  **120..123** no galinheiro, **104..107** com queijo e **108..111** com queijo/fio.
  Os modelos, texturas e zonas **14/33/15** já existem nesses mapas.
  `event_cmd_set_target(-3)`, também conferido no BIN **0x800e5e98**, tem uma
  precedência diferente de bits do celeiro. O carregamento comum segue
  `scene_resolve_selection`, por nível; não usa esse helper de alvo de evento.
- `people/player.c`: `player_zone_egg_box_use`, `player_zone_milk_box_use`,
  `player_zone_wool_box_use` e ramos `PLAYER_MODE_MAKE_*`: ponto 224 à frente,
  limites estritos, direção da zona e animação base **23+direção**, seguida de
  apresentação **11+direção**. Ingrediente inadequado usa **2:0x1a2..0x1a4**.

O BIN instalado tem SHA-1 `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6` para
o overlay conferido. A referência PS1 é `664eee540550138d6e3717bb7e1593aeb8fe9d38`;
não se afirma igualdade com essa referência. Nenhum recurso foi adicionado ao Git.

## Estado e integração

`makers.c` mantém somente posse, encomenda e manhãs restantes na sessão. As
flags usadas pela agenda são derivadas desse estado. Ampliações físicas continuam
em `construction`/`poultry`, com capacidades 10/20 e 5/10 preservadas.
A encomenda não ganha uma segunda autoridade de inventário ou dinheiro.

A loja exibe a próxima máquina elegível e mantém compra das ferramentas animais.
A validação/remoção usa o inventário acessível, sem aceitar adamantita no armário
ou na geladeira. O fluxo pede mãos vazias, revalida antes de efetivar, retira uma
unidade e debita uma vez. A apresentação real da adamantita usa `event_item`;
a aceitação termina com a transição existente para área **32**, posição
**(1000,0,-2800)**, e refaz apenas a agenda de Saibara como no original.
As animações locais de portas não foram modificadas neste recorte.

O contador diário reutiliza a guarda por data de `daily.c`, inclusive ao avançar
vários dias. A consulta de entrega também exige prazo encerrado: falar novamente
antes dele não pode aplicar a máquina nem abrir uma nova encomenda.
O script 354 faz a entrega uma única vez, ao falar com Saibara.

O carregador conserva as áreas lógicas **96/112**, escolhendo os recursos com
máquinas conforme posse e estação. Cochos, alimentação, animais e portas usam
as mesmas áreas e donos de estado. Não há modelo ou instalação sintetizados.

A conversão ocupa o mesmo bloqueio de ação de `farm_feed`. O ingrediente fica
na mão até a ANI de submissão terminar; o produto o substitui e passa pela ANI
de recebimento. Não exige uma vaga na mochila: mesmo cheia, o produto substitui
o ingrediente na mão. Entradas inadequadas não consomem itens; direção errada
não inicia conversão. Todos os produtos seguem a remessa comum.

| Matéria-prima | Produto |
|---|---|
| Ovo normal/bom/excelente/dourado | Maionese S/M/L/G |
| Ovo de Popuri | Maionese L |
| Leite S/M/L/G | Queijo S/M/L/G |
| Lã S/M/L/G | Fio S/M/L/G |

O codec atual **v15** inclui posse/encomenda/prazo; ações, mensagens e transições
continuam bloqueando snapshots instáveis. A retomada reconstitui os empréstimos
e seleciona o mapa correto. Apenas o estado necessário foi adaptado.

## Validação

Linux Debug: seleções CTest **13/13** e **8/8** aprovadas, **16 casos
identificáveis distintos** entre unidade e percursos. O wrapper final também
passou, com três aplicações novas retomando encomenda pendente, interior com
máquinas/produto na mão e fazenda concluída. Linux Release: build, unidade e
percurso completos aprovados. Windows MinGW: build aprovado; Wine: unidade,
percurso completo e retomadas pendente/interior aprovados, com avisos do driver
Mesa/EDID e áudio wasapi, mas exit 0 nos checks finais. Windows físico não foi
validado. A suíte inteira não foi executada.

Contagem conservadora da rodada: **50 processos**, incluindo CTest, wrappers,
filhos e tentativas (Wine contado como lançador + programa, inclusive a tentativa
com nome de executável inválido). Distribuição: 2 unidades iniciais, 1 percurso
inicial, 14 da primeira seleção, 2 tentativas do retorno antecipado à forja,
14 da seleção final, 2 Release, 10 Windows/Wine com tentativas e 5 do wrapper
final. Não repetir a suíte nesta rodada.

Reprodução, escolhendo apenas os checks pertinentes ao orçamento da rodada:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_makers(_playthrough)?$'
```

`native_makers` executa uma unidade; `native_makers_playthrough` executa o
wrapper e quatro filhos (percurso + três apps novos). Somando CTest, a seleção
acima consome **7 processos**. Capturas opcionais fora do Git, diretório já criado:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  build/native-debug/port/hmbtn_makers_playthrough data /tmp/makers.hms /tmp/makers-shots
```

Logs desta rodada em `/tmp/hmbtn-makers-{regression,final-debug,process-final}.log`,
`/tmp/hmbtn-makers-release-playthrough.log` e `/tmp/hmbtn-makers-wine-*.log`.
Capturas de referência em `/tmp/hmbtn-makers-shots/` foram inspecionadas para
menus/confirmações e máquinas/produtos; não são distribuídas com o repositório.

`makers_test.c` cobre sequência/posse/ampliações, material acessível, saldo,
cancelamento nas duas confirmações, consumo único, guarda de encomenda, cinco
manhãs com observador idempotente, save/rebind, capacidade, variantes sazonais,
13 entradas de qualidade, mochila cheia, limites/direção, falta de ingrediente,
bloqueio de save durante ANI e valores de remessa.

`makers_playthrough.c` escava três adamantitas, faz encomendas no script/menus
instalados, cancela, confirma e confere a forja fechada antes do prazo. Dorme pelo caminho comum,
recebe cada entrega no script 354, usa zonas/ANI/modelos e remete 12 qualidades.
Retoma snapshots pendentes/concluídos. Ampliações físicas, saldo, energia e
matérias-primas dos vetores de qualidade são fixtures explícitas; adamantita e
máquinas são adquiridas. Loads diretos encurtam deslocamentos não relacionados.
`makers_process.py` abre snapshots em novos processos da aplicação.

## Continuação implementada

As melhorias de ferramentas foram integradas posteriormente nesta fila:
[PF06_TOOL_UPGRADES.md](PF06_TOOL_UPGRADES.md). Compartilham o pedido e as
travas de Saibara com as máquinas. Os limites abaixo descrevem a entrega original
deste recorte de máquinas.

## Limites e continuação

Presentes/acessórios e melhorias de ferramentas da forja ainda não estão
implementados; a opção Improve Tool continua informativa. Vozes/cues completos,
família, ajudantes, concursos/festivais e conteúdo restante das minas continuam
parciais. O bootstrap `--diagnostics` não executa o jogo; o runtime permanece um
porte parcial. A próxima entrega recomendada é melhoria real de ferramentas de
Saibara, usando minério, XP, dinheiro, prazo e ações existentes.
