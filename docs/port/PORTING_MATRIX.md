# Matriz de reaproveitamento e migração

Inventário inicial de 2026-10-25. Ordem e aceite são definidos somente no
[ROADMAP.md](ROADMAP.md). Os IDs MA abaixo são referências de escopo do
plano anterior, não dependências sequenciais ativas; o roadmap contém a
correspondência para as entregas PF. MA-07..25 foram substituídas, sem conclusão. Diretrizes em
[PORTING_STRATEGY.md](PORTING_STRATEGY.md). MA-01..03 foram executadas para a
propriedade do inventário, economia, calendário e cadastro. MA-04a extraiu o
ciclo de vida desses dados e field/village/poultry para `HmbtnSession`; MA-04b
transferiu execução/atores e explicitou a retomada de rotas. MA-05 concluiu os
contratos mínimos de entrada, recursos, mensagens, animação e áudio. MA-06
extraiu o tick de exploração livre, sem depender do desenho. A entrega
PF-01 concluiu as regras compartilhadas de inventário/dinheiro e PF-02 integrou
o primeiro cultivo completo/ciclo diário. PF-03 compartilhou comandos e integrou
Duke; PF-04 consolidou o tick e separou atualização/desenho. PF-05 entregou saves
nativos do estado estável. PF-06 integrou batata, grama/feno, culturas base de verão/outono, clima diário/chuva, sementes sazonais de Won, desbloqueios do supermercado, Pink Cat/Toy Flower e variantes de Magic Red; Moondrop e visitas de Karen também integradas; carpintaria/galinheiro também integrados; próximo recorte: primeira casa na cadeia da estufa. Nenhuma depende de terminar o interpretador inteiro. A matriz cobre os principais pontos de integração,
não é uma lista exaustiva de todas as funções decompiladas.

## Prioridade funcional e extensibilidade

| Entrega ativa | Linhas desta matriz usadas na implementação |
|---|---|
| PF-01 | `event_items_take`/`event_item_store`, armazenamento/troca e `estate_gold_change`; integrar operações já existentes sem reescrever equivalentes. Remessa atual é consumidora da operação financeira; ampliar regras de remessa conforme o percurso PF-02. |
| PF-02 | `clock_advance_minute`, `clock_events.c`, `event_request.c` e `field_daily_tick`; preservar os consumidores de galinhas e economia. |
| PF-03 | `event_script.c`, `event_commands.c`, flags e gatilhos, por evento e suas dependências reais. |
| PF-04 | Coordenação `main_run_frame`/cena, título, roteiros e transições; backends existentes preservados. |
| PF-05 | Concluída para exploração estável: `P/save.c`, `filesystem/save_file.c`, cena candidata e Continue/diário; formato nativo v1, sem layout PS1. |
| PF-06 | Batata, grama/feno e culturas base de verão/outono entregues usando motor/catálogo PF-02 e estados autoritativos; próximo: clima diário/chuva no campo. Galinhas, moradores, relacionamentos, pesca e demais sistemas conforme a funcionalidade selecionada no roadmap. |
| PF-07 | Integração e distribuição Linux/Windows; validação das plataformas também nas entregas anteriores. |

As funções originais fornecem algoritmos. Contagens de itens/culturas/atores,
endereços, overlays e layout de disco não definem os limites futuros do runtime.
As adaptações recebem estado explícito e definições de conteúdo quando necessário;
sem camada MIPS/SDK simulada nem segundo sistema para conteúdos novos.
Os testes T abaixo são selecionados pelo domínio alterado, não por uma cadeia
obrigatória de MA-07 até MA-25. A matriz registra candidatos; cada implementação
verifica dependências e equivalência antes de declarar reaproveitamento concluído.

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
| `src/main/game/farm_state.h`: `FarmState`; `farm_state.c`: `farm_state_init` | `HmbtnGameState.inventory`, `.economy`, `.clock` e `.registration` concluídos em MA-01..03; `HmbtnSession` agora possui esse estado e field/village/poultry, eventos/atores e casa (MA-04a/b), controle/ações/coleta/pesca/menu/loja e metadados de exploração (MA-06); recursos/caches em `HmbtnScenePreview` | Casa/menus acessam o mesmo estado; recursos e geometria/ANI são empréstimos do host. `FarmState` inclui headers PSY-Q e ponteiros. | **E/A**, MA-01..04 concluídas: tipos neutros, inventário/economia/calendário/cadastro, ciclo de vida e execução/atores; contratos MA-05 e tick livre MA-06 concluídos; coordenação de roteiros/separação de atualização e desenho concluídas em PF-04 (`P/runtime.c`). Ver [MA-01](INVENTORY_STATE.md), [MA-02](ECONOMY_STATE.md) e [MA-03](CALENDAR_STATE.md) e [MA-04a](SESSION_STATE.md), [MA-04b](SESSION_EVENTS.md). Não copiar ABI/padding. | T-ESTADO; menus, lojas, eventos e trocas de área conservam os mesmos valores. |
| `src/main/main.c`: `main_run_frame`; `F/farm_frame.c`, `F/farm_main.c`; `F/doors.c` | `P/runtime.c`: `hmbtn_session_tick`, entrada de área e roteiros/portas/sono; `P/exploration.c`: fases livres; `P/update_clock.c`/`core/app.c`: agendamento; `graphics/scene_preview.c`: adapters tick/draw separados | Providers síncronos de recursos/efeitos, geometria/ANI emprestadas; cobertura de conteúdo parcial. Coordenador sem SDL/arquivos/desenho. | **E, PF-04 concluída no recorte atual:** conserva ordem/regras nativas existentes; callbacks persistentes recebem sessão, frame só durante tick; backend e caches no host. | `native_update_clock`, `native_runtime_update_playthrough`, T-SESSAO: mesmos ticks com 0/1/2 draws, draw sem mutação, Duke sim/não, portas/sono/daily, falha de carga e suite completa. |
| `src/main/game/event_flags.c`: `event_flag_get`, `event_flag_set` | `HmbtnVillage.flags`, leituras/escritas em vários módulos | Flags originais em bits, host em bool; acesso a `game_state` e shift com sinal a auditar. | **A/D?**, MA-09: um acesso tipado, sem exigir mudar representação e regra ao mesmo tempo. | Flags de fronteira, clear/set, índices inválidos e visibilidade entre comandos/rotinas. |
| `src/main/graphics/screen_fade.c`: `rand_range` | `core/psx_random.c`: `hmbtn_psx_random_range`; PRNG próprio em `P/poultry.c` | Estados locais, galinhas com outra fórmula; original consome RNG por frame. | **M/A**, MA-09: propriedade central com sequências atuais preservadas; decisão de sequência original fica explícita em MA-25. | Vetores em `village_affinity_test.c`, continuidade de seeds e ausência de sorteio no desenho. |
| `src/main/cd/afile.c`; tabelas de `F/farm_load.c` | `filesystem/resource_file.c`, `event_resources.c`, `action_resources.c`, `text_resources.c`; includes gerados | CD, offsets e endereços PS1 não são ponteiros host. | **M**, MA-05 concluída: `P/services.c` + `filesystem/service_resources.c` atendem IDs atuais com buffers próprios/tickets; MA-08 mantém a separação de recursos de apresentação. Não recriar CD/MIPS. | `native_resource_files`, importação, textos/ações, falha/truncamento e T-SERVICOS. |

