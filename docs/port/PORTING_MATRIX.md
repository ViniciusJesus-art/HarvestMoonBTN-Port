# Matriz de reaproveitamento e migração

Inventário inicial de 2026-10-25. Ordem e aceite são definidos somente no
[ROADMAP.md](ROADMAP.md); os IDs MA abaixo remetem a ele. Diretrizes em
[PORTING_STRATEGY.md](PORTING_STRATEGY.md). MA-01..03 foram executadas para a
propriedade do inventário, economia, calendário e cadastro. MA-04a extraiu o
ciclo de vida desses dados e field/village/poultry para `HmbtnSession`; MA-04b
e as demais migrações estão **planejadas**. A matriz cobre os principais pontos de integração,
não é uma lista exaustiva de todas as funções decompiladas.

## Convenções e leitura

- `F/` = `src/overlay/11_GF_FARM/`; `P/` = `port/game/`.
- Nas implementações nativas, `core/`, `graphics/`, `audio/` e `filesystem/`
  são relativos a `port/`; nomes de testes são relativos a `port/tests/`.
  Nomes de arquivos sem diretório na mesma célula seguem o diretório do arquivo
  anterior. Caminhos iniciados por `src/` são relativos à raiz do repositório.
- **A:** adaptar algoritmo/função original em `port/`, removendo dependências.
- **D?:** candidato a compilação direta, sujeito a auditar a unidade e headers.
- **M:** manter backend ou adaptação existente, alterando sua fronteira.
- **E:** extração de propriedade/coordenação, sem trocar regras nesta etapa.

Hoje nenhum `.c` de `src/` entra nos targets nativos. Uma correspondência nesta
matriz indica origem comportamental, não identidade de implementação ou
equivalência completa. Nas linhas com funções `static`, uma adaptação rastreável
pode extrair o recorte para `port/`, sem editar ou incluir textualmente o `.c`
original. Dependências citadas são os principais obstáculos, não listas de
símbolos de link completas; inventariar os chamadores ao executar cada tarefa.

## Estado, sessão e serviços

| Origem original | Implementação/proprietário atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `src/main/game/farm_state.h`: `FarmState`; `farm_state.c`: `farm_state_init` | `HmbtnGameState.inventory`, `.economy`, `.clock` e `.registration` concluídos em MA-01..03; `HmbtnSession` agora possui esse estado e field/village/poultry (MA-04a); demais dados em `HmbtnScenePreview`, `HmbtnFarmMenu.stats` | Casa empresta inventário e menu consulta economia; demais estados espalhados; `FarmState` inclui headers PSY-Q e ponteiros. | **E/A**, MA-01..03 concluídas: tipos neutros, inventário/economia/calendário/cadastro e inicialização; MA-04a concluída, MA-04b pendente. Ver [MA-01](INVENTORY_STATE.md), [MA-02](ECONOMY_STATE.md) e [MA-03](CALENDAR_STATE.md) e [MA-04a](SESSION_STATE.md). Não copiar ABI/padding. | T-ESTADO; menus, lojas, eventos e trocas de área conservam os mesmos valores. |
| `src/main/main.c`: `main_run_frame`; `F/farm_frame.c`, `F/farm_main.c` | `core/app.c`; `graphics/scene_preview.c`: `opening_tick`, `hmbtn_scene_preview_draw` | Tick misturado com SDL, desenho, carregamento e áudio. | **E**, MA-05..08: separar contratos, tick e visão de leitura; conservar ordem existente antes de revisar o loop original. | T-SESSAO; mesmas entradas/ticks com desenho em cadências diferentes. |
| `src/main/game/event_flags.c`: `event_flag_get`, `event_flag_set` | `HmbtnVillage.flags`, leituras/escritas em vários módulos | Flags originais em bits, host em bool; acesso a `game_state` e shift com sinal a auditar. | **A/D?**, MA-09: um acesso tipado, sem exigir mudar representação e regra ao mesmo tempo. | Flags de fronteira, clear/set, índices inválidos e visibilidade entre comandos/rotinas. |
| `src/main/graphics/screen_fade.c`: `rand_range` | `core/psx_random.c`: `hmbtn_psx_random_range`; PRNG próprio em `P/poultry.c` | Estados locais, galinhas com outra fórmula; original consome RNG por frame. | **M/A**, MA-09: propriedade central com sequências atuais preservadas; decisão de sequência original fica explícita em MA-25. | Vetores em `village_affinity_test.c`, continuidade de seeds e ausência de sorteio no desenho. |
| `src/main/cd/afile.c`; tabelas de `F/farm_load.c` | `filesystem/resource_file.c`, `event_resources.c`, `action_resources.c`, `text_resources.c`; includes gerados | CD, offsets e endereços PS1 não são ponteiros host. | **M**, MA-05/08: leitores existentes atendem pedidos; não recriar CD/MIPS. | `native_resource_files`, importação, textos/ações, falha/truncamento e T-SERVICOS. |

