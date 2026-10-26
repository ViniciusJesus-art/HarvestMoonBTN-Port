# Execução de eventos e identidade de atores — MA-04b

Entrega em 2026-10-25 sobre a revisão local `d3c0f38`. Ordem e aceite continuam
no [ROADMAP.md](ROADMAP.md); diretrizes em [PORTING_STRATEGY.md](PORTING_STRATEGY.md)
e dependências em [PORTING_MATRIX.md](PORTING_MATRIX.md). MA-04a é o registro
histórico [SESSION_STATE.md](SESSION_STATE.md). Esta entrega conclui MA-04b e
MA-04; não inicia MA-05.

## Propriedade e ciclo de vida

`port/game/session.h` acrescenta a `HmbtnSession` os objetos já existentes:

| Membro | Responsabilidade |
|---|---|
| `intro`, `event_code[3296]` | Jogador e atores de roteiro, bytecode próprio, cursor, slots, esperas, pedidos e resultados. |
| `visit`, `talk` | Visita ativa/atores temporários e execução das conversas. `talk.code` já guardava seu próprio bytecode. |
| `dialogue`, `animal_name`, `animal_name_active` | Estado transitório da mensagem/seleção e da edição do nome de animal. |
| `house` | Interações/clima/contadores existentes; inventário, relógio e cadastro emprestados de `game`. Conversas emprestam esta casa estável. |
| `event_item` | Apresentação/consumo de item pelo roteiro, sem outro inventário ativo. |
| `trigger_data`, `triggers` | Tabela delimitada e referência aos bytes da própria sessão. |
| `animal_event`, `previous_place`, `trigger_entry`, `first_visit_*`, `first_exit*` | Identidade, entrada, primeira visita e transição já existentes. |

Esses membros foram retirados de `HmbtnScenePreview`; consumidores usam os mesmos
endereços em `scene->session`. Criação inicializa casa e diálogo com as funções
anteriores. Destruição retira empréstimos do host antes de liberar a sessão.
Não copiar/mover/serializar a struct: código, gatilhos, conversas e atores têm
referências internas. A cena ainda possui a sessão; não foi criada agora uma API
de troca completa de adapters ou gameplay independente da renderização.

`hmbtn_session_open_event` usa o validador já existente e só então copia os bytes
do script para `event_code`. Entrada inválida conserva bytecode, evento, cursor,
slots e vínculos anteriores. A abertura/limpeza conserva os resets anteriores.
`hmbtn_session_event_tick` chama `hmbtn_farm_intro_tick`; não há outro interpretador.
Conversas mantêm seu runtime existente, agora armazenado na sessão.

## Uma identidade ativa por morador

`hmbtn_session_bind_event_actor` seleciona apenas armazenamento pertencente à
sessão, recusando IDs inválidos e vínculos conflitantes. `copy_route` transfere
uma posição inicial somente na primeira associação. Repetir a associação não
recopia nem sobrescreve o ator em execução.

| Fonte | Uso atual |
|---|---|
| `PRIMARY` | Thomas (17) no singleton já usado pelas visitas; não aceita outros IDs. |
| `INTRO` | Slots nomeados de NPCs (9, 10, 15, 17, 19, 23, 26, 34); torta, tour e funeral. |
| `VISIT` | Atores dos moradores presentes no local na entrega de ingredientes. |
| Rota | `village.people[id].actor`, quando não há override. |

`hmbtn_village_actor` resolve singleton → override por ID → rota. Colisão e
interação já seguiam essa prioridade; agora usam uma consulta compartilhada.
Zonas de conversa passam a considerar também o singleton, evitando consultar
uma posição de rota dormente para o mesmo morador. Há teste específico dessa
correção de integração. O desenho mantém os caminhos de arte existentes e evita
desenhar a mesma identidade duas vezes.