## Persistência nativa — PF-05

| Origem original | Implementação nativa | Dependências e diferenças | Estratégia e situação | Validação |
|---|---|---|---|---|
| `src/overlay/15_saveload/saveload.c`: `save_pack_farm_state`, `save_unpack_farm_state`, checksum/confirmação | `P/save.c`: disponibilidade/encode/decode; `filesystem/save_file.c`: arquivo temporário/flush/rename | Original depende de imagens de structs, globais/endereço fixo, memory card/SDK e formato PS1. Host usa estado explícito, IDs, versão, tamanhos/CRC e construtor de sessão; nenhum ponteiro é gravado. | **M/E, PF-05 concluída:** seleção conceitual dos dados reaproveitada; codec/transporte próprios necessários para o PC. Exploração estável; o formato atual v5 rejeita outras versões, sem migradores exigidos nesta fase. | `native_save`, `native_save_files`: campos, propriedade, dois saves, restart real, truncamento/corrupção/versão/seções, recusa, falhas de publicação e slot anterior intacto. |
| `F/world/clock_events.c`: `farm_overnight_finish`; título Continue e diário | `runtime.c`/`daily.c`: pedido após noite; `core/app.c`: I/O entre ticks; `graphics/scene_preview.c`: cena candidata; título e cama atuais | Cadência, regras e atualização diária atuais preservadas. Restaurar recursos não é nova entrada em área: conservar flags, pesca, moradores e chave diária. | **A/E, PF-05 concluída:** pedido de save consumido uma vez, sucesso somente após I/O; troca de cena somente depois da carga completa. Slot nativo separado do BIN e sem emulação de memory card. | `native_save_playthrough`: cama/diário, Continue do app, perda de recursos, persistência de campo/galinhas/economia, efeitos únicos e Duke concluído sem repetição. |

Detalhes iniciais, esquema e caminhos em [PF05_NATIVE_SAVES.md](PF05_NATIVE_SAVES.md).
O escritor/leitor atual é v5: RNGs, progresso de Moondrop e prazo de obra em
AUXI; a pontuação derivada de construções não é gravada. Leitura de versões
anteriores retirada por decisão do usuário em 2026-10-27. Referências a v1..v4
nas entregas abaixo registram o formato da ocasião, sem promessa de compatibilidade.

## Eventos e comandos

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `F/event/event_script.c`: `event_block_entry`, `event_block_find_marked`, `event_script_run` | `P/event_script.c`: `hmbtn_event_block_open`, `hmbtn_event_block_entry`, `hmbtn_event_tick`; `session.c`: `hmbtn_session_open_event`/`event_tick` possuem bytecode e execução (MA-04b) | Blocos LE, cursor, flags de yield, resultado e transição; handlers incompletos. | **M/A**, MA-11: manter leitor validado e consolidar despacho sem criar outro interpretador. | `native_event_runtime`; saltos, fim, limite de instruções, retorno e espera sem repetir efeitos. |
| `F/event/event_commands.c`: `event_var_get`, `event_var_update`, `event_var_compare`, `event_var_arith`, `event_var_address` | `P/event_variables.c`: `hmbtn_event_variable_get/update`, `hmbtn_event_arithmetic/comparison`, `hmbtn_event_variables_command`; conversas/visitas/slots da abertura consomem o módulo | Globais, uniões de slots, ponteiros para variáveis, sentinelas, clamps e valores iniciais fixos no host. | **A, PF-03 concluída no recorte 13..22**: operadores privados removidos; contexto empresta estado/slots com tamanho explícito, sem endereços PS1. Tabela inteira de variáveis e sistemas ausentes continuam em PF-06. | `native_event_variables`; T-EVENTOS, signedness, wrap definido, divisão/resto extremos, índices, clamps, comparação que indica falha, serviços pendentes/falhos/cancelados sem efeitos posteriores. |
| `F/event/event_commands.c`: `event_message_start`, `event_cmd_wait_frames`, `event_cmd_set_sprite`, `event_cmd_set_position`, `event_cmd_return` | Handlers `command` em `P/farm_intro.c`/`village_talk.c`; callbacks em `P/runtime.c`, providers de apresentação em `graphics/scene_preview.c` | Mensagens, atores, conclusão de animação, recursos e slots; casos presos a IDs. | **A/M**, MA-12/13: migrar famílias separadas para o mesmo despacho, usando contratos da sessão. | Abertura/infância, Thomas Sim/Não, funeral, nomeação e conversas; T-SERVICOS/T-EVENTOS. |
| `F/event/event_commands.c`: `event_triggers_fire`; `F/farm_init.c`: `farm_event_available` | `P/farm_visit.c`: `hmbtn_farm_event_trigger`, `available`, `window_match` | Datas, clima, inventário, flags e registros de área; whitelist de eventos/condições. MA-01 removeu cópia da consulta usando `hmbtn_event_items_count(const HmbtnInventory *)`. | **A**, MA-14: adaptar predicados originais e ampliar cobertura por condição comprovada. | `native_farm_visit`, visitas, torta, ingredientes; limites inclusivos, virada de dia/ano e disparo único. |
| `F/event/event_commands.c`: `event_items_take`, `event_item_can_take`, `event_item_store` | `P/inventory.c`: `hmbtn_inventory_count/take/store_tool`; `event_item.c` mantém wrappers; `village_talk.c` resolve operandos e notifica | Estado em `HmbtnGameState.inventory`; equipamento/mochila/baú representados. Geladeira/armário continuam ausentes. | **A/M, PF-01 concluída:** travessia existente equivalente preservada; ordem de armazenamento compartilhada com medicina. Comandos adicionais em PF-03/06. | `native_inventory_rules`, item/tool-store, Greg, presentes e ingredientes; capacidade, slot inicial, metadados, rollback e consumo único. |
| `F/event/event_script.c`: bloco/dispatch; `event_commands.c`: reset/posição/face/movimento/variáveis/término; gatilho 140 e mensagens 1:418..421 instalados | `P/farm_visit.c`, `farm_intro.c`, `session.c`, loader/cena: convite de Duke para a vindima, com `event_variables.c` comum | Ator residente 21 ligado ao contexto, ANI/banco reais, calendário/flags/amizade existentes. Sem novo interpretador ou cópia de progressão. | **A, PF-03 concluída:** aceitar/recusar, movimento, flag 205, duas instruções de amizade e execução única. PF-06 para vindima/Cliff e demais eventos; agendamento entregue em `P/runtime.c` na PF-04. | `native_vineyard_invitation_playthrough`: gatilho/data, palavras exatas, dois ramos/clamp, ator ativo/rota, integridade de inventário/saldo e persistência/ausência de repetição entre áreas; regressões dos eventos anteriores. |