## Eventos e comandos

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `F/event/event_script.c`: `event_block_entry`, `event_block_find_marked`, `event_script_run` | `P/event_script.c`: `hmbtn_event_block_open`, `hmbtn_event_block_entry`, `hmbtn_event_tick` | Blocos LE, cursor, flags de yield, resultado e transição; handlers incompletos. | **M/A**, MA-11: manter leitor validado e consolidar despacho sem criar outro interpretador. | `native_event_runtime`; saltos, fim, limite de instruções, retorno e espera sem repetir efeitos. |
| `F/event/event_commands.c`: `event_var_get`, `event_var_update`, `event_var_compare`, `event_var_arith`, `event_var_address` | `P/village_talk.c`: `value`, `update`, `compare`, `arithmetic`; comparações/slots em `farm_intro.c` e `farm_visit.c` | Globais, uniões de slots, ponteiros para variáveis, sentinelas, clamps e valores iniciais fixos no host. | **A**, MA-10/11: acessores tipados e algoritmos comuns; substituir endereços por referências validadas. | T-EVENTOS; signedness, overflow definido, divisão/resto extremos, índices, clamps e comparação que indica falha. |
| `F/event/event_commands.c`: `event_message_start`, `event_cmd_wait_frames`, `event_cmd_set_sprite`, `event_cmd_set_position`, `event_cmd_return` | Handlers `command` em `P/farm_intro.c`/`village_talk.c`; callbacks de `scene_preview.c` | Mensagens, atores, conclusão de animação, recursos e slots; casos presos a IDs. | **A/M**, MA-12/13: migrar famílias separadas para o mesmo despacho, usando contratos da sessão. | Abertura/infância, Thomas Sim/Não, funeral, nomeação e conversas; T-SERVICOS/T-EVENTOS. |
| `F/event/event_commands.c`: `event_triggers_fire`; `F/farm_init.c`: `farm_event_available` | `P/farm_visit.c`: `hmbtn_farm_event_trigger`, `available`, `window_match` | Datas, clima, inventário, flags e registros de área; whitelist de eventos/condições. MA-01 removeu cópia da consulta usando `hmbtn_event_items_count(const HmbtnInventory *)`. | **A**, MA-14: adaptar predicados originais e ampliar cobertura por condição comprovada. | `native_farm_visit`, visitas, torta, ingredientes; limites inclusivos, virada de dia/ano e disparo único. |
| `F/event/event_commands.c`: `event_items_take`, `event_item_can_take`, `event_item_store` | `P/event_item.c`: `hmbtn_event_items_take`; branches em `P/village_talk.c` | Inventário agora em `HmbtnGameState`, comandos usam referência direta; algoritmos de capacidade/seleção ainda aguardam MA-15. | **A**, MA-15: consultas/remoção/armazenamento comuns usados por comandos e interfaces. | `native_event_item`, `native_event_tool_store`, Greg, presentes e ingredientes; mochila cheia, slot inicial e consumo único. |