Rotas e animações de rota não avançam sob override; atrasos por colisão de uma
rota dormente também não se acumulam. Afinidade, flags, agenda,
local/etapa agendados continuam no registro persistente, com seus algoritmos
atuais. Os slots temporários são a única transformação ativa durante o evento;
a transformação de rota é contexto dormente. Ao terminar, liberar aliases e
marcar `placed=false` permite retomar a agenda pelo posicionamento/catch-up
existente. Não se sincronizam posições por frame nem se grava a posição final
do roteiro na rota. `hmbtn_session_finish_visit` concentra a limpeza anterior
de visita/flags/atores; controles e áudio ainda são restaurados pelo adapter.

Na chegada/infância, antes do handoff da vila, os atores são locais ao roteiro.
Em especial, IDs 1..4, 27 e 33 da lembrança não são identidades dos moradores de
mesmo número. Com vila inicializada, tour (12) vincula Thomas/Ellen/Zack e funeral
(172) Carter/Barley/Rick, liberando-os no encerramento. Torta e ingredientes
mantêm seus slots e passagens de controle anteriores.

## Empréstimos e persistência

| Recurso | Proprietário e validade |
|---|---|
| Bytecode principal e de conversa | Sessão; buffers de origem podem ser descartados após validar/copiar. |
| Jogador e atores em `village.player`, `event_actor`, `event_actors` | Aliases internos para a própria sessão; sobrevivem a unbind/rebind de recursos. |
| Bytes de agendas | Host; manter até rebind/unbind. Agenda decodificada e progresso pertencem à sessão. |
| `intro.ground` | Modelo decodificado do host; reassociar após substituir a área/backend. |
| `intro.game_command` / `game_context` | Empréstimo do adapter atual; unbind limpa ambos. |
| ANI, atlas, texturas, geometria, áudio e UI | Host/backends existentes; animações ainda emprestam bytes de arte, inclusive `event_block`. |

Antes de aposentar recursos, pausar o adapter e chamar
`hmbtn_session_unbind_resources`. Agendas/terreno/callback são retirados, sem
perder eventos, cursores, mensagens ou aliases internos. Reassociar recursos,
modelo e callback/contexto necessários antes de continuar a coordenação.
Rebind de agendas nunca reinicializa a vila após a primeira associação.
A sessão termina antes da liberação dos recursos na destruição da cena.
Copiar bytecode não permite liberar arte enquanto suas animações forem usadas.

Transições mantêm os registros, gatilhos de saída e regras atuais. A persistência
validada é em memória, entre ticks, UI, recursos e áreas. Saves em disco continuam
ausentes; nenhuma migração de formato foi feita. Checkpoints transacionais de
conversa, buffer de edição e mensagens copiadas para a apresentação conservam
papéis transitórios; não são fontes paralelas de estado de gameplay.

## Rastreabilidade e limites

A extração parte das implementações nativas existentes em `scene_preview.c` na
revisão `d3c0f38`, seguindo a base de MA-04a. Referências originais, preservadas:

- `src/main/game/farm_state.h`: `FarmState` e registros de atores/estado.
- `src/overlay/11_GF_FARM/farm_init.c` e `farm_main.c`: propriedade e coordenação.
- `src/overlay/11_GF_FARM/event/event_script.c`: `event_script_start`,
  `event_script_run`, `event_script_finish`, cursores e atores do evento.
- `src/overlay/11_GF_FARM/people/villager.c`: agenda e retomada de moradores.

Esta etapa transfere propriedade e elimina autoridade concorrente; não aumenta
o conjunto de comandos ou troca seus algoritmos por adaptações originais novas.
Nenhum módulo de `src/` passou a ser compilado diretamente. Comandos comuns,
regras e rotas originais continuam nas etapas específicas da matriz.

Callbacks específicos, controle, ações, sono, lojas e agendamento dos ticks
permanecem no adapter atual. `draw` ainda pode avançar a simulação; não prometer
renderização sem mutação ou gameplay completo headless nesta entrega. MA-05a
é a próxima implementação: pedido/resultado de recursos com identidade,
propriedade do buffer e resultados pendente/erro/sucesso explícitos. MA-05b e
MA-06..08 completam serviços, tick e separação gráfica.

## Validação

Linux x86-64 Debug com SDL3 3.4.18 local e `HMBTN_TEST_GAME_DATA` apontando para
`data/`, sem adicionar recursos proprietários ao Git:

- Antes da edição: configuração/build aprovados; 72/72 CTests em 44,34 s.
- Após a migração: configuração/build aprovados; 74/74 CTests em 44,07 s,
  zero falhas; 48 independentes dos dados opcionais e 26 percursos com dados locais.
- `native_session_events`: bytecode próprio/limites, origem sobrescrita, waits e
  cursor, entrada inválida sem perda, duas execuções independentes, aliases,
  conflitos/IDs locais, colisão/alvos/zonas, suspensão e retomada de rota,
  tour/funeral e rebind sem SDL.
- `native_session_events_playthrough`: duas sessões com visitas ativas reais,
  UI recriada, recurso de agenda substituído/liberado durante pedido pendente,
  cursor/mensagem/ator conservados, destruição da segunda sessão, recusa pelo
  script original e retomada de uma agenda real na área correspondente.
- Os 72 anteriores incluem visitas, torta/ingredientes, primeira manhã Sim/Não,
  nomeação/funeral, conversas/presentes/rollback, abertura, áreas, inventário,
  calendário, economia e transporte de galinhas. `native_session_state` foi
  atualizado para a propriedade interna dos aliases após unbind.

Não executados: Windows/cross-build, Release, Android, inspeção manual de janela/
áudio ou instalação limpa. Sanitizers não foram repetidos; faltavam runtimes
ASan/UBSan no ambiente já registrado. Os resultados não provam jogo completo,
fidelidade integral ou persistência em disco. `--diagnostics` não executa o jogo.

## Arquivos desta entrega

Implementação e build:

- `port/CMakeLists.txt`
- `port/game/session.c`
- `port/game/session.h`
- `port/game/village.c`
- `port/game/village.h`
- `port/graphics/scene_preview.c`

Testes novos:

- `port/tests/session_events_playthrough.c`
- `port/tests/session_events_test.c`

Percursos/testes existentes adaptados ao novo proprietário (mesmas asserções,
com exceção do contrato de aliases após unbind em `session_state_test.c`):

- `port/tests/calendar_playthrough.c`
- `port/tests/economy_playthrough.c`
- `port/tests/farm_feed_playthrough.c`
- `port/tests/farm_interiors_playthrough.c`
- `port/tests/farm_visit_playthrough.c`
- `port/tests/feed_shop_playthrough.c`
- `port/tests/first_morning_playthrough.c`
- `port/tests/fishing_playthrough.c`
- `port/tests/greg_rod_playthrough.c`
- `port/tests/ingredient_delivery_playthrough.c`
- `port/tests/inventory_playthrough.c`
- `port/tests/pie_quest_playthrough.c`
- `port/tests/poultry_helpers.h`
- `port/tests/poultry_lifecycle_playthrough.c`
- `port/tests/poultry_medicine_playthrough.c`
- `port/tests/poultry_playthrough.c`
- `port/tests/poultry_status_playthrough.c`
- `port/tests/poultry_transport_playthrough.c`
- `port/tests/session_playthrough.c`
- `port/tests/session_state_test.c`
- `port/tests/shipping_playthrough.c`
- `port/tests/village_gifts_playthrough.c`
- `port/tests/village_playthrough.c`
- `port/tests/village_talk_playthrough.c`
- `port/tests/world_clock_playthrough.c`
- `port/tests/world_map_playthrough.c`

Documentação:

- `docs/port/ARCHITECTURE.md`
- `docs/port/PORTING_MATRIX.md`
- `docs/port/PORTING_STRATEGY.md`
- `docs/port/ROADMAP.md`
- `docs/port/ROADMAP_PENDENTE_CODEX.md`
- `docs/port/ROADMAP_STATUS.md`
- `docs/port/SESSION_EVENTS.md`

`docs/port/SESSION_STATE.md`, já não rastreado no início desta entrega, foi
preservado byte a byte. Nenhum arquivo fora de `port/` e `docs/port/` foi alterado.
Os percursos ainda incluem `scene_preview.c` diretamente até MA-08c; transferir
seus caminhos para `session` não substitui essa futura migração de API.
