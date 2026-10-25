# Economia autoritativa — MA-02

Entrega de 2026-10-25. Ordem, dependências e aceite no [ROADMAP.md](ROADMAP.md);
diretrizes em [PORTING_STRATEGY.md](PORTING_STRATEGY.md) e correspondências em
[PORTING_MATRIX.md](PORTING_MATRIX.md). Na entrega MA-02, a próxima tarefa era MA-03; sua entrega posterior está em
[CALENDAR_STATE.md](CALENDAR_STATE.md).
Esta etapa transferiu propriedade, preservando os algoritmos financeiros atuais.

Atualização MA-04a: esses dados agora pertencem a `HmbtnSession.game`,
com campo/moradores/galinhas também na sessão. A cena só mantém a referência
a esse proprietário. Execução de eventos ainda é MA-04b. Detalhes em
[SESSION_STATE.md](SESSION_STATE.md); o restante deste documento registra sua
entrega original.

## Proprietário e interfaces

`port/game/game_state.h` define `HmbtnEconomy`, pertencente exclusivamente a
`HmbtnGameState.economy`, com:

- `gold`: carteira;
- `shipments[25]`: contadores acumulados de produtos enviados;
- `ledger[4][2]`: receitas/despesas da estação atual e três anteriores;
- `shipping`: valor pendente, quantidade de peixes enviados, coleta realizada,
  existência de item enviado e bônus do mel (`HmbtnFarmShipping`).

`hmbtn_game_state_init` instala o saldo atual de 500 G, os outros dados
financeiros zerados e os defaults de inventário. Só a criação do novo runtime
chama essa função. `hmbtn_game_economy`/`hmbtn_game_economy_const` fornecem acesso
ao mesmo objeto, sem alocar, copiar ou sincronizar registros. Cada cena possui
seu próprio estado; a transferência desse proprietário para sessão é MA-04.

`HmbtnFarmMenu` recebe `const HmbtnEconomy *` em `hmbtn_farm_menu_init`.
Recriar o menu reinicializa sua interface e estatísticas não financeiras atuais,
mas não carteira, contas ou remessas. Seu tempo de vida deve estar contido no do
estado emprestado. Foram removidos `gold`, `shipments` e `ledger` de
`HmbtnFarmStatistics` e o membro `shipping` separado de `HmbtnScenePreview`.
Não restam espelhos financeiros ativos em produção; cópias nos testes são apenas
valores esperados.

| Consumidor | Acesso após MA-02 |
|---|---|
| `port/game/farm_menu.c` e `port/graphics/farm_menu_ui.c` | Referência const à economia para saldo, contadores e livro-caixa. |
| `port/game/feed_shop.c`: `hmbtn_feed_shop_tick` | Recebe a economia central para débitos/créditos/contas; continua recebendo inventário via casa e estatísticas não financeiras para contagem de animais. |
| `port/graphics/feed_shop_ui.c`: `hmbtn_feed_shop_ui_draw` | Recebe `const HmbtnEconomy *`, sem depender de estatísticas do menu para mostrar dinheiro. |
| `port/game/farm_shipping.c`: `hmbtn_farm_shipping_add`/`hmbtn_farm_shipping_clock` | Recebem um único `HmbtnEconomy *`; carteira, contas e remessa não podem ser combinadas de proprietários diferentes na chamada. |
| `port/game/farm_carry.c`: `hmbtn_farm_carry_flight` | Entrega o item à remessa central após a colisão válida; sem dependência financeira do menu. |
| `port/graphics/scene_preview.c`: `visit_command`/`world_clock_events` | Recompensa da torta, coleta, bônus/contadores e gatilho de 200 peixes usam `game.economy`. |

A remoção do include transitivo de menu em `farm_shipping.h` exigiu declarar
includes reais: `farm_field.h` na implementação de shipping, `farm_action.h`
em `poultry.h` e `farm_menu.h` nos testes que chamam troca/ciclo de inventário.
Não se alteraram regras de animais ou inventário.

## Origem e comportamento preservado

Referência original na revisão `4402de2`:
`src/main/game/farm_state.h` (`FarmState.player`),
`src/overlay/11_GF_FARM/people/player.h` (`FarmPlayer.gold`),
`src/overlay/11_GF_FARM/estate.h` (`FarmLedger`),
`src/overlay/11_GF_FARM/menu/shop_dialogues.h` (`FarmShipment`),
`src/overlay/11_GF_FARM/estate.c` (`estate_gold_change`, `estate_shipment_add`,
`estate_shipment_settle`) e callbacks em `world/clock.c` do mesmo overlay.
Essas fontes são referência; esta entrega não compila novas funções de `src/`.

Permanecem os preços/tabelas, contadores limitados a 999, bônus do mel, teto
de 9.999.999 G e registro da receita integral mesmo no teto. Remessas só recebem
itens de preço positivo após aterrissagem válida; pagamento continua às 17h58,
com as mesmas datas especiais e proteção contra segunda coleta. O reset da
coleta pela manhã, rollover do ledger às 05h59 do dia 29 e ordem dos callbacks
continuam iguais. Compras/vendas/medicina e recompensa conservam os mesmos
predicados, cancelamentos e ordem de efeitos.