## Inventário, economia e calendário

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `F/menu/storage_swap.c`: `storage_try_swap`, `storage_stack_items`, `storage_stack_count`; `F/menu/item_storage.c`: `item_list_compact`, `item_stack_merge`; `F/people/player.c`: `player_exchange_item`, `player_exchange_tool` | `P/farm_menu.c`: `hmbtn_inventory_swap`, `hmbtn_inventory_cycle`; `house_interaction.c`, `farm_carry.c` | Slots const/mutável e tipo de item centralizados em MA-01; troca/ciclo recebem `HmbtnInventory *`, mantendo algoritmos existentes no menu. | **A**, MA-15: portar regras com estado central; preservar cursores/UI como apresentação. | `native_farm_menu`, `native_house_interaction`, `native_farm_carry`; limites 2/4/8, sementes, baú e cancelamento. |
| `F/estate.c`: `estate_gold_change`, `estate_shipment_add`, `estate_shipment_settle`, `estate_item_slot_index` | `P/farm_shipping.c`: `hmbtn_farm_shipping_add`, `hmbtn_farm_shipping_clock`; débitos/créditos em `feed_shop.c` e eventos | Carteira, ledger, 25 contadores e remessa em `HmbtnGameState.economy`; menu empresta consulta const, lojas/scripts/coleta usam o mesmo estado (MA-02 concluída). Preço, bônus e calendário conservam regras atuais. | **E** MA-02 concluída; **A**, MA-16: uma operação financeira e uma regra de remessa; gerar tabelas enquanto necessário. | `native_farm_shipping`, lojas/escassez/remessa; zero, saldo insuficiente, teto, receita integral e pagamento único às 17h58. |
| `F/menu/shop_dialogues.c`; scripts 365/368 instalados; `F/estate.c` | `P/feed_shop.c`: `hmbtn_feed_shop_open`, `hmbtn_feed_shop_tick`, compra/venda/medicamento | Horários e predicados copiados de scripts; inventário/economia/calendário/cadastro centrais em MA-01..03; loja empresta o relógio; registros de animais ainda no contexto atual. | **A/M**, MA-17: preservar UI; consumir comandos/regras comuns e predicados originais em um estabelecimento por entrega. | Loja → cocho; compra/venda/cancelamento; nomeação cancelada, medicina já possuída e ausência do vendedor. |
| `F/world/clock.c`: `clock_advance_minute`, `clock_time_period`, `clock_date_index`; `F/world/weather.c` | `P/farm_clock.c`: `hmbtn_farm_clock_tick`, `hmbtn_farm_clock_advance_notify`, `hmbtn_daylight_*`; relógio em `HmbtnGameState.clock` | MA-03 removeu `sync_clock` e campos de data/hora da casa; consumidores usam a mesma referência. Pausa, calendário 0/1-based, callbacks e luz mantêm regras atuais. | **E** MA-03 concluída; **A/M**, MA-18: adaptar avanço/agendamento original, mantendo blend no backend visual. | `native_farm_clock`, `native_world_clock_playthrough`; 06h, estação/ano, sono, pausa, avanço de vários dias e datas especiais. |