## Inventário, economia e calendário

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `F/menu/storage_swap.c`: troca/pilhas; `F/menu/item_storage.c`: compactação; `F/people/player.c`: troca rápida | `P/inventory.c`: `hmbtn_inventory_swap/cycle/compact`; menus/casa chamam o módulo | Preservadas regras nativas já baseadas no original; catálogo e UI atuais ainda limitados. | **M/A, PF-01 concluída no recorte:** removidas regras de `farm_menu.c` e compactação própria da casa; ciclo corrigido para respeitar capacidade. Ampliar catálogos em PF-06. | Inventário, casa, menu e carry; células inativas, pilhas/sementes, limites e cancelamento. |
| `F/estate.c`: `estate_gold_change`, `estate_shipment_add/settle`, `estate_item_slot_index` | `P/economy.c`: `hmbtn_economy_gold_change`; remessa em `farm_shipping.c`, consumidores em lojas e evento da torta | Carteira/ledger/remessa centrais; teto nativo configurável, default original; rollover e seleção de remessa continuam nos módulos existentes. | **A, PF-01 concluída para dinheiro:** removidas operações duplicadas de saldo/ledger; PF-02/06 reconciliam regras de remessa conforme o percurso, sem declará-las inteiramente portadas aqui. | `native_economy_rules`, shipping, lojas, shortage e torta; zero/insuficiência/reset, teto, receita integral e pagamento único às 17h58. |
| `F/menu/shop_dialogues.c`; scripts 365/368 instalados; `F/estate.c` | `P/feed_shop.c`: `hmbtn_feed_shop_open`, `hmbtn_feed_shop_tick`, compra/venda/medicamento | Horários e predicados copiados de scripts; inventário/economia/calendário/cadastro centrais em MA-01..03; loja empresta o relógio; registros de animais ainda no contexto atual. | **A/M**, PF-01 integrada para dinheiro/medicina; PF-02 adapta o menu base de primavera na mesa (grama/nabo/batata/pepino), `shop_menu_open_choose`/`shop_menu_choose_next`/`shop_buy_stored_item`, falas por banco/ID e `shop_item_room`/`shop_item_add` para lotes 1..99; PF-06 integra menus base de verão/outono, grupos B/C extraídos, chaves 0x129/12A, saudações 0x273/274 e catálogo por estação compartilhado com a UI. Demais comandos/lojas: preservar UI; consumir comandos/regras comuns e predicados originais em um estabelecimento por entrega. | `native_seed_shop` e percurso com BIN: nomes, quantidade, falas/bancos, cancelamento, saldo/espaço e lote inteiro no baú. Loja → cocho; compra/venda/nomeação/medicina e ausência do vendedor. |
| `F/farm_main.c`: `farm_track_id`, `farm_set_message_timing`; `F/world/doors.c`: `door_step`, `door_entries_update`; `F/farm_init.c`: `farm_zone_trigger`; `F/people/villager.c`: células de evento | `P/world_map.c`: seletores com clima; `P/area_doors.c/.h`, `HmbtnSession.area_doors`; `village.c`, `exploration.c`, `runtime.c`, `graphics/scene_preview.c` | Recursos VAB/SEQ e modelos/zonas/rotas existentes; portas comuns por evento/modelo, sem pool global PS1. Inventário continua único. | **A/E, PF-06 correção:** chuva troca música por faixa 22; portas precedem arremesso; NPC espera abertura, cruza e fecha; recuo/tempos do usuário preservados. Estado visual reinicia na carga, rota persiste em v2. Eventos especiais de portas continuam parciais. | `native_area_doors`, `native_weather`, `native_weather_doors_playthrough`, chuva/interiores/transporte: PCM real/loop, dia/noite/carga, mãos ocupadas/recusa/arremesso, quatro sentidos/folha dupla, pausa/catch-up, rotas reais de entrada/saída e save durante abertura. |
| `F/world/clock.c`: `clock_advance_minute`, `clock_time_period`, `clock_date_index`; `F/world/weather.c` | `P/farm_clock.c`: `hmbtn_farm_clock_tick`, `hmbtn_farm_clock_advance_notify`, `hmbtn_daylight_*`; relógio em `HmbtnGameState.clock` | MA-03 removeu `sync_clock` e campos de data/hora da casa; consumidores usam a mesma referência. Pausa, calendário 0/1-based, callbacks e luz mantêm regras atuais. | **E** MA-03; **M/A, PF-02:** avanço nativo equivalente conservado; sono/eventos usam observer diário compartilhado em `daily.c`. Blend fica visual; PF-06 completa clima/callbacks ausentes. | `native_farm_clock`, `native_world_clock_playthrough`; 06h, estação/ano, sono, pausa, avanço de vários dias e datas especiais. |

