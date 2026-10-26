# Plano MA anterior — referência histórica

Arquivado em 2026-10-25 após esclarecer a direção do produto. **Não é a fila
executável.** MA-01..06 foram concluídas; MA-07..25 não foram executadas nem
concluídas e foram substituídas pelas entregas PF do [roadmap](ROADMAP.md).
As dependências sequenciais e regras de divisão abaixo estão desativadas.
Este registro preserva a intenção e os critérios técnicos anteriores.

### Propriedade do estado — preservar regras atuais

| Tarefa | Depende de | Entrega pequena e critério de conclusão | Validação mínima |
|---|---|---|---|
| MA-01 — Inventário autoritativo **concluída** | MA-00 | Introduzir `port/game/game_state.*` com os dados de equipamento, mochila, mão, baú, capacidade e suprimentos hoje pertencentes à casa. Casa/menu/ações/scripts usam referências ao mesmo inventário; retirar o armazenamento antigo, sem alterar algoritmos, preços, PRNG ou comportamento. Separar o tipo de item de headers de UI. | T-ESTADO; `native_house_interaction`, `native_farm_menu`, `native_farm_carry`, `native_event_item`, `native_event_tool_store`; percursos loja/cocho, Greg, presentes e ingredientes. |
| MA-02 — Carteira, contas e remessas **concluída** | MA-01 | Transferir dados financeiros de `HmbtnFarmMenu.stats` e estado da remessa para o domínio central. Menu passa a consultar; lojas e scripts usam a mesma carteira. Conservar regras financeiras atuais nesta entrega. | T-ESTADO; `native_farm_shipping`, `native_feed_shop`, percursos shipping, shortage, compra/venda de galinhas e torta. |
| MA-03 — Calendário e cadastro **concluída** | MA-02 | Tornar data/hora e cadastro únicos no estado central; remover a cópia mutável de calendário em `HmbtnHouseInteraction` e `sync_clock`. TV, calendário, scripts e nomes consultam a mesma origem. Não trocar o algoritmo de avanço ainda. | `native_farm_clock`, `native_new_game`, `native_house_interaction`, world-clock e first-morning; data exibida igual à consumida pelos scripts. |
| MA-04 — Propriedade da sessão e registros **concluída: 04a/b** | MA-03 | Introduzir ciclo de vida de `port/game/session.*`, transferindo propriedade dos módulos field/village/poultry e da execução de eventos da cena para a sessão. Manter algoritmos; documentar empréstimos de dados decodificados. Identidades/overrides de atores ficam explícitos, sem duas cópias autoritativas. | T-ESTADO; criar/destruir duas sessões, trocar área, transportar galinha e entrar/sair de eventos sem reinicializar ou duplicar registros. |

### Separação entre sessão e apresentação

| Tarefa | Depende de | Entrega pequena e critério de conclusão | Validação mínima |
|---|---|---|---|
| MA-05 — Contratos mínimos de serviços **concluída: 05a/b na mesma entrega** | MA-04 | Definir entrada lógica, pedidos/resultados de recursos, mensagem, animação e áudio apenas para os consumidores existentes. Adaptar os backends atuais; tipos públicos da sessão não incluem SDL. Resultados têm identidade e propriedade explícitas; erro/pendência não equivale a sucesso. | T-SERVICOS com resposta imediata/atrasada/erro, repetição de polling e buffers; testes de recursos/áudio existentes. |
| MA-06 — Tick de exploração livre **concluída: 06a/b/c** | MA-05 | Extrair para a sessão atualização de movimento, relógio, ações, pausa e menus da exploração livre. Consulta de chão/animação necessária ao gameplay funciona sem desenhar. Host agenda ticks; não alterar cadência ou ordem de efeitos. | T-SESSAO no recorte livre; controle, campo, carry, feed, fishing, menu e world-clock, com serviços sintéticos. |
| MA-07 — Tick de roteiros e transições | MA-06 | Extrair coordenação de abertura, mensagens, portas, visitas e sono de `opening_tick` para a sessão, preservando handlers atuais provisoriamente. Efeito de uma ação é concluído pela simulação/contrato, não pelo renderizador. | T-SESSAO/T-SERVICOS; infância, Thomas Sim/Não, funeral/nomeação, torta/ingredientes e sono; cursor/item/débito não repetidos. |
| MA-08 — Apresentação de leitura e API de testes | MA-07 | `scene_preview` fica com recursos/caches/desenho. Separar também tick do título/cadastro de seu desenho. `app.c` chama atualização e apresentação separadamente; testes de integração passam pela API da sessão, sem incluir `.c` de gráficos. Gameplay não inclui SDL, abre arquivo ou chama áudio/render. | T-SESSAO a 0/30/60 apresentações por segundo com os mesmos ticks; desenho sem mutação; CTests completos, título e prévias preservados. |