## Atualização diária, campo, animais e moradores

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `F/world/clock_events.c`: `scene_overnight_step`, `estate_ledger_roll`, `farm_hourly_refresh`, `farm_overnight_finish` | `graphics/scene_preview.c`: `world_clock_events`, `morning`; `P/farm_clock.c` | Ordem de efeitos, sono, datas, fome, campo, economia, scripts e clima; recorte incompleto. | **A/E**, MA-19: pipeline ordenado e testável; portar um callback por entrega, sem sucesso fictício para sistemas ausentes. | T-DIARIO; ração consumida antes da limpeza, produção, pagamentos e flags uma vez por dia, inclusive no sono. |
| `F/event/event_request.c`: `scene_item_queue_reset`, `scene_message_timers_tick` | `P/event_days.c`: `hmbtn_event_days_init`, `hmbtn_event_days_advance` | Contadores, flags/datas/clima e casamento; defaults gerados. | **A/M**, MA-19: conectar ao pipeline com estado central e preservar saturações. | `native_event_days`; sentinela 255, saturação 120, clima/datas e múltiplos dias. |
| `F/field/field.c`: `field_daily_tick`, `field_tile_item_stage`, `field_tile_harvest`, `field_tile_clear_tool` | `P/farm_field.c`: `hmbtn_farm_field_new_day`, `hmbtn_farm_field_tool`; `farm_action.c`; proprietário `HmbtnSession.field` (MA-04a) | Culturas/irrigação/estações, pedidos, RNG, clima e animação; atualização host só parcial. | **A**, MA-20: portar primeiro crescimento diário de uma cultura; ampliar colheita/regrowth/estações em subtarefas registradas. | `native_farm_field`, carry/feed/shipping; T-DIARIO com semear → regar → dormir → crescer → colher, seco e fora de estação. |
| `F/animals/chicken.c`: `chicken_daily_tick`, `chicken_weather_tick`, `chicken_kind_by_age`, `chicken_timer_tick` | `P/poultry.c`: `hmbtn_poultry_new_day`, `hmbtn_poultry_tick`; `poultry_carry.c`; proprietário `HmbtnSession.poultry` (MA-04a) | Ração, ovos, idade/doença, RNG host diferente, flags, cues e registro de ator. | **A**, MA-21: começar por alimentação/ovos; separar incubação/saúde/movimento, sem migrar outras espécies nessa tarefa. | Todos os percursos `native_poultry_*`; sem duplicar animal/ovo, três noites, crescimento, morte/liberação, transporte e medicina. |
| `F/people/villager.c`: `villager_schedule_advance`, `villager_update_frames`, `villager_route_step_move` | `P/village.c`: `hmbtn_village_sync`, `hmbtn_village_tick`; proprietário `HmbtnSession.village` (MA-04a); cópias de atores de eventos ainda na cena | Agendas BIN, mapa, relógio, colisões, espera/atraso e override de atores. | **A**, MA-22: portar seleção/retomada primeiro; depois movimento de uma rota, mantendo identidade. | `native_village`, percursos vila/mapa; atraso de 30 minutos, troca de área/hora, evento e retomada sem teleportar. |
| `F/people/villager.c`: `family_flags_update`, `villager_affection_decay`; `F/people/family.c` | `P/village.c`: `daily_affinity`; `village_talk.c`: presentes/aniversários/variáveis | Flags diárias, RNG, amizade/afeição; família/casamento ainda ausentes. | **A**, MA-23: portar ganho/decay já coberto; registrar fronteiras de família para tarefas futuras, sem implementá-la inteira. | `native_village_affinity`, `native_item_reaction`, presentes; clamps, negligência, exceções, ganho único e aniversário alternativo. |
| `F/field/fishing.c`: `fishing_bite_wait`, `fishing_roll_catch`, `fishing_player_frame`; `F/people/player.c` | `P/fishing.c`: `hmbtn_fishing_wait`, `hmbtn_fishing_roll`, `hmbtn_fishing_tick` | Calendário, flags de captura, inventário, ação/animação, chão e RNG próprio. | **A/M**, MA-24: algoritmos de espera/captura sobre serviços comuns; manter renderização/sons existentes. | `native_fishing`, Greg e pesca; captura única, berry, estampas, requisitos de peixes, cancelamento e bolsa cheia. |