A aritmética financeira ainda está nos consumidores existentes. Consolidar
operações com `estate_gold_change` e demais algoritmos originais é MA-16;
esta etapa não introduziu um segundo serviço de transações nem alterou regras.
Na entrega MA-02, nomes/calendário e `sync_clock` aguardavam MA-03; essa
migração já foi feita. Contadores não financeiros do menu, coordenação da cena
e UI continuam pendências das tarefas seguintes.

## Ciclo de vida e persistência

Carregar fazenda, casa, galinheiro e lojas não inicializa economia. Recriar menu
preserva inclusive o pagamento já realizado e remessas adicionadas após o
horário da coleta. Dormir usa os callbacks existentes, preservando registros e
aplicando apenas os efeitos financeiros normais do avanço do relógio.

O runtime ainda não grava nem restaura saves. O pedido de salvar ao dormir
continua sem arquivo e a tentativa de carregar continua no limite explícito.
Os testes garantem preservação em memória nesses caminhos; não demonstram
persistência em disco nem reabertura de uma partida salva. Instalação pelo BIN,
recursos, SDL3, renderização e áudio foram preservados. Nada em `src/`, `sdk/`,
`disc/` ou `tools/` foi modificado nesta etapa.

## Validação

Linux x86-64, Debug, SDL3 3.4.18 local e recursos instalados em `data/`:

- `cmake --preset native-debug`: sucesso, com os avisos opcionais de dependências
  SDL já registrados na referência inicial (incluindo libdecor ausente).
- `cmake --build --preset native-debug`: sucesso, incluindo runtime e testes.
- `ctest --preset native-debug --output-on-failure`: **68/68 aprovados**, zero
  falhas, 44,17 s: 45 testes independentes de `HMBTN_TEST_GAME_DATA` e 23 percursos
  opcionais com os recursos locais. Sem o parâmetro, a cobertura é menor.
- `git diff --check -- port docs/port`: aprovado.

`native_economy_state` testa defaults, acessores, dois jogos, dois menus e duas
lojas com a mesma carteira, cancelamento, débito único, contas, remessa pendente,
coleta única, recriação de UI e pagamento no dia seguinte. Compara campos, sem
comparar padding ou ponteiros como conteúdo do estado.

`native_economy_playthrough` usa recursos instalados e os caminhos reais de
carregamento de áreas, recompensa de script, compra e coleta. Renderiza saldo,
contadores e ledger com o mesmo tick visual antes/depois de alterações para
comprovar consulta do valor vivo; verifica todos os campos financeiros antes e
depois das consultas. Cobre manhã, limites de salvar/carregar e cenas independentes.
Usa provisoriamente o acesso privado a `scene_preview.c`, como os drivers
existentes; substituir por API pública da sessão é MA-08.

Continuam aprovados os testes anteriores de shipping (incluindo datas especiais,
rollover, bônus, limites e colisões), lojas/escassez, compras/vendas de galinhas,
medicina, torta, pesca, inventário e relógio. Não foram executados nesta entrega:
Windows real/cross-build, Release, Android, instalação limpa, inspeção manual ou
ASan/UBSan. A tentativa de sanitizers em MA-01 falhou por runtimes ausentes e não
foi repetida. Testes aprovados não demonstram jogo completo ou paridade integral.

## Arquivos alterados nesta entrega

A lista abaixo corresponde somente à MA-02, comparada ao início desta etapa;
alterações anteriores não relacionadas foram preservadas. São 41 arquivos em
`port/` (incluindo três novos de testes) e sete documentos (um novo).

```text
docs/port/ARCHITECTURE.md
docs/port/ECONOMY_STATE.md
docs/port/PORTING_MATRIX.md
docs/port/PORTING_STRATEGY.md
docs/port/ROADMAP.md
docs/port/ROADMAP_PENDENTE_CODEX.md
docs/port/ROADMAP_STATUS.md
port/CMakeLists.txt
port/game/farm_carry.c
port/game/farm_carry.h
port/game/farm_menu.c
port/game/farm_menu.h
port/game/farm_shipping.c
port/game/farm_shipping.h
port/game/feed_shop.c
port/game/feed_shop.h
port/game/game_state.c
port/game/game_state.h
port/game/poultry.h
port/graphics/farm_menu_ui.c
port/graphics/feed_shop_ui.c
port/graphics/feed_shop_ui.h
port/graphics/scene_preview.c
port/tests/economy_checks.h
port/tests/economy_playthrough.c
port/tests/economy_state_test.c
port/tests/farm_carry_test.c
port/tests/farm_interiors_playthrough.c
port/tests/farm_menu_test.c
port/tests/farm_shipping_test.c
port/tests/feed_shop_playthrough.c
port/tests/feed_shop_test.c
port/tests/fishing_playthrough.c
port/tests/fishing_test.c
port/tests/inventory_playthrough.c
port/tests/inventory_state_test.c
port/tests/pie_quest_playthrough.c
port/tests/poultry_carry_test.c
port/tests/poultry_helpers.h
port/tests/poultry_medicine_playthrough.c
port/tests/poultry_playthrough.c
port/tests/poultry_status_playthrough.c
port/tests/poultry_test.c
port/tests/poultry_transport_playthrough.c
port/tests/shipping_playthrough.c
port/tests/shop_shortage_playthrough.c
port/tests/world_clock_playthrough.c
port/tests/world_map_playthrough.c
```