### Serviços comuns e interpretador de eventos

| Tarefa | Depende de | Entrega pequena e critério de conclusão | Validação mínima |
|---|---|---|---|
| MA-09 — Flags e propriedade do RNG | MA-08 | Centralizar acesso a flags e seeds. Adaptar `event_flag_get/set`; preservar representação se suficiente. RNG fica em serviço da sessão, conservando por enquanto fórmula e sequência de cada domínio, inclusive galinhas. Registrar divergências deliberadas; nenhum sorteio no render. | Flags em índices-limite e vetores RNG existentes; continuidade de seeds, sessões independentes e `native_village_affinity`. |
| MA-10 — Variáveis e aritmética de eventos | MA-09 | Adaptar `event_var_*` para acesso tipado ao estado; consolidar comparações, aritmética e clamps dos comandos já cobertos. Não transportar ponteiros PS1. Remover cópias equivalentes nos contextos migrados. | T-EVENTOS: signedness, divisão/resto extremos, sentinelas, índices e clamps; talk/visit/event-item existentes. |
| MA-11 — Despacho e fluxo comuns | MA-10 | Evoluir `hmbtn_event_tick`, sem segundo interpretador, para despacho único de slots/comparações/aritmética/retorno. Contextos de abertura/conversa/visita usam as mesmas operações; manter validação LE, orçamento e limites explícitos. | `native_event_runtime`; T-EVENTOS com mesmos comandos em contextos diferentes, saltos/STOP/PENDING e subrotinas. |
| MA-12 — Mensagens, espera e atores | MA-11 | Migrar famílias de mensagens, espera, posição e sprite para handlers comuns, uma família por entrega. IDs escolhem atores/recursos pelo contexto, sem cópia de regras por evento. Preservar escolhas e resultados de mensagem. | T-SERVICOS; abertura, conversas/presentes, Greg, Thomas, funeral e pintinhos; pausas e conclusão de animação. |
| MA-13 — Áreas, áudio e controle de eventos | MA-12 | Migrar pedidos de área/música/XA e bloqueio/liberação de controle para comandos comuns e contratos host. Retirar casos específicos somente após os roteiros correspondentes passarem. | Infância/XA, passeio, portas, visitas e funeral; erro de recurso mantém cursor/estado coerente, conclusão só uma vez. |
| MA-14 — Condições e gatilhos | MA-13 | Adaptar predicados de `event_triggers_fire`/`farm_event_available`, começando pelas condições já suportadas. Compartilhar datas, flags e consultas de inventário. Documentar condições ainda ausentes; não liberar todos os eventos sem seus sistemas. | `native_farm_visit`, torta/ingredientes e visitas; fronteiras de data/hora/clima, ano e disparo único. |

### Reaproveitamento das regras originais