## Atualização diária, campo, animais e moradores

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `F/world/clock_events.c`: `scene_overnight_step`, roll/hour/overnight; `F/world/clock.c` | `P/daily.c`: observer/avanço/preparação de manhã; `farm_clock.c` conserva avanço equivalente; cena carrega recursos | Campo, produção/ração e data pertencem à sessão. Clima/previsão/chuva integrados em PF-06 após produção/limpeza. Família/espécies ausentes não têm callbacks simulados. | **A/E, PF-02 concluída no recorte:** campo → produção → limpeza, datas/morning idempotentes; PF-04 retirou coordenação de gameplay da apresentação (`P/runtime.c`), PF-06 completa callbacks. | `native_crop_daily` e percurso; todo poultry/feed/calendar/shipping; sono, dias múltiplos, pausas e ordem. |
| `F/event/event_request.c`: `scene_item_queue_reset`, `scene_message_timers_tick` | `P/event_days.c`: `hmbtn_event_days_init`, `hmbtn_event_days_advance` | Contadores, flags/datas/clima e casamento; defaults gerados. | **A/M**, MA-19: conectar ao pipeline com estado central e preservar saturações. | `native_event_days`; sentinela 255, saturação 120, clima/datas e múltiplos dias. |
| `F/world/weather.c`: `weather_roll_tomorrow`, `weather_step_today`, `date_next_day`, `field_reset_cells`, partículas; `F/world/clock_events.c`; `screen_fade.c`: RNG; `event_commands.c`: aniversários | `P/weather.c`, `HmbtnGameState.weather`; `daily.c`, campo/regador; casa/TV/ações/pesca/galinhas/eventos/vila; `weather_effects.c`, luz/cena | Calendário/cadastro/RNG/campo existentes; save v2 com migração v1. Flags meteorológicas derivadas. Câmera e luz sobre a chuva corrigidas pelo original. Casamento ativo, danos de tempestade, estufa e efeitos/áudio/falas completos continuam pendentes. | **A/E, PF-06 clima diário/chuva concluído no recorte:** estado autoritativo, previsão e uma troca por manhã; crescimento anterior à irrigação; reaproveita motor/gerador, apresentação separada. | `native_weather`, `native_weather_playthrough`, `native_save`; vetores sazonais, 14 datas/aniversários alternativos, vésperas/ano, dias múltiplos, TV, chuva/solo/enxada/grama/inverno, cinco estados/flags, RNG/persistência/carga v1 real, viagens/pausa/draw e regressões existentes. |
| `event_commands.c`: dispatch 8/`EVENT_SHOP_SEEDS`; conversa 400; `menu/shop_dialogues.c`: `shop_seed_menu_setup_d`, `shop_result_request_std`, `shop_buy_stored_item`; `field.c`/`farm_tables.c`: repolho, abacaxi, pimentão | `P/village_talk.c`, `session.c`, `exploration.c`: adapter/espera; `feed_shop.c`/UI/gerador: catálogo/perfil comuns; `farm_field.c`/`estate_art.c`: suporte/sprites | Agenda/conversa reais; grupo D nativo atual tem 6 entradas antes e 7 das 8 após liberar Moondrop pela visita 19. Orangecup/estufa pendente; milestones do supermercado integrados no recorte seguinte. Save v2 existente. | **A, PF-06 sementes sazonais de Won concluídas no recorte:** sem outro motor/estoque/economia; original dispatch/wait, compra/cultivo/remessa/rebrota. | `native_won_seed_rules`, `native_won_0/1/2_playthrough`; entradas fonte, mensagens distintas, quantidades, cancelamento/fundos/baú/lotação, estágios/arte/compra/pagamento, fim da conversa e persistência em outro processo. |
| `farm_init.c`: `farm_event_available(7)`; `field.c`: `field_find_free_tile`, cultivo e arte de Moondrop; `event_commands.c`: `event_wedding_role_match`, `event_bond_match`, `event_calendar_match`, `event_cmd_dispatch(27)`, publicação/ID de item; blocos 18/19; `shop_seed_menu_setup_d` | `P/farm_visit.c`: gatilhos/contexto de campo; `runtime.c`/`farm_intro.c`: roteiros originais e pacote; `event_variables.c`: progressão compartilhada; `game_state.h`: `progress`; `feed_shop.c`: disponibilidade; `save.c/.h`: v4; `graphics/scene_preview.c`: ANI da entrada 0x0831; `estate_art.c`: 12/40 | Visita 18 é dependência para obter o primeiro pacote; 19 libera Won na chegada. Progressão distinta de flags 269/270, sem novo interpretador/inventário. Busca de existência ignora chão e não sorteia posição. Estufa/família completa ausentes. | **A, PF-06 Moondrop concluída:** 300 G, seis dias regados, sem rebrota ou remessa; 6/7 linhas contextuais. V4 persiste booleano, lê v1..v3 com falso. Família atual solteira; demais papéis continuam fora do recorte. | `native_farm_visit`, `native_flower_rules`, `native_won_seed_rules`, `native_save`, `native_moondrop_playthrough`: limiar/solo/estágio/espaço, presente/conversa/espera, arte/textos/música, idempotência, compra/colheita/mochila/presente, estação/inverno e carga em outro processo. |
| `F/field/field.c`: `field_event_dispatch`, `field_daily_tick`, `field_cell_look_fall`; `farm_tables.c`: `CROP_BLUE_MAGIC_RED`/`CROP_RED_MAGIC_RED`; `src/main/graphics/screen_fade.c`: `rand_range`; `shop_seed_menu_setup_d` | `P/farm_field.c/.h`: sorteio e dono `plant_random`; `core/psx_random.c` existente; `graphics/estate_art.c`: sprites 17/25; `P/save.c/.h`: v3; mesma seleção de Won | Sorteia somente células aceitas, grava variantes 0x178/179; não toca outros RNGs. V3 grava sequência em AUXI, lê v1/v2 com seed 1. Sem motor/estoque/diário novos. | **A, PF-06 Magic Red concluída:** 600 G, dez dias regados, azul sem renda/vermelha 200 G sem contador dedicado. Alias maduro zero seguro; inverno limpa. Próximo: Moondrop com disponibilidade separada do suporte. | `native_magic_red_rules`, `native_save`, `native_won_seed_rules`, `native_won_5/6_playthrough`: ambos os sorteios, grade com células recusadas, espera da ANI, ausência de reroll, noites secas, sprites, compra/baú, presentes/remessas e saves antes/depois em outro processo. |
| `farm_tables.c`: `CROP_PINK_CAT`/`CROP_TOY_FLOWER`; `shop_seed_menu_setup_d`; `field_daily_tick`, `field_tile_item_stage`, `field_cell_look_spring/summer/fall`; `graphics/anim.c`: `anim_draw_sprt_parts` | `P/feed_shop.c`: seleção filtrada; `farm_field.c`: suporte; `graphics/estate_art.c`: sprites/faixa madura | Mesmos catálogo, campo, diário, inventário, presentes e save v2; sem novos estados. Índices visíveis variam com Moondrop; entradas fonte 4/6 sempre preservadas. | **A, PF-06 flores simples concluídas:** preços/estágios originais, coleta única, preço de venda zero, sprites vivos/murchos. Faixa madura zero explícita substitui leitura original fora de limite. Demais flores/estufa pendentes. | `native_flower_rules`, `native_won_seed_rules`, `native_won_3/4_playthrough`: compra/cancelamento/fundos/espaço/baú, noite seca, estágios/arte, mão/mochila, presente real para Lillia, sem renda de remessa, estação/inverno e save/load em outro processo. |
| `F/field/field.c`: `field_event_dispatch`, `field_daily_tick`, `field_tile_dry`, `field_tile_item_stage`; `people/player.c`: plantio; `picture_cells.c`: grade | `P/farm_field.c`: definição `HmbtnFarmCrop`, dias regados por célula, plantar/crescer/colher; `farm_action.c` usa banco 24/ANI/grade original; carry/remessa existentes | Nabo e batata com percurso completo; pepino com rebrota testada. Grama/feno, tomate/milho/cebola e berinjela/cenoura/batata-doce também integrados. Chuva usa o mesmo campo/regador; sem estufa ou ervas aleatórias. Catálogo/instâncias separados; outros seeds permanecem fora do filtro de suporte atual. | **A, PF-02 concluída:** compra real de sementes → plantar/regar → quatro noites → colher/vender. PF-06 habilitou batata 0x23 com catálogo existente (3+4 dias, produto 0x42, sem rebrota), sprite 8 e save v1; grama/feno integrados por adaptação de `field_daily_tick`, `field_tile_harvest`, `field_grown_cell_request` e `estate_fodder_add`, usando o mesmo motor/estoque. | `native_crop_daily`, `native_crop_playthrough`, `native_potato_playthrough`, field/carry/shipping; seco, inverno/estação, skip-day, nine-cell, fase/consumo únicos e sprites reais. |
| `F/field/field.c`: `field_reshape_grown`, `field_cell_retexture`, `field_gate_open`; `F/field/field_item_requests.c`: `item_request_fill`; catálogo `CROP_GRASS` | `P/farm_field.c`: grama sem rega, corte/rebrota/estoque e formas de terreno derivadas; `field_effects.c`, `exploration.c`, `graphics/scene_preview.c`: ANI 14/19, terreno sazonal | Campo/observer/inventário/celeiro/saves v1 existentes; sem cache de forma ou estoque independente. `merge=1`, clima e estufa fora do recorte. | **A, PF-06 grama/feno concluído no recorte:** compra na mesa, onze noites, corte único, estoque 0 e cochos, rebrota/skip-day, inverno/primavera. Capacidade inicial 999 é regra de gameplay. | `native_grass_daily`, `native_grass_playthrough`: crescimento/estações/capacidade/ferramentas/colisão, vizinhos/borda/UV, efeito, save/load e outro processo, retirada/devolução/cocho; regressões de cultivos/feed/saves. |
| `F/event/milestones.c`: `milestone_check_group_a/b/c`; `shop_seed_menu_setup_a/b/c`; `farm_tables.c`: morango/abóbora/espinafre; `field_cell_look_spring/summer/fall` | `P/farm_shipping.c`: consulta de desbloqueio; `feed_shop.c`: estoque contextual; gerador: grupos/limiar/entradas; `farm_field.c`, `graphics/estate_art.c` | Mesmas remessas, loja, campo, ações, diário e save v2; economia emprestada pela loja. Sem novo status persistente ou catálogo paralelo. | **A, PF-06 desbloqueios concluídos no recorte:** 101 de cada produto libera a linha da estação; definições/sprites no motor comum, rebrota do morango conforme tabela original. | `native_seed_unlock_rules`, `native_seed_unlock_0/1/2_playthrough`: 100/101 por produto, grupos isolados, mensagens/índices/fundos/espaço/baú, última remessa real, crescimento/ANI/colheita/pagamento e saves em outro processo. |
| `F/people/player.c`: `player_update_position`, `player_draw`, `player_draw_carried_item` | `P/runtime.c`, `exploration.c`: limitação na porta; `graphics/model_render.c`: anexos; `scene_preview.c`, `house_ui.c`: item no mundo/cache compartilhado | Profundidade do dono, ANI/atlas existentes, limites da área decodificados; sem avanço de gameplay no desenho. | **A/E, correção PF-06:** câmera permanece limitada na entrada; item cobre o corpo e é encoberto junto com ele pelo cenário. Movimentos/tempos preservados. | `native_model_renderer`, `native_weather_doors_playthrough`: pixels de dono/anexo/parede/NPC, entrada em região livre e além do limite real Z=300; capturas de entrada/casa/supermercado. |
| `farm_tables.c`: `CROP_TOMATO/CORN/ONION`; `F/field/field.c`: `field_daily_tick`, `field_tile_item_stage`, `field_cell_look_summer/fall`; `F/menu/shop_dialogues.c`: `shop_seed_menu_setup_b`, `shop_result_request_ready`; `F/event/milestones.c`: grupo B | `P/farm_field.c`: habilita sementes; `P/feed_shop.c` e UI: catálogo por estação; `graphics/estate_art.c`: `hmbtn_estate_crop_look`; cena: composição base/sazonal na carga | Mesmos campo/ações/daily/inventário/economia/remessas/save v1. Abacaxi integrado por Won; abóbora/grupo B integrados na entrega de desbloqueios. | **A, PF-06 verão base concluído:** três culturas, compra na mesa, crescimento, colheita e rebrota prevista; sem algoritmos paralelos. Bancos recompostos permitem save novo no verão. | `native_summer_crop_rules`, `native_tomato/corn/onion_playthrough`, `native_seed_shop`; vetores de estágios/noites secas, quatro sentidos, falas/chave/quantidade/preços/espaço, sprites/murchamento, skip-day, remessas/pagamento adiado e carga em outro processo. |
| `farm_tables.c`: `CROP_EGGPLANT/CARROT/SWEET_POTATO`; `F/menu/shop_dialogues.c`: `shop_seed_menu_setup_c`, `shop_result_request_alt`; `F/field/field.c`: crescimento/colheita e `field_cell_look_fall/winter`; `F/event/milestones.c`: grupo C | `P/farm_field.c`: habilita 0x2C..0x2E; `P/feed_shop.c`/gerador/UI: catálogo, chave 0x12A e moldura ajustável; `graphics/estate_art.c`: sprites de outono | Mesmo campo/daily/ações/inventário/economia/remessas/save v1. Pimentão integrado por Won; espinafre/grupo C integrados na entrega de desbloqueios. | **A, PF-06 outono base concluído:** compra, dias regados/secos, rebrota nos estágios 2 e 1, colheita/pagamento e limpeza no inverno, sem outro motor. | `native_fall_crop_rules`, `native_eggplant/carrot/sweet-potato_playthrough`, `native_seed_shop`; vetores, quatro sentidos, chave/falas/preços/capacidade, capturas de UI inspecionadas, skip-day, coleta adiada, sono Outono 30→Inverno 1, carga comparada e outro processo. |
| `F/animals/chicken.c`: `chicken_daily_tick`, `chicken_weather_tick`, `chicken_kind_by_age`, `chicken_timer_tick` | `P/poultry.c`: `hmbtn_poultry_new_day`, `hmbtn_poultry_tick`; `poultry_carry.c`; proprietário `HmbtnSession.poultry` (MA-04a) | Ração, ovos, idade/doença, RNG host diferente, flags, cues e registro de ator. | **A**, MA-21: começar por alimentação/ovos; separar incubação/saúde/movimento, sem migrar outras espécies nessa tarefa. | Todos os percursos `native_poultry_*`; sem duplicar animal/ovo, três noites, crescimento, morte/liberação, transporte e medicina. |
| `F/people/villager.c`: `villager_schedule_advance`, `villager_update_frames`, `villager_route_step_move` | `P/village.c`: `hmbtn_village_sync`, `hmbtn_village_tick`; proprietário `HmbtnSession.village` (MA-04a); atores temporários também na sessão e autoridade explícita (MA-04b) | Agendas BIN, mapa, relógio, colisões, espera/atraso e override de atores. | **A**, MA-22: portar seleção/retomada primeiro; depois movimento de uma rota, mantendo identidade. | `native_village`, percursos vila/mapa; atraso de 30 minutos, troca de área/hora, evento e retomada sem teleportar. |
| `F/people/villager.c`: `family_flags_update`, `villager_affection_decay`; `F/people/family.c` | `P/village.c`: `daily_affinity`; `village_talk.c`: presentes/aniversários/variáveis | Flags diárias, RNG, amizade/afeição; família/casamento ainda ausentes. | **A**, MA-23: portar ganho/decay já coberto; registrar fronteiras de família para tarefas futuras, sem implementá-la inteira. | `native_village_affinity`, `native_item_reaction`, presentes; clamps, negligência, exceções, ganho único e aniversário alternativo. |
| `F/field/fishing.c`: `fishing_bite_wait`, `fishing_roll_catch`, `fishing_player_frame`; `F/people/player.c` | `P/fishing.c`: `hmbtn_fishing_wait`, `hmbtn_fishing_roll`, `hmbtn_fishing_tick` | Calendário, flags de captura, inventário, ação/animação, chão e RNG próprio. | **A/M**, MA-24: algoritmos de espera/captura sobre serviços comuns; manter renderização/sons existentes. | `native_fishing`, Greg e pesca; captura única, berry, estampas, requisitos de peixes, cancelamento e bolsa cheia. |

