# Inventário autoritativo — MA-01

Entrega de 2026-10-25. Critérios e sequência em [ROADMAP.md](ROADMAP.md).
Este relatório registra MA-01: naquele momento somente o inventário havia sido
migrado. As entregas posteriores estão em [ECONOMY_STATE.md](ECONOMY_STATE.md)
e [CALENDAR_STATE.md](CALENDAR_STATE.md).
Referência de origem: árvore em `4402de2` com alterações locais anteriores;
essas alterações foram preservadas. `src/` não foi modificado.

Atualização MA-04a: esses dados agora pertencem a `HmbtnSession.game`,
com campo/moradores/galinhas também na sessão. A cena só mantém a referência
a esse proprietário. Execução de eventos ainda é MA-04b. Detalhes em
[SESSION_STATE.md](SESSION_STATE.md); o restante deste documento registra sua
entrega original.

## Proprietário e dados migrados

`port/game/game_state.h` define `HmbtnGameState`, que possui um único
`HmbtnInventory`. Neste recorte cada `HmbtnScenePreview` possui seu estado;
a extração do proprietário para uma sessão independente continua em MA-04.
Os tipos de domínio não dependem de SDL nem de headers de interface.

| Dado | Armazenamento autoritativo |
|---|---|
| Equipamento | `HmbtnGameState.inventory.equipped` |
| Mochila de ferramentas e produtos | `inventory.bag[8]` e `inventory.items[8]` |
| Mão | `inventory.held_item` |
| Baú de ferramentas | `inventory.toolbox[64]` |
| Capacidade | `inventory.rucksack_level`, preservando níveis 0/1/2 e 2/4/8 slots acessíveis |
| Suprimentos | `inventory.supplies[4]`: forragem, madeira, ração de galinhas e alimento de peixe |

`HmbtnStoredTool` mantém `id`, `modifier` e `count` de 16 bits. O nome legado do
tipo foi conservado para não misturar renomeação com mudança de propriedade.
Não houve troca de regras de empilhamento, contagem, economia ou RNG.
`FarmStoredItem` em `src/overlay/11_GF_FARM/menu/item_storage.h` e os registros de
jogador/fazenda são a referência conceitual; o formato binário PS1 não foi copiado.

## Interfaces e consumidores

- `hmbtn_game_state_init` inicializa o jogo novo uma vez, na criação da cena.
  Mantém exatamente os defaults host anteriores: ferramentas 1/11/6/16/21 nas
  cinco primeiras posições do baú, modificadores/contagens zero, demais registros,
  nível e suprimentos zero. Não importar novas regras do original nesta etapa.
- Na entrega MA-01, `hmbtn_house_interaction_init(house, inventory)` inicializava somente a interface
  e guardava uma referência emprestada ao inventário, sem resetar seus dados.
  Desde MA-03 recebe `house, state` para vincular também relógio e cadastro,
  preservando a mesma garantia sobre o inventário.
- `hmbtn_game_inventory`/`_const` expõem o inventário do estado; seleção de slots
  mutável/const agora fica em `game_state.c`, conservando a numeração existente.
- `hmbtn_inventory_swap`, `hmbtn_inventory_cycle` e `hmbtn_farm_menu_tick` recebem
  `HmbtnInventory *`. As regras atuais continuam em `farm_menu.c`, aguardando MA-15.
- Consultas/remoção/apresentação de itens e gatilhos de visitas recebem o tipo
  de inventário. `hmbtn_event_items_count` usa a mesma travessia de registros do
  comando existente, sem clonar o inventário ou converter const em escrita.
- Conversas guardam referência direta ao inventário. Casa, ferramentas, coleta,
  depósitos/cochos, lojas, pesca e galinhas usam os mesmos registros. Consumidores
  que também precisam de hora/clima ainda recebem contexto da casa; isso não
  transfere a propriedade do inventário para a UI.
- `hmbtn_house_expand_item_reading` recebe o item selecionado somente para leitura.
  A descrição não copia a casa nem substitui temporariamente o equipamento.

Há um checkpoint em `HmbtnVillageTalk.saved_inventory` para rollback de scripts
não suportados. Ele não é consultado como estado de gameplay, não é sincronizado
continuamente e não constitui inventário ativo paralelo. O escopo de rollback
permanece o anterior: equipamento, mão, mochilas e baú, além dos outros efeitos
já revertidos pela conversa. Capacidade/suprimentos não ganharam rollback novo.
Registros temporários de item salvo durante uma ação e itens transferidos para
voo continuam necessários ao comportamento existente.