| Tarefa | Depende de | Entrega pequena e critério de conclusão | Validação mínima |
|---|---|---|---|
| MA-15 — Regras de inventário | MA-14 | Portar primeiro consulta/remoção/armazenamento de `event_items_take`/`event_item_store`; em entregas seguintes, troca/empilhamento e ciclo originais. Menus, scripts, lojas e coleta chamam a mesma regra; retirar duplicações de cada recorte substituído. | Inventário/carry/menu/event-tool-store; capacidade, sementes, baú, slot inicial, Greg, presentes e ingredientes, com vetores de origem. |
| MA-16 — Regras financeiras e remessa | MA-15 | Adaptar `estate_gold_change`; depois `estate_shipment_add/settle` e grupos de produtos em recortes separados. Uma regra de saldo/ledger para todos os consumidores. Preservar diferenças só se documentadas e testadas. | Shipping/feed-shop/shortage; teto/insuficiência, cobrança única, receita integral, datas sem coleta e 17h58. |
| MA-17 — Integração das lojas existentes | MA-16 | Migrar primeiro o fluxo de um serviço de Lillia para comandos/regras comuns; depois os demais serviços já existentes e Barley. Preservar UI, textos e instalação. Não adicionar novas lojas nesta tarefa. | Feed-shop, poultry e medicina; presença/horário, cancelamento/nome, lotação, débito/estoque únicos e retorno ao mundo. |
| MA-18 — Avanço e agendamento do calendário | MA-17 | Adaptar `clock_advance_minute` e callbacks/agendamento sobre o calendário central, primeiro avanço normal e depois fast-forward/sono. Manter renderer de luz; não introduzir clima completo junto da refatoração. | Farm-clock/world-clock; 06h, pausa, estação/ano, sono longo, datas especiais, callbacks em ordem e uma única notificação. |
| MA-19 — Pipeline diário e contadores | MA-18 | Portar coordenação de pernoite e contadores a partir de `clock_events.c`/`event_request.c`, um callback por entrega. Garantir alimentação/produção antes da limpeza e efeitos financeiros/flags sem duplicação. Sistemas ausentes permanecem explicitamente ausentes. | T-DIARIO, event-days e poultry-lifecycle; acordar/sono/avanço de dias, ração, ovos, ledger e flags. |
| MA-20 — Primeiro cultivo completo | MA-19 | Adaptar crescimento de uma cultura a partir de `field_daily_tick`; depois colheita/regrowth/virada de estação em subtarefas. Reutilizar tabelas e ferramentas existentes; retirar regra antiga só para recortes cobertos. | T-DIARIO, field/carry/shipping; semear → regar → dormir → crescer → colher/vender, falta de água e estação inválida. |
| MA-21 — Regras das galinhas | MA-20 | Portar alimentação/ovos de `chicken_daily_tick`; depois crescimento/incubação, saúde/morte e cues em entregas separadas. Preservar identidade, transporte e nomeação; política RNG não muda incidentalmente. | Todos os testes e percursos de poultry; consumo antes de limpar, ovos únicos, três noites, sete noites, medicina, funeral e vaga reutilizada. |
| MA-22 — Agenda e rotas de moradores | MA-21 | Portar seleção/retomada de agenda; depois movimento de uma rota e ampliar por padrão. Conservar presença, atraso, colisões e devolução do ator ao terminar evento. | Village/mapa/interiores; bloqueio por 30 minutos, mudança de hora/data/área e retomada sem duplicar ator. |
| MA-23 — Relacionamentos já suportados | MA-22 | Portar ganho diário/negligência/decay de `family_flags_update` e `villager_affection_decay`. Usar comandos comuns para presentes/aniversários. Família/casamento completos viram tarefas futuras específicas. | Affinity/item-reaction/gifts; flags diárias, limites, exceções, aniversário alternativo e ganho diário único. |
| MA-24 — Pesca sobre serviços comuns | MA-23 | Adaptar/reconciliar espera e captura originais com flags, inventário, calendário e RNG comuns. Conservar animação, áudio e recursos; remover dependência da estrutura de UI. | Fishing/Greg; captura, berry, estampas, pré-requisitos, bolsa cheia, cancelamento e consumo de sorteios registrado. |
| MA-25 — Revisão e próxima fila funcional | MA-24 | Atualizar matriz com o que foi realmente portado e revisar lacunas. Decompor clima, espécies, família, festivais, minijogos, ferramentas, saves e distribuição em tarefas com fontes e dependências. Decidir separadamente a política RNG de fidelidade. Publicar a próxima tarefa no roadmap. | Toda a referência, builds/CTest Linux/Windows disponíveis, sem exigir PS1/SHA-1; registrar diferenças e plataformas não verificadas. |