## Recortes de apresentação e backends preservados

| Origem original | Implementação nativa atual | Dependências e lacunas | Estratégia e tarefa | Testes necessários |
|---|---|---|---|---|
| `src/overlay/27_GF_TITLE/title.c`: `title_sequence_step`, `title_fill_field_layout`, `title_player_reset_for_new_game`; `src/main/ui/name_entry.c` | `P/title_flow.c`, `new_game.c`; `graphics/title_preview.c`, `new_game_ui.c` | MA-03 separou `HmbtnRegistration` do editor; título/abertura usam estado central e transferência única de valores. Flow/editor sem SDL; host conserva recursos/UI/áudio, tick e draw separados desde PF-04; amostras visuais conservam a ordem de ANI/scroll. | **E/M, PF-04 concluída:** fluxo conservado, RNG/timers/ANI/editor no tick; composição somente no draw, sem tempo/entrada. Ampliar conteúdo e fidelidade conforme PF-06/07. | `native_title_flow`, `native_new_game`, `native_title_preview`, `native_title_update_playthrough`: cadastro completo 0/1/2 draws/tick, RNG/ANI invariantes e transferência para abertura. |
| `F/people/player.c`: `player_update_mode`, `player_update_position`, `player_apply_tool_target`; `src/main/geometry/ground.c`: `ground_find_object`, `ground_height_at` | `P/house_control.c`, `farm_action.c`, `ground.c`; coordenação/ANI em `P/exploration.c` (MA-06) | Ground/ANI decodificados e serviços síncronos; transições coordenadas em `P/runtime.c` desde PF-04. GTE/tipos do SDK na origem. | **M/A**, MA-06 concluída: tick livre/ações sem desenho; PF-04 concluiu roteiros/apresentação; revisar/ampliar algoritmos por ação conforme PF-06. | Controle/campo/carry, portas, escadas, velocidade pelo modo anterior, menus e efeito da ferramenta sem desenho. |
| `src/main/geometry/model.c`, `src/main/graphics/texture_pack.c`, `src/main/graphics/anim.c`; `src/main/sound/` | `graphics/model_decode.c`, `model_render.c`, `asset_decode.c`; `audio/psx_audio.c`, `xa_*` | GPU/GTE/SPU/CD e tipos PSY-Q; backend nativo já funcional, fidelidade parcial. | **M**, MA-05 concluída: contratos identificados e `audio/service_audio.c`, UI/ANI atuais reportam conclusões; ANI de gameplay livre no domínio MA-06; separação do tick/desenho concluída na PF-04 para os fluxos atuais. Não implementar camada completa de SDK. | Modelos, assets, áudio, XA, screenshots sintéticos, T-SESSAO/T-SERVICOS e comparação visual/auditiva posterior. |