## Inicialização, áreas, sono e saves

O único chamado de produção a `hmbtn_game_state_init` está na criação da cena.
`load_area`, troca de interfaces e `morning` não reinicializam nem copiam o
inventário. Novas cenas independentes recebem armazenamento independente.
Os snapshots dos testes antigos foram atualizados para capturar os dados,
pois copiar somente `HmbtnHouseInteraction` passaria a copiar um ponteiro.

A transição do cadastro para a abertura continua criando um jogo novo; antes
dela não existia inventário jogável a transportar. Nomes/cadastro e valores
iniciais continuam no fluxo existente. Não há serialização nativa de inventário
em disco a migrar: save/load permanecem ausentes e explicitamente sinalizados.
Dormir com pedido de salvar ainda avança para a manhã sem escrever um save;
o diário/load continua no limite não implementado, sem ler ou resetar os dados.
Nenhum ponteiro host passou a ser serializado.

Leitores de recursos, instalação pelo BIN, áudio/renderização e pacotes não
foram substituídos. Não houve alteração nos proprietários de carteira, contas,
remessas, calendário ou no RNG: são etapas posteriores.

## Validação

- `cmake --preset native-debug`: aprovado, com SDL3 3.4.18 local.
- `cmake --build --preset native-debug`: aprovado, incluindo reconfiguração para
  registrar os testes novos.
- `ctest --preset native-debug`: **66/66 aprovados**, zero falhas, 43,48 s.
  Cache com `HMBTN_TEST_GAME_DATA=data/`: 22 percursos opcionais e 44 demais testes.
- Os 64 testes anteriores passaram após adaptação dos consumidores; a suíte
  final inclui esses mesmos testes e os dois novos, sem remover cobertura.
- `native_inventory_state`: valores iniciais, slots/capacidades, duas instâncias,
  duas interfaces compartilhando o mesmo inventário, toolbox/menu/scripts,
  consulta sem mutação, rollback, depósito/suprimentos/mão, conclusão de ferramenta,
  descrição de item e recriação/revinculação de UI. Compara valores, não padding.
- `native_inventory_playthrough`: carregamentos reais de casa/fazenda/galinheiro/
  vila, reabertura de interfaces, manhã, limites de save/load e cenas independentes
  usando recursos instalados. As passagens físicas continuam protegidas pelos
  percursos existentes de mapa/interiores/animais.

ASan/UBSan foram tentados em build separado, mas a configuração parou no teste
do compilador: faltam `libasan.so.8.0.0`/`libubsan.so.1.0.0` e seus arquivos
estáticos. Portanto não houve execução instrumentada. Windows real, cross-build
(toolchain MinGW não disponível no PATH), Release, Android, janela/áudio manual e
instalação limpa não foram executados nesta entrega. Sucesso de CTest não comprova
jogo completo nem fidelidade integral ao original.

## Arquivos desta entrega

Lista em relação à árvore existente antes da MA-01, incluindo arquivos novos;
não representa todas as modificações locais anteriores mostradas pelo Git.
Os consumidores/testes existentes receberam migração de acessos, vinculação
explícita do estado e snapshots de dados quando necessário.

### Estado e gameplay

- [port/game/event_item.c](../../port/game/event_item.c)
- [port/game/event_item.h](../../port/game/event_item.h)
- [port/game/farm_action.c](../../port/game/farm_action.c)
- [port/game/farm_carry.c](../../port/game/farm_carry.c)
- [port/game/farm_feed.c](../../port/game/farm_feed.c)
- [port/game/farm_gather.c](../../port/game/farm_gather.c)
- [port/game/farm_menu.c](../../port/game/farm_menu.c)
- [port/game/farm_menu.h](../../port/game/farm_menu.h)
- [port/game/farm_visit.c](../../port/game/farm_visit.c)
- [port/game/farm_visit.h](../../port/game/farm_visit.h)
- [port/game/feed_shop.c](../../port/game/feed_shop.c)
- [port/game/fishing.c](../../port/game/fishing.c)
- [port/game/game_state.c](../../port/game/game_state.c)
- [port/game/game_state.h](../../port/game/game_state.h)
- [port/game/house_interaction.c](../../port/game/house_interaction.c)
- [port/game/house_interaction.h](../../port/game/house_interaction.h)
- [port/game/poultry.c](../../port/game/poultry.c)
- [port/game/poultry_carry.c](../../port/game/poultry_carry.c)
- [port/game/village_talk.c](../../port/game/village_talk.c)
- [port/game/village_talk.h](../../port/game/village_talk.h)

