# Calendário e cadastro autoritativos — MA-03

Entrega de 2026-10-25. Ordem, dependências e critérios no [ROADMAP.md](ROADMAP.md),
diretrizes em [PORTING_STRATEGY.md](PORTING_STRATEGY.md) e correspondências em
[PORTING_MATRIX.md](PORTING_MATRIX.md). MA-03 transfere propriedade, preservando
algoritmos. A próxima implementação é MA-04, ainda não iniciada.

Atualização MA-04a: esses dados agora pertencem a `HmbtnSession.game`,
com campo/moradores/galinhas também na sessão. A cena só mantém a referência
a esse proprietário. Execução de eventos ainda é MA-04b. Detalhes em
[SESSION_STATE.md](SESSION_STATE.md); o restante deste documento registra sua
entrega original.

## Dados e propriedade

`port/game/game_state.h` agora possui:

| Dado | Origem única |
|---|---|
| Ano, estação, dia, dia da semana, hora, minuto e frames até o minuto seguinte | `HmbtnGameState.clock`, com o `HmbtnFarmClock` existente. |
| Nomes de jogador, fazenda e cachorro | `HmbtnGameState.registration`, tipo `HmbtnRegistration`; oito glifos de 16 bits mais terminador. |
| Aniversário do jogador | `registration.birthday_season` e `registration.birthday_day`, zero-based. |

O relógio separado de `HmbtnScenePreview`, os seis campos de data/hora de
`HmbtnHouseInteraction` e `sync_clock` foram removidos. Não há sincronização de
uma segunda data atual. `hmbtn_game_state_init` chama os inicializadores de
relógio/cadastro somente ao criar um novo estado: ano/estação 0, dia/weekday 1,
06h00, frames 0; aniversário 0/0; nomes preenchidos com `0xffff`. São os mesmos
defaults host anteriores. Inventário e economia conservam seus defaults.

`hmbtn_game_clock`/`_const` e `hmbtn_game_registration`/`_const` retornam os
objetos do estado, sem copiar ou alocar. Cada cena possui seu estado até MA-04.
Não existem arquivos de persistência nem layouts PS1 reinterpretados como
estruturas host nesta mudança.

## Interfaces compartilhadas

| Interface/consumidor | Mudança |
|---|---|
| `hmbtn_house_interaction_init(house, state)` | Vincula inventário, relógio const e cadastro const. Recriar casa/UI não reinicia o domínio. Substitui a assinatura de MA-01 que recebia somente inventário. |
| `hmbtn_house_expand_reading`/`hmbtn_house_expand_item_reading` | Buscam nomes pela referência ao cadastro da casa. Chamadores não fornecem arrays de nomes separados. |
| `house_interaction.c`, `house_ui.c`, `farm_menu_ui.c` | TV, seleção de calendário, HUD e estação do livro-caixa leem o relógio central. O cursor `calendar_day` continua escolha local da interface. |
| `farm_action.c`, `fishing.c`, `poultry.c` | Fadiga continua com os mesmos cálculos, consultando a hora pelo relógio emprestado à casa. |
| `HmbtnFeedShop.clock` | Empresta o relógio const em vez de armazenar a antiga cópia `date`; compra registra a data pelo mesmo relógio. O mundo continua pausado durante a loja. |
| `hmbtn_new_game_init(editor, registration)` | Inicializa somente editor/mensagem e vincula cadastro; não limpa nomes ou aniversário já registrados. |
| `HmbtnNewGame` | Removidos os três arrays de nomes persistentes e os antigos `season`/`day`; só conserva ponteiro para cadastro e edição transitória. |
| `village_talk.*` | Recebe `const HmbtnRegistration *`; lê aniversário central e relógio emprestado. Variáveis de eventos mantêm seus valores/conversões anteriores. |
| `feed_shop.*`, `farm_menu_ui.*`, `feed_shop_ui.*` | Textos e telas recebem o registro de domínio, sem depender de um editor como proprietário dos nomes. |
| `scene_preview.c` | Avanços, scripts, gatilhos, sono, recursos sazonais e mensagens passam a consultar `game.clock`/`game.registration`. Ordem dos efeitos preservada. |

O buffer `HmbtnNewGame.name`, `birthday_value`, escolha e fase do editor são
transitórios, não cadastros ativos paralelos. O nome só é copiado do buffer para
o registro no mesmo ponto de confirmação anterior. O aniversário conserva os
pontos de atualização/volta das fases anteriores. O editor de nomes de animais
usa `hmbtn_name_editor_begin`, sem vincular cadastro do jogador, e entrega o
buffer à rotina de animal já existente.