PF-01: [origens e limites](PF01_SHARED_RULES.md). Os testes novos
`native_inventory_rules`/`native_economy_rules` acrescentam vetores de algoritmos,
capacidade, extremos numéricos e integração com consumidores reais. As entradas
MA restantes abaixo/na matriz são escopos históricos convertidos pela fila PF.

## Testes novos exigidos pela migração

Os nomes T abaixo são grupos de requisitos. T-ESTADO do inventário tem cobertura
em `native_inventory_state`/`native_inventory_playthrough` desde MA-01 e da economia
em `native_economy_state`/`native_economy_playthrough` desde MA-02. Calendário/cadastro têm cobertura em `native_calendar_state`/`native_calendar_playthrough`
desde MA-03. As telas financeiras, de calendário e nomes têm consultas testadas
sem mutação. MA-04a acrescentou `native_session_state` (sem SDL) e
`native_session_playthrough` (recursos locais) para ciclo de vida, UI, empréstimos
e registro de animal entre áreas. MA-04b acrescentou `native_session_events`
(sem SDL: bytecode, waits, identidade e retomada) e `native_session_events_playthrough`
(recursos locais: evento ativo, UI/rebind, duas sessões e rota real). MA-05
acrescentou `native_services`, `native_session_services` (sem SDL) e
`native_services_playthrough` (recursos/áudio reais) para tickets, buffers,
respostas atrasadas, erro/cancelamento e efeitos únicos; entrada lógica também
testada. MA-06 acrescentou `native_exploration` (sem SDL, chão/ANI sintéticos)
e `native_exploration_playthrough` (três sessões reais com 0/1/2 apresentações
por tick) para ações completas, pausa/calendário e domínio independente do
desenho no recorte livre. O restante continua planejado, não
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

PF-02: [origens, limites e percurso](PF02_CROP_CYCLE.md). A adaptação de relógio
já equivalente foi mantida; `daily.c` agora centraliza o observer que antes ficava
em `exploration.c`. Os testes sem dados e com dados instalados cobrem as regras
novas; naquela entrega a separação completa de apresentação ainda estava pendente (entregue em PF-04).


PF-04: [coordenação e apresentação](PF04_UPDATE_PRESENTATION.md). Os novos
`native_update_clock`, `native_runtime_update_playthrough` e
`native_title_update_playthrough` verificam agendamento, estado independente de
draw, transições/sono/falhas e cadastro. Todos os percursos usam APIs públicas,
sem incluir arquivos `.c` de produção. Saves estáveis foram entregues em PF-05.


PF-06, batata: [fontes e resultado](PF06_POTATO.md). Reutilizados `HmbtnFarmCrop`,
`farm_field.c`, ação/grade/loja/remessa/sono e codec de save existentes. A entrega
habilita o filtro e a seleção visual corretos; não substitui algoritmos já úteis.
O driver de cultivo é compartilhado com nabo e valida sete dias regados, noite
seca, save/load durante crescimento, produto/preço e transação única.


PF-06, grama/feno: [fontes e resultado](PF06_GRASS.md). Mesmo campo/observer
para crescimento e rebrota, mesmo `inventory.supplies[0]` para corte e celeiro,
mesmo codec v1. Formas de terreno são derivadas no retexture; ANI de corte é
avançada no tick. Não foram criados subsistemas paralelos.


PF-06, verão: [fontes e resultado](PF06_SUMMER_CROPS.md). Funções de crescimento,
plantio, colheita, inventário e pagamento foram reutilizadas; integração de
catálogo sazonal e seleção visual adapta os ramos originais. A carga de área
compõe os bancos necessários sem depender do histórico da cena. Saves v1 intactos.


PF-06, outono: [fontes e resultado](PF06_FALL_CROPS.md). Reutilizados campo,
algoritmos diários/colheita, inventário, economia, serviços e saves. Grupo C e
sprites foram integrados; moldura da UI acompanha o maior nome original. Próximo
recorte funcional é clima diário/chuva, com critérios definidos no roadmap.


## Carpintaria e melhorias — PF-06