### Apresentação e integração

- [port/graphics/farm_menu_ui.c](../../port/graphics/farm_menu_ui.c)
- [port/graphics/house_ui.c](../../port/graphics/house_ui.c)
- [port/graphics/scene_preview.c](../../port/graphics/scene_preview.c)

### Testes

- [port/tests/event_item_test.c](../../port/tests/event_item_test.c)
- [port/tests/event_tool_store_test.c](../../port/tests/event_tool_store_test.c)
- [port/tests/farm_carry_test.c](../../port/tests/farm_carry_test.c)
- [port/tests/farm_feed_playthrough.c](../../port/tests/farm_feed_playthrough.c)
- [port/tests/farm_feed_test.c](../../port/tests/farm_feed_test.c)
- [port/tests/farm_field_test.c](../../port/tests/farm_field_test.c)
- [port/tests/farm_gather_test.c](../../port/tests/farm_gather_test.c)
- [port/tests/farm_interiors_playthrough.c](../../port/tests/farm_interiors_playthrough.c)
- [port/tests/farm_menu_test.c](../../port/tests/farm_menu_test.c)
- [port/tests/farm_visit_playthrough.c](../../port/tests/farm_visit_playthrough.c)
- [port/tests/farm_visit_test.c](../../port/tests/farm_visit_test.c)
- [port/tests/feed_shop_playthrough.c](../../port/tests/feed_shop_playthrough.c)
- [port/tests/feed_shop_test.c](../../port/tests/feed_shop_test.c)
- [port/tests/first_morning_playthrough.c](../../port/tests/first_morning_playthrough.c)
- [port/tests/fishing_playthrough.c](../../port/tests/fishing_playthrough.c)
- [port/tests/fishing_test.c](../../port/tests/fishing_test.c)
- [port/tests/greg_rod_playthrough.c](../../port/tests/greg_rod_playthrough.c)
- [port/tests/house_interaction_test.c](../../port/tests/house_interaction_test.c)
- [port/tests/ingredient_delivery_playthrough.c](../../port/tests/ingredient_delivery_playthrough.c)
- [port/tests/inventory_checks.h](../../port/tests/inventory_checks.h)
- [port/tests/inventory_playthrough.c](../../port/tests/inventory_playthrough.c)
- [port/tests/inventory_state_test.c](../../port/tests/inventory_state_test.c)
- [port/tests/pie_quest_playthrough.c](../../port/tests/pie_quest_playthrough.c)
- [port/tests/poultry_carry_test.c](../../port/tests/poultry_carry_test.c)
- [port/tests/poultry_helpers.h](../../port/tests/poultry_helpers.h)
- [port/tests/poultry_lifecycle_playthrough.c](../../port/tests/poultry_lifecycle_playthrough.c)
- [port/tests/poultry_medicine_playthrough.c](../../port/tests/poultry_medicine_playthrough.c)
- [port/tests/poultry_playthrough.c](../../port/tests/poultry_playthrough.c)
- [port/tests/poultry_status_playthrough.c](../../port/tests/poultry_status_playthrough.c)
- [port/tests/poultry_test.c](../../port/tests/poultry_test.c)
- [port/tests/poultry_transport_playthrough.c](../../port/tests/poultry_transport_playthrough.c)
- [port/tests/shipping_playthrough.c](../../port/tests/shipping_playthrough.c)
- [port/tests/village_gifts_playthrough.c](../../port/tests/village_gifts_playthrough.c)
- [port/tests/village_talk_test.c](../../port/tests/village_talk_test.c)
- [port/tests/world_clock_playthrough.c](../../port/tests/world_clock_playthrough.c)
- [port/tests/world_map_playthrough.c](../../port/tests/world_map_playthrough.c)

### Documentação

- [docs/port/ARCHITECTURE.md](../../docs/port/ARCHITECTURE.md)
- [docs/port/INVENTORY_STATE.md](../../docs/port/INVENTORY_STATE.md)
- [docs/port/PORTING_MATRIX.md](../../docs/port/PORTING_MATRIX.md)
- [docs/port/PORTING_STRATEGY.md](../../docs/port/PORTING_STRATEGY.md)
- [docs/port/ROADMAP.md](../../docs/port/ROADMAP.md)
- [docs/port/ROADMAP_PENDENTE_CODEX.md](../../docs/port/ROADMAP_PENDENTE_CODEX.md)
- [docs/port/ROADMAP_STATUS.md](../../docs/port/ROADMAP_STATUS.md)

### Build

- [port/CMakeLists.txt](../../port/CMakeLists.txt)