A ordem coloca o inventário sob propriedade central antes do interpretador, mas
adia a troca de seus algoritmos até que os comandos tenham um contrato comum.
Isso evita dependência circular. A mesma distinção vale para calendário/economia:
primeiro transferir dados; depois portar as regras. Um agrupamento acima não
significa que sistemas não relacionados devam compartilhar uma implementação.

### Recortes de entrega

Para as tarefas agrupadoras abaixo, os sufixos identificam subtarefas, com o
estado indicado nas entregas e na tabela. Executar
`a` antes de `b`, e assim por diante, após a dependência da tarefa principal.
Os recortes podem ser executados juntos na tarefa integral autorizada. Cada
recorte deve passar os testes pertinentes da tarefa e conservar os demais
percursos; a tarefa principal só termina quando todos os recortes terminarem.
As condições descritas são seus critérios específicos, somados ao aceite comum.

| Tarefa | Subtarefas em ordem e conclusão observável |
|---|---|
| MA-04 | **04a concluída:** ciclo de vida possui field/village/poultry atuais e mantém seus valores ao trocar a UI. **04b concluída:** execução/identidade dos atores de evento pertence à sessão e volta à rota sem cópia autoritativa concorrente. |
| MA-05 **concluída integralmente** | **05a concluída:** recurso/resultado com IDs sem SDL, buffer próprio e erro explícito. **05b concluída:** mensagens, animações, ações e áudio com tickets e conclusão idempotente nos consumidores atuais; entrada lógica neutra. Ver [contratos](SERVICE_CONTRACTS.md). |
| MA-06 **concluída integralmente** | **06a concluída:** movimento/colisão no tick da sessão. **06b concluída:** ferramentas/coleta/transporte e ANI independentes de desenho. **06c concluída:** relógio/observadores e pausa de menus na mesma sequência anterior. Ver [tick de exploração](EXPLORATION_TICK.md). |
| MA-07 | **07a:** abertura/infância avançam pelo tick público. **07b:** portas e viagens mantêm posição/controle sem depender de desenho. **07c:** visitas/conversas/eventos de animais usam o mesmo tick. **07d:** sono e manhã mantêm a ordem atual de efeitos fora de gráficos. |
| MA-08 | **08a:** render de cena só consulta o estado e atualiza caches visuais. **08b:** título/cadastro têm tick separado. **08c:** percursos deixam de incluir `.c` de gráficos e exercitam a API pública. |
| MA-09 | **09a:** acessos a flags passam por uma interface comum com limites testados. **09b:** seeds pertencem à sessão e preservam as sequências atuais sem consumo na apresentação. |
| MA-10 | **10a:** aritmética/comparações usam algoritmos comuns com vetores de extremos. **10b:** variáveis/flags/calendário já cobertos usam os mesmos acessores tipados em todos os contextos. |
| MA-12 | **12a:** mensagens/escolhas têm handler comum e resultado único. **12b:** esperas/retornos de animação conservam os ticks do roteiro. **12c:** posição/sprite resolvem atores por identidade e não por cópia específica de evento. |
| MA-13 | **13a:** carregamento/transição de área preserva cursor e conclui uma vez. **13b:** música/XA usam pedidos/resultados existentes sem avançar prematuramente. **13c:** bloqueio/liberação de controle migram e passam os roteiros antes de retirar os casos antigos. |
| MA-14 | **14a:** filtros de flags/data/hora/clima reproduzem fronteiras originais já cobertas. **14b:** condições de disponibilidade/inventário e disparo único usam esses filtros, mantendo ausências explícitas. |
| MA-15 | **15a:** consultar/remover usa `event_items_take` adaptado. **15b:** armazenar usa a regra de `event_item_store`, com capacidade e notificação separadas. **15c:** troca/empilhamento seguem `storage_*`/`item_*`. **15d:** ciclo/compactação de equipamento e itens usa o algoritmo original em todos os consumidores. |
| MA-16 | **16a:** `estate_gold_change` adaptado atende débitos/créditos e ledger. **16b:** adição/grupos de remessa seguem `estate_shipment_add`. **16c:** liquidação/datas/pagamento seguem a regra original uma única vez. |
| MA-17 | **17a:** compra de ração de Lillia usa serviços comuns. **17b:** compra/venda/nome de galinhas preservam cancelamento e cobrança única. **17c:** medicina e serviços já existentes de Barley usam as mesmas regras, sem duplicar predicados. |
| MA-18 | **18a:** avanço normal/calendário e pausas correspondem ao original. **18b:** fast-forward/sono emitem as mesmas notificações ordenadas, sem duplicar datas. |
| MA-19 | **19a:** contadores mantêm sentinelas e condições originais. **19b:** ledger/callbacks horários são centralizados. **19c:** pernoite conecta callbacks reais na ordem revisada, com consumo/produção antes da limpeza; ausências não são callbacks de sucesso fictício. |
| MA-20 | **20a:** uma cultura cresce por irrigação/dia conforme `field_daily_tick`. **20b:** sua colheita entra no inventário e economia comuns. **20c:** regrowth quando aplicável e virada de estação são testados antes de ampliar a outras culturas. |
| MA-21 | **21a:** alimentação/ovos usam a regra diária portada. **21b:** incubação/nomeação/crescimento mantêm identidade e vagas. **21c:** saúde/medicina/morte/funeral seguem regras portadas. **21d:** cues/movimento/repouso são reconciliados sem perder transporte e sem alterar RNG incidentalmente. |
| MA-22 | **22a:** seleção/retomada de agenda porta os algoritmos originais. **22b:** uma rota porta movimento/espera/atraso; ampliar somente a padrões testados, mantendo handback de eventos. |
| MA-23 | **23a:** ganho diário/negligência conserva flags, exceções e clamps. **23b:** decay e integração com presentes/aniversários usam as mesmas regras, sem simular família inexistente. |
| MA-24 | **24a:** espera/captura usam calendário, RNG e flags comuns com vetores originais. **24b:** ação/recebimento/cancelamento integra inventário sem duplicar produto, berry ou estampa. |

## Critério comum de conclusão

Uma tarefa MA só termina quando seu recorte está integrado aos consumidores
reais, a cópia antiga de estado/regra foi removida, a origem está rastreada e os
testes pertinentes passaram. Atualizar este arquivo, a matriz e a arquitetura
na mesma entrega. Não concluir uma tarefa de portabilidade apenas com mudança
de nomes ou movimentação de arquivos; em tarefas de propriedade, demonstrar a
fonte única de dados. Em tarefas de algoritmo, demonstrar sua correspondência
com o original e registrar diferenças.

Os grupos T-ESTADO/T-SESSAO/T-SERVICOS/T-EVENTOS/T-DIARIO são definidos na
[matriz](PORTING_MATRIX.md#testes-novos-exigidos-pela-migração) e são requisitos por etapa. T-ESTADO para o inventário já tem cobertura em
`native_inventory_state`/`native_inventory_playthrough`; os demais recortes continuam planejados. Somar esses testes à proteção existente no recorte que os
introduz. Preservar diagnósticos, prévias, sequência normal, leitores e fluxo BIN.