## Recortes de apresentação e backends preservados

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `src/overlay/27_GF_TITLE/title.c`: `title_sequence_step`, `title_fill_field_layout`, `title_player_reset_for_new_game`; `src/main/ui/name_entry.c` | `P/title_flow.c`, `new_game.c`; `graphics/title_preview.c`, `new_game_ui.c` | MA-03 separou `HmbtnRegistration` do editor; título/abertura usam estado central e transferência única de valores. Mensagens, desenho e loads ainda coordenados na apresentação. | **E** MA-03 concluída; **M/A**, MA-08 e MA-25: conservar fluxo e extrair tick/serviços sem reescrever UI. | `native_title_flow`, `native_new_game`, `native_title_preview`, abertura completa e nomes retidos. |
| `F/people/player.c`: `player_update_mode`, `player_update_position`, `player_apply_tool_target`; `src/main/geometry/ground.c`: `ground_find_object`, `ground_height_at` | `P/house_control.c`, `farm_action.c`, `ground.c` | Estados, geometria e conclusão de animação; GTE e tipos do SDK na origem. | **M/A**, MA-06/08 e MA-25: manter consultas nativas; extrair coordenação e depois revisar algoritmos por ação. | Controle/campo/carry, portas, escadas, velocidade pelo modo anterior, menus e efeito da ferramenta sem desenho. |
| `src/main/geometry/model.c`, `src/main/graphics/texture_pack.c`, `src/main/graphics/anim.c`; `src/main/sound/` | `graphics/model_decode.c`, `model_render.c`, `asset_decode.c`; `audio/psx_audio.c`, `xa_*` | GPU/GTE/SPU/CD e tipos PSY-Q; backend nativo já funcional, fidelidade parcial. | **M**, MA-05..08: preservar backend e expor contratos mínimos. Não implementar camada completa de SDK. | Modelos, assets, áudio, XA, screenshots sintéticos, T-SESSAO/T-SERVICOS e comparação visual/auditiva posterior. |

## Testes novos exigidos pela migração

Os nomes T abaixo são grupos de requisitos. T-ESTADO do inventário tem cobertura
em `native_inventory_state`/`native_inventory_playthrough` desde MA-01 e da economia
em `native_economy_state`/`native_economy_playthrough` desde MA-02. Calendário/cadastro têm cobertura em `native_calendar_state`/`native_calendar_playthrough`
desde MA-03. As telas financeiras, de calendário e nomes têm consultas testadas
sem mutação. MA-04a acrescentou `native_session_state` (sem SDL) e
`native_session_playthrough` (recursos locais) para ciclo de vida, UI, empréstimos
e registro de animal entre áreas. O restante continua planejado, não
corresponde a CTests já implementados.
Usar os CTests existentes citados na matriz como proteção adicional.

| Grupo | Casos e critério verificável | Introdução |
|---|---|---|
| T-ESTADO | Duas sessões não compartilham inventário/carteira/calendário/cadastro; menu/casa/script acessam o mesmo dado; recriar UI e atravessar áreas não o reinicializa. Testar valores, não `memcmp` de structs. | MA-01..04 |
| T-SESSAO | Mesmas entradas e número de ticks produzem o mesmo estado a 0/30/60 apresentações; desenhar repetidamente não altera mundo; pausa congela as mesmas ações; tick não exige SDL, vídeo ou dispositivo de áudio. | MA-05..08 |
| T-SERVICOS | Recursos/animações/mensagens atrasados, recusados ou concluídos uma vez; polling repetido não duplica item, débito ou avanço de cursor; cancelamento e erro preservam propriedade dos buffers. | MA-05/06/12/13 |
| T-EVENTOS | Blocos/operandos inválidos, saltos, yield, PENDING, retorno/subrotina, término e unsupported; mesmos comandos têm efeito igual em conversa, abertura e visita. Vetores derivados dos handlers originais para aritmética/variáveis. | MA-10..14 |
| T-DIARIO | Minutos atravessando 06h/estação/ano, sono longo, pausa, flags diárias; ração → alimentação/ovos → limpeza; carteira/contas, crescimento e morte atualizados uma única vez na ordem revisada do original. | MA-18..23 |

## Uso e manutenção da matriz

Cada entrega deve atualizar sua linha com funções efetivamente migradas,
dependências removidas, limitações e resultados de teste. Referências ao código
original exigem revisão/identificador de origem na implementação, especialmente
quando um arquivo upstream muda. As tabelas geradas seguem válidas durante a
migração; substituí-las por outra forma de compilação só quando houver benefício
demonstrado e testes de conteúdo.

Os testes com dados reais não são automaticamente executados em CI: dependem
de `HMBTN_TEST_GAME_DATA`, sem recursos proprietários no Git. A referência local
e o que ainda não foi validado em Windows estão no roadmap. Não marcar uma
linha como migrada só porque seu teste nativo antigo passou.