| Origem original | Implementação nativa | Dependências / estratégia | Verificação |
|---|---|---|---|
| `F/menu/shop_dialogues.c`: `shop_lumber_menu_setup`, `shop_lumber_deal_step`, `shop_menu_choose`, `shop_first_clear_flag`, `shop_silo_offers` | `P/feed_shop.c/.h`, `graphics/feed_shop_ui.c`, `cmake/generate_farm_tables.py` | **A, primeiro recorte concluído:** mesmo modal, mensagens 2:0x132..0x145 e confirmação 0x5F, materiais/ledger autoritativos. Madeira/galinheiro/primeira casa/obra do celeiro expostos; criação bovina/ovina, casa2/estufa pendentes. Casa: descrição 0x135, oferta 0x13D, 4.700 G/370 madeiras. | `native_construction`, `native_carpenter_playthrough`: descrições, quantidade, 50 G, 5.000 G/420 madeiras, falhas/cancelamento, estoque, ocupação e transação única. |
| `F/event/milestones.c`: `milestone_arm_countdown`, `milestone_tick`; `world/clock_events.c` | `P/construction.c/.h`, `daily.c`, `session.c`, `save.c` | **A:** quatro manhãs no observador diário existente. `construction` possui prazo/tipo/níveis da casa e celeiro; `poultry.upgraded` possui nível do galinheiro. Save v8 atual, sem migração. Espécies ausentes e demais milestones continuam pendentes. | Noites normais, notificação repetida, save/reload durante/após obra, rejeição de outras versões e retomada do aplicativo em outro processo. |
| `event_flag_get`/`event_var_get`, `people/villager.c: villager_set_destination`, `event_cmd_store_levels`; roteiro BIN 374 | `P/village.c`, `event_variables.c`, `village_talk.c`, `farm_visit.c` | **A:** flags 0x38/0x3A/0x3B/0x3C derivadas; destino 38 é o byte alto da rota. Dispatch 81/8/9 usa loja compartilhada; script decide elegibilidade, sem contador/balcão alternativo. | Conversa real de Gotz, consulta ocupada, leituras/recusa de alteração dos flags gerenciados, vetor destino 6 versus mapa 51 e regressões de conversas/eventos. |
| `F/world/scene.c`: `scene_resolve_selection`, `farm_layout_index`; `menu/status_pages.c`: pontuação | `P/world_map.c`, `graphics/scene_preview.c`, `farm_menu.c`, `graphics/farm_menu_ui.c`; `poultry.c`/`farm_feed.c` existentes | **A:** IDs lógicos preservados; mapas originais ampliados; pontuação derivada, sem campo persistente duplicado. Primeira casa integrada; segunda casa e demais melhorias pendentes. | Recursos 4..7/116..119, entrada/saída/carga, dez vagas/cochos, uso da segunda incubadora; capturas de menu/falas/interior. |


## Primeira casa, depósitos e cozinha — PF-06

| Origem original | Implementação nativa | Dependências / estratégia | Verificação |
|---|---|---|---|
| `F/menu/shop_dialogues.c`: oferta/custo/flags; `milestones.c`: prazo; `world/scene.c`: `scene_resolve_selection`, `farm_layout_index` | `P/construction.c`, `daily.c`, `feed_shop.c`, `world_map.c`, `graphics/scene_preview.c`, `save.c` | **A, primeira casa concluída no recorte:** requer galinheiro, 4.700 G/370 madeiras, quatro manhãs, recursos 8..11/84..87; mesmos donos/serviços. Save atual v8 só lê v8; a obra do celeiro foi integrada abaixo. Casa2/estufa ainda ocultas. | `native_house_upgrade`, `native_house_upgrade_playthrough`, construção/save: falhas atômicas, Gotz real, prazo com reload, mapas sazonais, entrada/saída, persistência em outro processo. |
| `F/farm_data.c`: `house_door_positions`; `F/people/player.c`: `player_result_camera_points`; zonas BIN | `P/daily.c`, `house_sleep.c`, `runtime.c`, `house_interaction.c` | **A:** despertar/cama conforme nível autoritativo; TV/calendário/baú/diário nos novos lugares. Preserva curva de porta ajustada pelo usuário; nível é argumento de gameplay. | Casa ampliada: móveis reais, dormir/salvar/carregar e conservação de campo/inventário; regressões de chuva/portas/interiores. |
| `F/menu/item_storage.c`: navegação `item_markers`, resolução/trocas, classes/pilhas; `F/event/event_commands.c`: `event_items_take` | `P/inventory.c`, `game_state.h`, `house_interaction.c`, `exploration.c`, `graphics/house_ui.c`, gerador | **A:** geladeira/armário em `game.inventory`; UI empresta, scripts compartilham. Navegação extraída, pilhas de 99 e retirada de unidade; baú conserva algoritmo anterior. Valores de conteúdo originais não são imposição de hardware. | Depósito com mão ocupada, classes/recusa atômica, pilhas/overflow, modificador, última página, mochila expandida, scripts e save/load. |
| `F/menu/house.c`: `cooking_find_record`, `cooking_claim_recipe_tools`, `cooking_claim_entry_slots`, `cooking_sum_wanted_cues`, `cooking_clear_pattern_value`, `farm_cue_add_delta`; `src/main/game/cooking_recipes.c` | `P/cooking.c/.h`, `cmake/generate_farm_tables.py` | **A, núcleo funcional:** receitas/bônus/nomes extraídos, matching ordenado/grupos/qualidade adaptados, sem SDL/globais PS1. Não compila módulos originais diretamente. | Vetores bamboo rice/raisin bread/pickles/fiasco, utensílio ausente, grupos, prioridade/bônus; regras e percurso com dados. |
| `F/menu/house.c`: `cooking_record_available`, `cooking_timer_decrement`, `cooking_hold_result`, `cooking_record_store`, fases do modal; `status_pages.c` | `P/cooking.c`, `house_interaction.c`, `game_state.h`, `farm_menu.c`, `new_game.c`, `graphics/house_ui.c`, `save.c` | **A/M:** cozinha empresta inventário; receitas/utensílios pertencem a `game.cooking`; consumo mochila→geladeira, resultado na mão; contagem derivada. Mensagens BIN e escolhas de 3/4 opções. Aquisição pela TV e apresentação completa pendentes, sem concessão gratuita. | Cancelar sem consumir, preparar/guardar/repetir/variar, falta de ingredientes, registros/save/carga, temperos com fixture de posse e fiasco; desenho não altera domínio. |

Arquivos, percurso reproduzível, validação **23/23 CTests** e fronteiras em
[PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md). Esta seção atualiza o estado
corrente; menções a versões antigas nas linhas de entregas anteriores são históricas.


## Obra e cochos do celeiro — PF-06