## Título, abertura e ciclo de vida

`title_preview.c` possui um `HmbtnGameState` provisório. Ao iniciar New Game,
inicializa esse estado e vincula seu cadastro ao editor. A UI de aniversário e
o resumo leem esse mesmo registro. `hmbtn_title_preview_registration` fornece
apenas `const HmbtnRegistration *` após a conclusão do fluxo/fade existente.

`app.c` preserva os valores em um `HmbtnRegistration` temporário, destrói o
título e libera seus recursos antes de criar a abertura. Então
`hmbtn_scene_intro_registration` transfere os valores para `scene.game.registration`.
Esse é um handoff único entre fases, não duas cópias de produção sincronizadas.
Nenhum ponteiro de editor/título é transportado para a partida; o diálogo da
cena já está vinculado ao seu próprio registro, cujo endereço não muda.

Carregar áreas e recriar casa, diálogo ou menu conservam data, frames e cadastro.
Dormir e fast-forward usam o mesmo algoritmo/callbacks existentes. Salvar ao
dormir continua sem escrever arquivo; carregar no diário continua um limite
explícito. A garantia testada é persistência em memória, não salvar/reabrir jogo.

Datas de nascimento de animais, aniversários dos próprios moradores, cache de
agenda (`date_key`), estado de iluminação e cursores são dados com significados
próprios. Não foram convertidos em aliases de uma data atual nem removidos como
se fossem calendários autoritativos duplicados. Clima/previsão ainda ficam no
contexto atual; sua migração e algoritmos completos pertencem às tarefas futuras.

## Origem e limites do recorte

Referência original: revisão `4402de2`, com alterações locais anteriores
preservadas. Tipos/rotinas relevantes:

- `src/overlay/11_GF_FARM/world/clock.h`: `FarmClock`; `clock.c`:
  `clock_advance_minute`, `clock_date_index`; `src/main/game/farm_state.h`:
  `FarmProgress.clock` e `FarmSystem.clockFrames`.
- `src/overlay/11_GF_FARM/people/player.h`: `FarmPlayer.name`/`birthday`;
  `estate.h`: `FarmEstate.farmName`; `animals/dog.h`: `FarmPet.actor`, cujo nome
  aparece em `game_state.animals.dog.actor.name` no título original.
- `src/overlay/27_GF_TITLE/title.c`: `title_sequence_step` e sequência de cadastro;
  `src/main/ui/name_entry.c`: edição/confirmação de nomes.
- `src/overlay/11_GF_FARM/menu/calendar.c`,
  `src/overlay/11_GF_FARM/event/messages.c` e `event/event_commands.c`: consultas e apresentação das datas/nomes.

Esta etapa não compila novas funções de `src/`, não troca o algoritmo de avanço,
não amplia comandos ainda ausentes e não implementa novos sistemas de clima ou
cadastro. Reaproveitamento mais direto do relógio e pipeline diário é MA-18/19;
consolidação de variáveis/comandos é MA-10..14. Sessão e separação de apresentação
continuam em MA-04..08. Backends, instalação pelo BIN e recursos foram mantidos.
Nenhum arquivo de `src/`, `sdk/`, `disc/`, `tools/` ou CMake PS1 foi alterado.

## Validação e cobertura

Linux x86-64, Debug, SDL3 3.4.18 local e recursos instalados em `data/`:

- `cmake --preset native-debug`: aprovado, com avisos opcionais de dependências
  SDL já registrados na referência inicial (incluindo libdecor ausente).
- `cmake --build --preset native-debug`: aprovado, runtime e testes.
- Os **68 testes anteriores** passaram após a migração: 44,13 s.
- `ctest --preset native-debug --output-on-failure`: **70/70 aprovados**, zero
  falhas, 44,19 s; 46 testes independentes de `HMBTN_TEST_GAME_DATA` e 24 percursos
  opcionais com os recursos locais. Sem esse parâmetro, a cobertura é menor.
- `git diff --check -- port docs/port`: aprovado.

`native_calendar_state` verifica defaults, acessores, duas interfaces/editores
com os mesmos registros, confirmação de oito glifos, recriação sem reset,
edição de animal sem alterar cadastro, leitura das oito variáveis de calendário/
aniversário por bytecode sintético, TV após avanço, cursor local do calendário,
nomes da casa e dois jogos independentes. Compara valores, sem padding/ponteiros.