| Origem original | Implementação nativa | Dependências / estratégia | Verificação |
|---|---|---|---|
| `F/menu/shop_dialogues.c`: `shop_silo_offers`, `shop_first_clear_flag`, `shop_lumber_menu_setup`, `shop_lumber_deal_step`; `F/event/milestones.c`: `milestone_tick` | `P/construction.c/.h`, `feed_shop.c`, `graphics/feed_shop_ui.c`, `port/CMakeLists.txt` | **A, obra concluída no recorte:** terceira oferta, 6.800 G/500 madeiras, quatro manhãs. `construction.barn_level` único; materiais/economia/modal existentes. Sem expor casa2/estufa. | `native_barn_upgrade`, `native_barn_upgrade_playthrough`: preços/recusas atômicas/cancelamento/ocupação, conversa 374, descrição 0x136/oferta 0x13E, prazo com reload e retomada pelo app. |
| `F/world/scene.c`: `scene_resolve_selection`, `farm_layout_index`, `house_level_offsets`, `barn_layout_offsets` | `P/world_map.c/.h`, `graphics/scene_preview.c` | **A:** recursos fazenda 12..15/celeiro 100..103; mantém áreas lógicas 0/96, mesma resolução para portas e carga de saves. | Quatro estações, entradas/saídas com feno na mão, permanência de campo/casa/depósitos e uso das colisões/texturas/zonas reais. |
| `F/people/player.c`: `player_zone_fodder_exchange`, `player_zone_place_fodder`; `F/animals/animals.c`: `farm_feed_bin_set`; `F/estate.c`: `estate_feed_bins_clear`, registros dos cochos | `P/farm_feed.c/.h`, `graphics/scene_preview.c`; ações/daily existentes | **A/E:** mesmas ações/posições/estoque. Helper consulta zonas ativas para desenhar feno; sem cópia de capacidade/nível. Cochos normais 10→20, maternidade 1→2. Não é capacidade animal implementada. | Máscaras 0x1003FF/0x3FFFFF, uso dos índices 10..19, retirada/devolução, sprites visuais, diário limpa uma vez e não altera estoque; regressões de grama/galinhas/feed. |
| Flag 0x3C, `event_cmd_store_levels`, pontuação de `status_pages.c` | `P/village.c`, `village_talk.c`, `farm_menu.c`, `save.c/.h` | **A:** flag, comando 95 e pontos derivam de `barn_level`; save v8 atual valida níveis/obra/pré-requisitos, sem migrador. | Escrita direta recusada, script sintético consulta quatro níveis, 60 pontos derivados, roundtrip com vínculos e rejeição de estados incoerentes. |
| `F/animals/animals.c`: `livestock_find_free`; `F/animals/livestock.c`: inicialização, atualização e ações; `F/menu/shop_dialogues.c`: `shop_barn_step` | `P/livestock.c/.h`, `feed_shop.c`, `daily.c`, `graphics/livestock_art.c`, `farm_menu.c`, `save.c` | **A/M, recorte base entregue abaixo:** coleção comum, compra/nome/arte, capacidade original 10/20 e alimentação antes da limpeza. Produção/reprodução/venda/cues integrais/pastagem/funerais pendentes. | `native_livestock`, `native_livestock_playthrough`: compra/cancelamento/fundos/vagas, animal visível/nome/estado, capacidade antes/depois da obra, consumo único, status e save/app em outro processo. |

Obra e abastecimento foram concluídos nesta entrega; aquisição/capacidade/
alimentação foram integradas no recorte seguinte, abaixo. Ciclo animal completo
continua parcial.
Fontes/arquivos/resultados: [PF06_BARN_EXPANSION.md](PF06_BARN_EXPANSION.md).
A ordem executável permanece no roadmap; não duplicar o controlador de obras
para implementar os consumidores animais.


## Vacas e ovelhas — aquisição/alimentação PF-06

| Origem original | Implementação nativa | Dependências / estratégia | Verificação |
|---|---|---|---|
| `F/animals/animals.c`: `livestock_find_free`, `place_row_spot`, `farm_row_spots`; `livestock.c`: `livestock_record_init`/`_b` | `P/livestock.c/.h`, `P/session.c/.h`, gerador | **A:** coleção única com capacidade 10/20 compartilhada, nível emprestado de construção, crescimento/arte/contagens derivados. Sem layout ABI PS1 ou nível paralelo. | Vagas mistas/lotação, posições e nomes originais, obra com animais, persistência e empréstimos após carga. |
| `F/menu/shop_dialogues.c`: `shop_fodder_menu_setup`, `shop_barn_step`; catálogo `ITEM_YOUNG_COW`/`ITEM_SHEEP` | `P/feed_shop.c/.h`, `graphics/feed_shop_ui.c`, CMake | **A:** Barley 6.000 G/4.000 G, mesma loja/editor/economia, descrições 0x67/68, recusa 0x72, nomeação 0x73 e sucesso 0x74. Cabeçalho 0x6B. Linhas sem operação real ficam ocultas. | `native_livestock`, `native_livestock_playthrough`: editor/cancelamento/fundos, confirmação e criação/débito únicos; regressões da loja de galinhas/medicina. |
| `F/animals/livestock.c`: `livestock_overnight_update`/`_alt`, crescimento e doença/afeição/idade; `animals.c`: dois passes diários | `P/livestock.c`, `P/daily.c`; `core/psx_random.c` existente | **A/M:** vacas antes de ovelhas consomem os mesmos cochos antes da limpeza. RNG local explícito e salvo; não preserva stream global PS1. Pastagem/reprodução/eventos de morte fora do recorte. | Alimento insuficiente/ordem por espécie, ganhos únicos, doença/medicina, maturidade 14→28, limpeza, repetição de data, cinco noites reais e save/app. |
| `livestock.c`: fala/cura e tabelas de frames; `animals.c`: `farm_message_request`; `menu/status_pages.c`: listas/contagens/ícones | `P/livestock.c`, `P/poultry.c/.h`, `P/exploration.c`, `P/farm_menu.c/.h`, `graphics/farm_menu_ui.c` | **A:** nome/saúde/afeição originais, mesma ação de medicina/dose/fadiga. Páginas consultam registros; removidos dois contadores persistentes vazios de gado. Cues/vozes integrais pendentes. | Texto com nome, estado após conversa, medicina/cura, status de ambas espécies e invariância de draw; galinhas/status/medicina continuam cobertos. |
| `F/farm_load.c`: slot 0; `F/farm_data.c`: `farm_slot_toc_0`; `livestock.c`: escala/estágios | `graphics/livestock_art.c/.h`, `scene_preview.c`, `asset_decode.c/.h`, `model_render.c/.h`, gerador | **A/E:** ANI/banco/offsets originais, limite de leitura validado; escala jovem 0xCC8/adulta 4096. Wrapper do decoder mantém tamanho anterior. Nenhum módulo original C compilado diretamente. | `native_asset_formats`, `native_model_renderer`, percurso instalado: redução/transparência/limites, ordenação/oclusão, artes jovem/adulta, transições e captura sem alterar gameplay. |
| Persistência nativa e seleção de campos | `P/save.c/.h` | **E:** v8 só lê v8; LSTK grava registros/RNG, sem ponteiros/atlas/contagens derivadas; capacidade e valores validados. | Roundtrip, rejeição de crescimento/vagas inválidos, arquivos transacionais e app novo carregando animais. |

Recorte base do celeiro atendido; **criação completa não concluída**. Venda,
produção/reprodução, transporte/pastagem, máquinas e eventos de morte continuam
pendentes, com mesma coleção como dono. Fontes, arquivos, testes e reprodução:
[PF06_LIVESTOCK.md](PF06_LIVESTOCK.md). Próxima construção: segunda casa no roadmap.