`native_calendar_playthrough` verifica o handoff de cadastro por valor pela API
da abertura, descartando o objeto de origem, e usa recursos reais para carregar
áreas, renderizar nomes, horário e calendário antes/depois de alterações. As
consultas não alteram os dados. Cobre pausa/retomada no tick de exploração,
TV, sono com virada de estação/ano, leitura de hora/ano por evento, nomes em
textos, limites de save/load, inventário/economia preservados e cenas isoladas.
Usa o acesso privado a `scene_preview.c` já empregado pelos outros percursos;
substituir por API pública da sessão continua em MA-08.

Os testes existentes de `native_new_game` continuam percorrendo edição,
confirmação/cancelamento, aniversário e cadastro completo, agora verificando o
registro central. First-morning, world-clock, lojas, galinhas, pesca, presentes,
torta, inventário/economia e os demais CTests também passaram. Não foi executado
um novo percurso automatizado completo de edição dentro do título SDL; o fluxo
de edição é coberto em unidade, o preview SDL pelo teste existente e o handoff
pela integração descrita acima.

Não foram executados nesta etapa: Windows real/cross-build, Release, Android,
instalação limpa, inspeção manual de janela/áudio ou ASan/UBSan. A ausência de
runtimes que impediu sanitizers em MA-01 permanece registrada; não foi repetida
essa tentativa. Testes aprovados não demonstram jogo completo ou paridade integral.

## Arquivos alterados nesta entrega

Lista relativa à árvore no início da MA-03. Alterações anteriores não
relacionadas foram preservadas. São 67 arquivos em `port/` (três novos de testes)
e nove documentos (um novo); os testes existentes tiveram suas referências e
fixtures ajustadas aos novos proprietários.

```text
docs/port/ARCHITECTURE.md
docs/port/CALENDAR_STATE.md
docs/port/ECONOMY_STATE.md
docs/port/INVENTORY_STATE.md
docs/port/PORTING_MATRIX.md
docs/port/PORTING_STRATEGY.md
docs/port/ROADMAP.md
docs/port/ROADMAP_PENDENTE_CODEX.md
docs/port/ROADMAP_STATUS.md
port/CMakeLists.txt
port/core/app.c
port/game/farm_action.c
port/game/feed_shop.c
port/game/feed_shop.h
port/game/fishing.c
port/game/game_state.c
port/game/game_state.h
port/game/house_interaction.c
port/game/house_interaction.h
port/game/new_game.c
port/game/new_game.h
port/game/poultry.c
port/game/village_talk.c
port/game/village_talk.h
port/graphics/farm_menu_ui.c
port/graphics/farm_menu_ui.h
port/graphics/feed_shop_ui.c
port/graphics/feed_shop_ui.h
port/graphics/house_ui.c
port/graphics/new_game_ui.c
port/graphics/scene_preview.c
port/graphics/scene_preview.h
port/graphics/title_preview.c
port/graphics/title_preview.h
port/tests/calendar_checks.h
port/tests/calendar_playthrough.c
port/tests/calendar_state_test.c
port/tests/economy_playthrough.c
port/tests/economy_state_test.c
port/tests/event_tool_store_test.c
port/tests/farm_carry_test.c
port/tests/farm_feed_playthrough.c
port/tests/farm_feed_test.c
port/tests/farm_field_test.c
port/tests/farm_gather_test.c
port/tests/farm_interiors_playthrough.c
port/tests/farm_menu_test.c
port/tests/farm_visit_playthrough.c
port/tests/feed_shop_playthrough.c
port/tests/feed_shop_test.c
port/tests/first_morning_playthrough.c
port/tests/fishing_playthrough.c
port/tests/fishing_test.c
port/tests/greg_rod_playthrough.c
port/tests/house_interaction_test.c
port/tests/ingredient_delivery_playthrough.c
port/tests/inventory_playthrough.c
port/tests/inventory_state_test.c
port/tests/new_game_test.c
port/tests/pie_quest_playthrough.c
port/tests/poultry_carry_test.c
port/tests/poultry_helpers.h
port/tests/poultry_lifecycle_playthrough.c
port/tests/poultry_medicine_playthrough.c
port/tests/poultry_playthrough.c
port/tests/poultry_status_playthrough.c
port/tests/poultry_test.c
port/tests/poultry_transport_playthrough.c
port/tests/shipping_playthrough.c
port/tests/shop_shortage_playthrough.c
port/tests/village_gifts_playthrough.c
port/tests/village_playthrough.c
port/tests/village_talk_playthrough.c
port/tests/village_talk_test.c
port/tests/world_clock_playthrough.c
port/tests/world_map_playthrough.c
```
