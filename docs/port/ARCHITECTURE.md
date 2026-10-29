# Arquitetura atual do porte

Estado inspecionado em 2026-10-28, incluindo alterações locais existentes.
Este documento descreve a implementação atual, não a arquitetura desejada.
A direção está em [PORTING_STRATEGY.md](PORTING_STRATEGY.md), o inventário de
funções em [PORTING_MATRIX.md](PORTING_MATRIX.md) e as próximas tarefas no
[ROADMAP.md](ROADMAP.md).

## Direção revisada do planejamento

A revisão de 2026-10-25 não altera a implementação descrita abaixo. MA-01..06
continuam concluídas; referências futuras a MA-07..25 identificam escopos do
plano anterior, substituído pelas entregas PF do roadmap. PF-01 (inventário/dinheiro) e PF-02 (primeiro cultivo/ciclo diário) foram
concluídas; PF-03 integrou comandos compartilhados e o convite de Duke.
PF-04 consolidou a coordenação em `game/runtime.c` e separou atualização/desenho,
inclusive título/cadastro. PF-05 entregou saves nativos estáveis; PF-06 entregou batata, grama/feno e culturas base de verão/outono; clima diário/chuva também integrado; sementes sazonais de Won, desbloqueios do supermercado e Pink Cat/Toy Flower integrados; Magic Red e suas variantes também integradas; Moondrop e visitas de Karen também integradas; carpintaria, galinheiro e primeira casa integrados como dependências da estufa, incluindo depósitos e núcleo da cozinha; obra/mapas/cochos do celeiro também integrados; aquisição/presença/alimentação de vacas e ovelhas também integradas; segunda casa e lareira de inverno também integradas; estufa com campo próprio e motor comum, obra, tempestade/reconstrução e save também integrados; produção/reprodução/ciclo completo ainda parciais; Orangecup também integrada; compra de ferramentas animais/escovação/leite/lã também integrados; venda de vacas e ovelhas também integrada; próxima entrega: reprodução bovina/ovina. Aquisição de utensílios e apresentação completa da cozinha continuam pendentes. Não há requisito de
concluir toda a reorganização antes de aproveitar uma função original.

A arquitetura atual tem uma base nativa reutilizável e acoplamentos por resolver;
não há motivo demonstrado para refazê-la inteira. Catálogos gerados com tamanhos
originais e limites de slots/mapas ainda existem. A ampliação futura requer revisar
esses consumidores: o runtime atual não passa a suportar novos conteúdos ou coop
por uma mudança documental. A estratégia determina separar limites de formato PS1
das capacidades configuráveis do jogo nativo.

## Escopo e situação funcional

Linux e Windows são os alvos atuais; Android é futuro. O executável nativo já
executa o título, cadastro, chegada à fazenda, infância, casa e recortes jogáveis
de fazenda/vilarejo. Há movimento, portas, ferramentas iniciais, coleta, menus,
relógio, vendas, lojas de alimentação, ciclo parcial das galinhas, conversas,
presentes, afinidade, pesca e alguns eventos de história. **O jogo completo ainda
não executa no host.** Saves nativos retomam exploração estável desde PF-05.
Clima completo, produção animal completa, outras espécies,
família, festivais e minijogos continuam incompletos ou ausentes.

O bootstrap `--diagnostics` não executa o jogo; `--window-test` não carrega seus
recursos. As opções de prévia são entradas de desenvolvimento para recortes do
mesmo runtime. Não confundir esses diagnósticos com o gameplay parcial normal.

## Build e uso do código decompilado

O CMake raiz seleciona `HMBTN_BUILD_NATIVE` antes de configurar o toolchain PS1.
O build nativo usa C11 e SDL3, sem SDK PSY-Q, execução MIPS ou emulador:

```text
port/core/main.c -> hmbtn_native
                         |
                     hmbtn_host
                         |
                   hmbtn_port_core + SDL3
```

Os targets estão em [port/CMakeLists.txt](../../port/CMakeLists.txt).
`hmbtn_port_core` reúne gameplay, leitores de formatos, renderizador por CPU e
decodificação de áudio; `hmbtn_host` integra aplicação, SDL3, arquivos, instalação,
interfaces, áudio em fluxo e coordenação das cenas.

**Nenhum módulo C de `src/` é compilado diretamente nesses targets.** O código
original serve de base às funções novas em `port/`; tabelas são extraídas para
includes no diretório gerado do build. O CMake lê layouts de `27_GF_TITLE/title.c`
e rótulos de `text_bank.c`/`text.c`; `port/cmake/generate_farm_tables.py` extrai
itens, culturas, regras de campo, posições, recursos de atores/ações, músicas,
datas, condições, reações a presentes e intervalos de scripts. Agendas também
são lidas dos recursos instalados. Esse reaproveitamento é mais amplo que apenas
os layouts iniciais.

Os scripts de eventos dos BINs instalados são interpretados por C nativo; não
são instruções MIPS. O CMake PS1 legado continua no repositório, mas compilá-lo,
reproduzir seus binários ou validar SHA-1 não é requisito do produto PC.

## Organização existente

| Diretório | Responsabilidade atual |
|---|---|
| `port/core/` | Entrada, CLI, loop SDL3, transição título/cadastro/cena, leitura LE e RNG adaptado. |
| `port/game/` | Estruturas próprias e recortes de regras/estados baseados no C decompilado. |
| `port/graphics/` | Decodificação, atlas, renderização 2D/3D, interfaces e coordenação do mundo em `scene_preview.c`. |
| `port/audio/` | VAB/ADPCM, SEQ, efeitos, mixagem e streaming/decodificação XA. |
| `port/filesystem/` | Leitura de intervalos, localização dos dados e resolução lógica de textos, eventos e ações. |
| `port/input/` | Teclado/gamepad para botões lógicos; hotplug, retenção de pressões e atalhos de janela. |
| `port/platform/` | Janela/renderizador SDL3, ícone e ferramentas de build host. |
| `port/installer/`, `packaging/` | Instalação dos recursos do BIN e distribuição sem dados proprietários. |
| `port/tests/` | Testes sintéticos e percursos opcionais com dados instalados. |

## Execução e apresentação

`core/app.c` recebe entrada SDL e usa `game/update_clock.c` para agendar
aproximadamente 29,97 ticks/s, independentemente da apresentação padrão de até
60 Hz. Bordas ficam pendentes até o tick e só são entregues uma vez; perder
foco limpa a entrada pendente. A cadência preserva comportamento do jogo,
sem exigir PS1 ou limitar a apresentação ao ritmo lógico.

`hmbtn_title_preview_tick` avança fluxo/cadastro/RNG/ANI/scroll;
`hmbtn_title_preview_draw` somente compõe/apresenta sua amostra visual.
`title_flow.c` e `new_game.c` conservam os algoritmos; o cadastro vive em
`HmbtnGameState.registration`, emprestado ao editor. Ao confirmar, a aplicação
copia somente `HmbtnRegistration` para a abertura, liberando os recursos do título.

`hmbtn_scene_preview_tick` fornece recursos decodificados a
`hmbtn_session_tick` em `game/runtime.c`. O coordenador possui as decisões de
roteiros, conversas, portas, transições, sono/manhã e a intercalação das fases
livres de `exploration.c`. Callbacks de eventos de animais/visitas e transações
foram retirados da cena. `HmbtnSession.runtime` guarda metadados de execução;
`daily.c` continua como observer diário único.

`graphics/scene_preview.c` possui os leitores/atlas/texturas/caches e fornece
os callbacks de recursos/efeitos. O frame só empresta seção/geometria/ANI durante
o tick, renovando referências nas cargas de área. `hmbtn_scene_preview_draw`
não recebe tempo/entrada e não avança estado. Modelos/portas/morphs decodificados
são atualizados no tick; caches/pixels podem mudar no draw. Isso separa a
execução da apresentação no recorte nativo atual; não completa sistemas ausentes.

`asset_decode.c` lê bancos e ANI e compõe atlas em CPU. `model_decode.c` lê TMD
prelit com offsets relativos; `model_render.c` implementa projeção inteira,
texturas afins, cores de 5 bits e ordenação das primitivas e sprites. Não há
tradução geral de todos os comandos GPU/GTE. Precisão GTE e bordas da GPU ainda
têm diferenças a comparar. `game/ground.c` consulta a geometria decodificada para
chão e colisão, sem depender de desenhar a cena.

`psx_audio.c` implementa bancos, amostras, sequência, vozes e mixagem. A saída é
SDL3; XA é lido e decodificado em fluxo. Interpolação/reverb e equivalência
completa à SPU continuam parciais. Preservar esses backends durante a migração.

## Estado e acoplamentos a migrar

| Dado | Proprietário atual | Problema de integração |
|---|---|---|
| Equipamento, mochila, mão, baú, geladeira, armário, capacidade e suprimentos | `HmbtnGameState.inventory` (MA-01) | Casa empresta `HmbtnInventory *`; alguns consumidores ainda recebem contexto de casa para dados não migrados. |
| Dinheiro, contadores de remessas e contas | `HmbtnGameState.economy` (MA-02) | Menu empresta `const HmbtnEconomy *`; remessa pendente e controle de coleta pertencem à mesma economia. Operação financeira compartilhada em `economy.c` desde PF-01. |
| Data/hora e contador de frames | `HmbtnGameState.clock` (MA-03) | Casa/loja/conversas emprestam referência const ao mesmo relógio; `sync_clock` foi removida. Regras de avanço continuam em `farm_clock.c`. |
| Nomes e aniversário do jogador | `HmbtnGameState.registration` (MA-03) | Editor recebe referência mutável; gameplay e apresentação recebem leituras. Buffer em edição é transitório. |
| Atores e eventos | `HmbtnSession.intro`, `.visit`, `.talk`, bytecode e atores, junto aos registros persistentes (MA-04b) | Uma consulta escolhe o ator ativo; rotas ficam suspensas durante overrides. Fases livres em `exploration.c`; callbacks e coordenação em `game/runtime.c` desde PF-04; renderer consulta a identidade ativa. |
| Campos, moradores e galinhas | `HmbtnSession.field`, `.greenhouse`, `.village`, `.poultry` | Dois campos distintos, mesmas regras; `hmbtn_session_field` empresta o campo da área. Registros/ticks no gameplay, recursos no host. |
| Construções | `HmbtnSession.construction` (níveis da casa/celeiro, estufa, tipo/prazo, RNG de tempestade); `poultry.upgraded` (galinheiro) | Loja/daily/scripts/mapa consultam os mesmos níveis; flags e pontuação derivados. |
| Lareira/madeira carregada | `HmbtnSession.house_lumber` (ação e entregas desde acordar); `village.flags[0x45]` (acesa nesta visita) | Inventário continua dono da mão e estoque; fadiga em `farm_action`. Quadros/tempo de textura são apresentação transitória. |
| Vacas/ovelhas | `HmbtnSession.livestock` | Registros e RNG próprios; capacidade deriva da construção, contagens/estágio/arte derivados. Loja/menu/cena emprestam, feno usa os cochos existentes. |
| Cozinha | `HmbtnGameState.cooking` | Utensílios e receitas aprendidas; casa e menu emprestam referências. Contagem derivada dos registros. |
| RNG | Estados em moradores, pesca, galinhas e gado | Moradores/pesca/gado usam fórmula original com estados locais; galinhas usam outro PRNG host. |

O estado original `FarmState` não é o estado de runtime do PC. Copiar seu layout
integral também não resolveria dependências de ponteiros, headers PSY-Q e buffers.
Ele é referência conceitual para o estado central, iniciado com inventário em MA-01, economia em MA-02 e calendário/cadastro em MA-03.

`game/event_script.c` valida blocos e comandos de 16 bytes, interpreta saltos e
impõe limite de instruções. Handlers estão separados em `farm_intro.c`,
`village_talk.c`, `farm_visit.c` e callbacks de `game/runtime.c`. Existem
operações de variáveis compartilhadas em `event_variables.c` (tipos 13..22),
consumidas por conversas/visitas e slots da abertura. Condições restritas a IDs
e limites explícitos para comandos não suportados ainda existem. Isso permite
executar roteiros reais já cobertos, mas ainda não é um interpretador completo.

Os percursos em `port/tests/` agora usam APIs públicas de tick e inspeção,
sem incluir `.c` de produção. `scene_inspection.h` fornece empréstimos explícitos
de sessão/recursos; `scene_fixture.h` somente adapta fixtures. Testes de cadência
comparam estados com 0/30/60 apresentações e verificam que draw não modifica a
mesma sessão nem as poses decodificadas. Fonte/limites no
[relatório PF-04](PF04_UPDATE_PRESENTATION.md).

## Inventário centralizado — MA-01

`port/game/game_state.h` define os tipos de domínio `HmbtnStoredTool`,
`HmbtnInventory` e `HmbtnGameState`, sem headers de UI/SDL. Cada
`HmbtnSession` possui um `HmbtnGameState`; a cena cria a sessão, e somente seu
construtor chama `hmbtn_game_state_init`. `HmbtnHouseInteraction` recebe uma referência emprestada,
sem os antigos campos/arrays de inventário. Mudanças de área e manhã não a recriam.

A seleção de slots mutável/const fica em `game_state.c`; operações de menu,
consulta/remoção/apresentação de itens e gatilhos recebem o inventário compartilhado.
Os consumidores que ainda precisam de hora/counters da casa acessam o mesmo
inventário por sua referência, sem outra cópia ativa. Clima é uma referência
emprestada a `HmbtnGameState.weather`, integrada no recorte meteorológico de PF-06. PF-01 moveu as regras de
troca/ciclo para `inventory.c`, junto a consulta/remoção e armazenamento de
ferramentas. Menus, conversas, medicina e coleta usam as operações compartilhadas;
a casa também usa a compactação comum. Operandos e notificações permanecem nos
adapters dos comandos. Detalhes em [PF01_SHARED_RULES.md](PF01_SHARED_RULES.md).

Descrições recebem o item selecionado como leitura, sem uma casa copiada com
equipamento alterado. Conversas mantêm um checkpoint para desfazer alterações
quando atingem ações não portadas; esse checkpoint não é fonte de consultas de
gameplay. Preservam-se também os registros temporários de itens em ações/voo.
Detalhes de inicialização, save/load, cobertura e arquivos em
[INVENTORY_STATE.md](INVENTORY_STATE.md). Field/village/poultry e execução de eventos
pertencem à sessão (MA-04a/b); contratos mínimos concluídos em MA-05 e
coordenação livre foi extraída em MA-06; roteiros e separação de atualização/desenho foram consolidados na PF-04.

## Economia centralizada — MA-02

`HmbtnGameState.economy` possui carteira, 25 contadores de remessas, quatro
pares de receita/despesa e todo `HmbtnFarmShipping` (valor pendente, peixes,
coleta realizada, item enviado e bônus do mel). O saldo inicial de 500 G é
instalado somente por `hmbtn_game_state_init`.

`HmbtnFarmMenu` consulta uma referência const. `HmbtnFarmStatistics` não possui
mais dados financeiros; seus contadores de animais/afinidade/etc. ainda aguardam
extração. A cena não possui outra remessa. Lojas, recompensa de evento e
`farm_shipping` alteram a economia compartilhada, sem sincronização ou espelho.

Preços, clamps, horários, datas especiais, rollover do ledger e ordem de efeitos
foram preservados. PF-01 adaptou `estate_gold_change` em `game/economy.c`; lojas,
remessas e recompensa de evento chamam a mesma operação, sem suas antigas
atribuições independentes de saldo/ledger. `gold_limit` permite um teto nativo
por sessão (zero usa o valor base), sem alterar o saldo inicial. O rollover de
contas continua na remessa; não é outra regra de débito/crédito.
Detalhes, caminhos e testes em [ECONOMY_STATE.md](ECONOMY_STATE.md).

## Regras compartilhadas — PF-01

[PF01_SHARED_RULES.md](PF01_SHARED_RULES.md) registra fontes, adaptações,
consumidores e testes. A sessão continua sendo a dona dos dados. A troca rápida
passa a respeitar a capacidade acessível, preservando células inativas; esta é
uma correção delimitada do host anterior, coberta por teste. Permanecem os
catálogos/arrays originais atuais e o interpretador parcial. Não há outro sistema
para itens novos nem promessa de mods completos; a colocação de registros é
independente do teto do catálogo, enquanto importação/UI serão ampliadas por
funcionalidade. Desde PF-05, esses dados sobrevivem também ao salvamento nativo em estados estáveis.

## Cultivo e ciclo diário — PF-02

`HmbtnSession.field` contém o progresso diário por célula (dias regados para
culturas comuns; dias sem necessidade de rega para grama), separado do catálogo
imutável `HmbtnFarmCrop`. A cena não tem cópia do crescimento. Nabo e batata estão jogáveis
do pacote comprado no supermercado (grupo de primavera na mesa, recorte do `feed_shop` existente) até a venda;
plantio usa ANI original e a grade extraída de `picture_grids[25]`. Cultivo/carry
chamam as mesmas operações de inventário/remessa; os estágios usam o pack visual
sazonal já carregado.

`game/daily.c` possui `hmbtn_session_clock_events`, `advance_minutes` e preparação
da manhã, com chave de data e handoff idempotente. Sono, tick normal e avanço de
evento usam o mesmo processamento. A cena não seca mais o campo separadamente.
Callbacks reais de vila/galinhas/ração/coleta foram conservados; família/
ajudantes não foram simulados. PF-06 acrescentou troca/previsão do clima e
irrigação após crescimento/produção/limpeza, seguindo `clock_events.c`. Desde PF-04, `game/runtime.c` coordena sono/manhã
e os providers host carregam os recursos e fornecem poses decodificadas; draw não
avança essa sequência. Detalhes do cultivo, diferenças e arquivos em
[PF02_CROP_CYCLE.md](PF02_CROP_CYCLE.md).

## Calendário e cadastro centralizados — MA-03

`HmbtnGameState.clock` contém o `HmbtnFarmClock` existente, incluindo frames até
o próximo minuto. Não há outro relógio atual na cena nem campos de data/hora
na casa. `hmbtn_house_interaction_init(house, state)` apenas vincula referências
a inventário, relógio e cadastro; recriar a interface não reinicia esses dados.
TV, calendário, HUD, fadiga das ações e scripts consultam a mesma data. A loja
também empresta o relógio, conservando a pausa durante seu fluxo modal.

`HmbtnRegistration` possui nomes de jogador/fazenda/cachorro e aniversário
zero-based. `HmbtnNewGame` conserva mensagem, escolhas e buffer em edição,
mas apenas empresta o cadastro. Inicializar diálogo ou editor não limpa nomes;
novo jogo inicializa o domínio explicitamente. O título possui seu estado
provisório e entrega somente os valores de cadastro antes de ser destruído;
a abertura recebe esses valores em seu próprio estado, sem ponteiro ao título.

Nascimento de animais, aniversário próprio dos moradores, cursores de calendário,
cache de agenda e iluminação continuam dados com significado específico; não
são relógios atuais paralelos. Clima/TV e coordenação ainda aguardam suas etapas; propriedade de eventos foi
resolvida em MA-04b. Nenhum algoritmo de avanço ou variável ainda não suportada foi
ampliado em MA-03. Detalhes e testes em [CALENDAR_STATE.md](CALENDAR_STATE.md).

## Ciclo de vida da sessão — MA-04a

`port/game/session.*` introduz `hmbtn_session_create`/`destroy`, sem SDL ou I/O.
`HmbtnSession` possui `game`, `field`, `village` e `poultry`. Esses registros saíram
do armazenamento inline da cena. Casa, menu, lojas, ações e scripts continuam
recebendo referências aos mesmos membros, sem cópias sincronizadas.

A criação conserva defaults e copia os valores do layout para o campo. Galinhas
e vila permanecem zeradas inicialmente, como antes; a vila recebe seus defaults
somente no handoff de recursos existente. `hmbtn_session_bind_schedules` inicializa
uma vez e, em associações posteriores, troca somente ponteiro/tamanho do recurso,
preservando agenda decodificada, afinidade, flags, contadores e RNG.

A cena possui o buffer de agendas; a sessão o empresta até rebind/unbind/destruição.
`hmbtn_session_unbind_resources` retira o buffer, o terreno e o callback/contexto
do adapter. Jogador, atores e execução agora pertencem à sessão (MA-04b) e
seus vínculos internos sobrevivem à retirada dos recursos. A cena
encerra a sessão antes de liberar seus recursos. O objeto de sessão não deve
ser copiado/movido, porque seus consumidores emprestam endereços de seus membros.

`HmbtnFarmField.quad`/`active` continuam caches de associação à geometria;
recarregar terreno pode reconstruí-los, sem reiniciar as células persistentes.
A sessão não possui texturas, áudio, renderer ou modelo de terreno. Detalhes
da entrega 04a em [SESSION_STATE.md](SESSION_STATE.md); os empréstimos de
atores descritos naquele registro histórico foram substituídos pela MA-04b.

## Execução e identidade de eventos — MA-04b

`HmbtnSession` possui `intro`, `visit`, `talk`, `dialogue`, `animal_name`,
`event_item`, gatilhos/bytes e metadados da primeira visita/eventos de animais.
A casa (`house`) também pertence à sessão, pois conversas emprestam seus
contadores e modo de interação. Construtores vinculam inventário/cadastro
ao mesmo estado. A sessão também possui controle, ações, menus e metadados de exploração desde
MA-06. Desde PF-04, a cena conserva os backends e `game/runtime.c` coordena os roteiros.

`hmbtn_session_open_event` valida e copia somente o bytecode para `event_code`
(3296 bytes), mantendo o interpretador existente. Conversas já possuem seu
próprio bytecode em `talk.code`. Cursors, slots, waits, pedidos e atores sobrevivem
ao rebind de recursos. Animações e texturas continuam emprestando o buffer de
arte do host: a cópia do bytecode não autoriza liberá-lo enquanto forem usados.

Vínculos são criados por `hmbtn_session_bind_event_actor`;
`hmbtn_village_actor` resolve evento singleton → evento por morador → rota.
Colisão, alvo, zonas de conversa e desenho usam essa identidade. A rota e sua
animação não avançam enquanto houver override. O registro persistente mantém
agenda e relacionamentos; a cópia inicial de posição é uma passagem de controle,
sem sincronização por frame. Encerrar libera os vínculos e solicita re-placement
pela agenda existente, sem gravar a posição do roteiro na rota. Crianças e
namorados da lembrança são IDs locais de roteiro, não os moradores de mesmo ID.
Tour e funeral vinculam explicitamente seus NPCs; visitas e ingredientes
selecionam os slots já usados por seus handlers.

**MA-04 concluída.** `hmbtn_session_event_tick` usa o interpretador sem SDL;
contratos foram introduzidos em MA-05 e a coordenação livre em MA-06. PF-04
acrescentou `hmbtn_session_tick` para todos os fluxos atualmente suportados.
Callbacks persistentes de gameplay recebem a sessão e consultam `active_frame`
somente durante o tick; o ponteiro temporário é limpo inclusive na falha.
`intro.ground`, geometria/ANI e agendas continuam empréstimos de recursos:
pausar antes de unbind e reassociar antes de continuar. A cena não agenda
roteiros durante draw. Isso não amplia a cobertura para o jogo original inteiro.
Origem da propriedade, testes e arquivos em [SESSION_EVENTS.md](SESSION_EVENTS.md);
contrato atual em [PF04_UPDATE_PRESENTATION.md](PF04_UPDATE_PRESENTATION.md).

## Contratos mínimos de serviços — MA-05

`game/services.*` define `HmbtnLogicalInput` e o protocolo sem SDL possuído por
`HmbtnSession.services`. Há canais para recurso, mensagem, animação de NPC,
ação do jogador e áudio. Cada pedido tem ID lógico, ticket monotônico e estado
explícito; um provedor adquire o pedido uma vez e pode responder depois. Polling
repetido e respostas antigas não repetem efeitos. Resultados terminais ficam
imutáveis até ack; recursos são copiados para buffers próprios do protocolo.

`filesystem/service_resources.c` atende IDs de eventos/gatilhos, ações e textos
com os leitores atuais. `audio/service_audio.c` atende música e XA com os backends
atuais. Os adapters são imediatos para leitura/início; o término de XA é observado
nos ticks. Um provedor externo pode adquirir um pedido e responder depois,
sem o adapter imediato disparar o mesmo efeito novamente. Não há threads novas.

A UI mantém sua abertura/seleção/animação atuais; seu fechamento responde ao
pedido de mensagem, incluindo escolhas de eventos e conversas. Animações reais
reportam o término do ator/ação com seu ticket. Sessão aplica a resposta uma vez
antes de continuar os handlers existentes. Carregamento de recurso e áudio só
libera o cursor após o consumidor aplicar e reconhecer o resultado, não apenas
quando os bytes ficam disponíveis. Erro/cancelamento são falhas explícitas.

Unbind preserva as intenções pendentes mas troca seus tickets e os republica;
respostas anteriores ficam inválidas. Resultados próprios já concluídos
sobrevivem. Novo evento/destruição limpa canais e libera seus buffers, mantendo
a sequência para não reciclar tickets. Chamadas e respostas ocorrem no thread
da simulação; não há promessa de acesso concorrente seguro.

Os backends e a construção/carregamento síncronos permanecem no host.
Desde PF-04, a coordenação de gameplay, incluindo portas/áreas e sono, está em
`game/runtime.c`, que usa providers e os serviços existentes. Nem todo acesso
gráfico ou efeito curto foi transformado em ticket; o contrato atende os
consumidores atuais, sem simular APIs PS1. PF-03 compartilhou comandos 13..22;
demais famílias serão ampliadas em PF-06 conforme os eventos reais.
Detalhes, testes, rastreabilidade e arquivos em [SERVICE_CONTRACTS.md](SERVICE_CONTRACTS.md)
e [PF04_UPDATE_PRESENTATION.md](PF04_UPDATE_PRESENTATION.md).

## Tick de exploração livre — MA-06

`game/exploration.*` contém `hmbtn_session_exploration_tick`, que recebe entrada
lógica e um `HmbtnExplorationFrame` com dados decodificados e serviços. O módulo
usa C e dados decodificados; arquivos, relógio host, áudio e renderer ficam nos
adapters. Controle,
ferramentas, coleta/carry/feed/gather, pesca, loja/menu e metadados de exploração
pertencem a `HmbtnSession`, sem espelhos na cena. Relógio e observador financeiro/
diário, moradores/galinhas, fadiga e ANI necessários ao gameplay avançam no domínio.

O tick chama as fases de controle, mundo e animação, na sequência anterior. O
coordenador existente usa essas mesmas funções nos pontos de intercalação
anteriores, sem conservar a implementação duplicada. `hmbtn_actor_animation_tick`
é a extração do algoritmo ANI já usado; o mesmo ENDED/JUMPED continua determinando
quando uma ferramenta, coleta ou transporte termina. Terreno e ANI são empréstimos
necessários mesmo sem desenhar. Renderizar mais vezes não avança essas fases.

Preparação de banco de ação/texto e de recursos de moradores usa o host síncrono
existente; ausência, pendência ou erro não são sucesso. Expansão/parsing de textos
ficam no domínio; `hmbtn_ui_message_art` só aplica a arte. Efeitos de apresentação
podem ser omitidos por um host sem vídeo/áudio; seus handles permanecem no host.

`HmbtnExplorationFrame` vale somente durante a chamada e deve ser reconstruído
após trocar uma área. A API específica `hmbtn_session_exploration_tick` continua
restrita à exploração livre e recusa roteiros/transições pendentes. Desde PF-04,
o tick integral `hmbtn_session_tick`, em `game/runtime.c`, intercala essas mesmas
fases com portas/viagens/sono/conversas. Providers renovam as referências após
carregar uma área. `app.c` agenda ticks e draw separadamente; draw não coordena
execução. A cobertura de conteúdo continua parcial.

Novos testes cobrem tick sem SDL e três sessões com dados reais a zero/uma/duas
apresentações por tick. Detalhes, origem, limites e arquivos em
[EXPLORATION_TICK.md](EXPLORATION_TICK.md).

## Recursos e instalação

O usuário fornece seu BIN no instalador nativo. O importador extrai os recursos
necessários, cria manifesto SHA-256 e usa preparação transacional/cancelamento.
`port/cmake/import_table.cmake` exclui os membros de código MIPS. O runtime lê a
pasta instalada; não executa o BIN nem exige Python no usuário final. Preservar
esse fluxo. Detalhes em [INSTALLERS.md](INSTALLERS.md) e [GAME_DATA.md](GAME_DATA.md).

Leitores LE e limites de intervalos permanecem necessários em PC: offsets e
endereços serializados continuam de 32 bits. Não reinterpretar buffers como
structs host, converter endereços do disco em ponteiros ou usar `long` como
inteiro PS1. Não existe uma tradução geral de memória PS1 no runtime.

## Validação e limites

A referência de configuração/build/CTest em Linux de 2026-10-25 é registrada no
[roadmap](ROADMAP.md#referência-inicial). Os testes cobrem formatos, entrada,
renderização, áudio, regras e percursos; os testes com recursos dependem de
`HMBTN_TEST_GAME_DATA`. Uma execução sem essa opção não tem a mesma cobertura.
CI Windows/Debug/Release está definida em `.github/workflows/native.yml`, mas
não substitui validação gráfica e instalação em Windows real.

Testes aprovados não comprovam gameplay completo nem equivalência integral ao
original. MA-01..03 migraram a propriedade de inventário, economia, calendário e cadastro, preservando
os algoritmos atuais; as demais etapas continuam planejadas. Os resultados
Linux Debug com recursos locais estão no roadmap por entrega. ASan/UBSan não
puderam ser executados em MA-01 por ausência de runtimes; Windows/Release não
foram validados.

A compra de sementes da PF-02 foi corrigida para `ZONE_SHOP_COUNTER` (37), com
seleção pelo registro/chave de cada grupo na mesa, não pela zona 36 de entrega
ao morador. `feed_shop.c` usa a tabela original gerada `shop_keys.inc` e os mesmos
registros de posição de `farm_gather`; a apresentação usa sprites sazonais desses
registros. O menu base de primavera oferece grama, nabo, batata e pepino,
com nomes/ordem/IDs/descrições extraídos de `shop_seed_menu_setup_a` e do banco
C original. A mesma sessão de loja coordena abertura → seleção → quantidade
(1..99) → resultado; o adaptador lê o banco de mensagens solicitado, inclusive
3:0x207 para envio ao baú. `inventory.c` possui uma única operação de compra de
lote: preenche equipamento/mochila em ordem ou envia o lote inteiro ao baú.
A função de um pacote delega à mesma regra; não há inventário/saldo de loja.
O desenho lê esse catálogo e só mostra nomes na lista; quantidade e total
aparecem depois da escolha, abaixo da lista para evitar sobreposição nativa.
O menu base de verão também está integrado, com tomate/milho/cebola e grama,
catálogo sazonal compartilhado e saudação 0x273. O menu base de outono também está integrado com berinjela/cenoura/batata-doce.
Morango/abóbora/espinafre e demais progressões/cultivos ainda estão pendentes. Batata, grama/feno e as culturas base
de verão/outono usam a mesma compra e o mesmo motor de campo.
Ver fontes, diferenças e validação da loja em
[PF02_CROP_CYCLE.md](PF02_CROP_CYCLE.md).


## Eventos reutilizáveis — PF-03

`game/event_variables.c/.h` adapta `event_var_get/update/arith/compare/clamp` e
os comandos originais 13..22. O contexto empresta o estado da vila, data,
cadastro, ator ativo e slots/tamanho da execução. Substitui os operadores
privados da conversa/abertura e os ramos equivalentes da visita, sem espelhar
progressão ou executar I/O. A tabela inteira de variáveis não foi portada;
consultas iniciais de sistemas ausentes conservam seus limites anteriores.

O evento 140 é o convite de Duke para a vindima, no primeiro ano, outono dia
14, pela tabela de entrada da fazenda. O mesmo `hmbtn_event_tick` executa as
18 instruções e falas originais nos dois ramos; não há roteiro novo hardcoded.
Aceitar escreve a flag 205 e executa as duas reduções de amizade do bytecode;
recusar não altera esses valores. A flag de visita 391 impede repetição.
A vindima/Cliff do dia seguinte são outros eventos e permanecem pendentes.

`session.c` fornece ao interpretador uma resolução emprestada dos bindings de
atores residentes. Duke usa `visit.actors[21]`, com uma única identidade ativa
consultada por scripts, direção, colisão e desenho; não foi criado um campo
novo por personagem. Movimento/reset também percorrem os bindings. A conclusão
libera o override e retoma a rota. Loader/ANI/texturas continuam nos adapters;
PF-04 levou o agendamento a `game/runtime.c` e migrou os testes para APIs públicas.

Fontes, diferenças, arquivos, testes e comando de reprodução em
[PF03_REUSABLE_EVENTS.md](PF03_REUSABLE_EVENTS.md). Linux Debug: **86/86 CTests**,
zero falhas, 52,10 s, com recursos locais. Sem validação Windows/Release ou
execução manual de janela/áudio nesta entrega.


## Atualização e apresentação — PF-04

`game/update_clock.c` acumula tempo/entrada para a aplicação. Um tick lógico
mantém o ritmo de referência (~29,97 Hz), independente da apresentação padrão
até 60 Hz; não é uma restrição de renderização PS1. Bordas são consumidas uma
vez, held persiste e perder foco limpa a entrada pendente. O catch-up é limitado
a oito ticks, preservando a proteção contra pausas longas.

`hmbtn_scene_preview_draw` e `hmbtn_title_preview_draw` não recebem tempo/entrada.
Título/cadastro avançam RNG, ANI, timers e scroll exclusivamente no tick; draw
usa amostras visuais capturadas na ordem original. O cadastro é transferido uma
vez para a abertura. Serviços de áudio continuam no host e são mantidos no tick.

A validação PF-04 passou **89/89 CTests** em Linux Debug, com comparativos de
zero/uma/duas apresentações por tick, transições, sono, cadastro e falha de carga.
Saves foram entregues posteriormente na PF-05; essa separação não implementa rede/coop nem
completa eventos/cultivos/espécies. Interfaces, arquivos, origem, resultados e
limites em [PF04_UPDATE_PRESENTATION.md](PF04_UPDATE_PRESENTATION.md).


## Saves nativos e retomada — PF-05

`game/save.c/.h` define disponibilidade e codec nativo sem SDL/arquivos.
A versão atual é **v11**, com depósitos, utensílios e receitas em GAME; prazo,
tipo de obra, níveis da casa/celeiro, estufa/RNG de perda e entregas de lenha do dia em AUXI, junto ao clima e à progressão de Moondrop. FILD grava ambos os campos, dimensões verificadas, estações e RNGs de plantio; não grava caches de geometria.
Vacas/ovelhas e seu RNG ficam na seção LSTK. PLAY não grava mais contagens de
gado: status as deriva da coleção. O leitor aceita somente v11: a compatibilidade entre versões de desenvolvimento
foi dispensada pelo usuário em 2026-10-27. Versões v1..v9/futuras são recusadas
com mensagem para iniciar jogo novo; nenhum arquivo antigo é apagado/consertado.
`menu.stats.building_score` foi removido do estado/codec: o status deriva a
contribuição dos níveis autoritativos. `menu.stats.recipes` também foi removido:
a quantidade de receitas é derivada de `game.cooking.records`. Somente
exploração estável entre ticks é salvável; eventos, diálogos, ações, transporte
animal, menus, portas/viagens e sono/fades bloqueiam. O save contém campos
explícitos de todos os sistemas cobertos, IDs de conteúdo, versão, tamanhos e
CRC-32; não contém imagens de `HmbtnSession`, callbacks, ponteiros ou tickets.
Decode constrói uma sessão nova com vínculos internos válidos e recursos soltos.

`filesystem/save_file.c` usa I/O UTF-8 SDL, temporário no mesmo diretório,
flush/close e rename para substituir o slot. O padrão é `farm.hms` na pasta de
preferências SDL, separado dos recursos BIN. `--save-file FILE` escolhe outro
slot e `--load-save FILE` inicia diretamente dele; previews continuam novos.

`graphics/scene_preview.c` recria recursos/agendas numa cena candidata sem
resetar a vila nem executar regras de entrada em área. Quads/caches/texturas
são reconstruídos. O host troca a cena somente após validação e primeiro draw;
erro mantém a sessão atual. Continue do título e Read diary usam esse carregador.
Dormir e salvar grava após atualização diária/fade da manhã, entre ticks, antes
de o gameplay prosseguir; erro é exibido e não anuncia sucesso.

Persistem chave diária/remessa, campo/cochos/coleta, animais/ovos/incubação,
rotas/flags/afinidade/RNGs, posição, stamina/experiência e contadores que afetam
ferramentas/controles. Progresso de música/vozes e efeitos visuais transitórios
não é retomado. Importação de saves PS1, troca de animais e scripts em andamento
não são suportados. Versões diferentes de 1/2 são rejeitadas; novas versões exigem
migração explícita, sem adotar layout MIPS ou novos limites do PS1.

Linux Debug: **92/92 CTests**, zero falhas, 123,82 s, incluindo processos distintos,
Continue real, cama, falhas e comparativos de estado por tick. Sem execução
Windows/Release ou inspeção manual em janela/áudio reais. Fontes, arquivos,
formato e limites em [PF05_NATIVE_SAVES.md](PF05_NATIVE_SAVES.md).


## Ampliação de culturas — PF-06, batata

O catálogo gerado já contém `CROP_POTATO`: 3 + 4 dias regados, estágio maduro 2,
sem rebrota, produto 0x42. `hmbtn_farm_crop_supported` agora aceita 0x23 junto
com nabo/pepino; plantio, grade, atualização diária e colheita continuam usando
as mesmas funções. Apresentação seleciona sprite 8, conforme o original,
independentemente do crescimento. Compra por 150 G e venda por 80 G consultam
catálogo/loja/remessa atuais, com os mesmos inventário e economia.

A persistência v1 já representa esses IDs/estágios. Um cenário compartilhado
com o nabo compra três pacotes, planta, insere noite seca, destrói/recria a sessão
por save no meio do crescimento e conclui a colheita/pagamento único. Outro
processo também inicia do arquivo. Nenhum novo proprietário de estado ou motor
de cultivo foi criado.

Linux Debug: **93/93 CTests**, zero falhas, 124,82 s; quantidade e maturidade
inspecionadas em capturas locais naquela entrega. Grama/feno foram integrados
no recorte seguinte; a cobertura geral PF-06 permanece parcial. Fontes, arquivos e limites
em [PF06_POTATO.md](PF06_POTATO.md).


## Grama e produção de feno — PF-06

Semente 0x36 usa o mesmo campo, plantio por ANI/grade e processamento de manhã.
O catálogo original define estágio maduro 3, 3 + 4 + 4 dias sem rega e rebrota
no estágio 1. A foice só produz feno na maturidade, devolve a planta ao estágio
de rebrota e ativa skip-day. Repetir a ação imediatamente não produz outra unidade.
Grama persiste no verão/outono e tem seu progresso reiniciado diariamente no
inverno conforme `field_daily_tick`; a primavera retoma o crescimento.

O corte incrementa `game.inventory.supplies[0]`, respeitando a capacidade inicial
999, e o celeiro retira/devolve desse mesmo estoque para abastecer os cochos.
Não há estoque privado do campo, da cena ou da alimentação. O codec v1 já
representa o progresso e o suprimento; não foi necessário mudar os saves.

O retexture calcula formas a partir dos vizinhos, seleciona os UVs do catálogo,
página 29 e paleta de outono/inverno, sem armazenar outro estado ou modificar o
crescimento. `scene_preview.c` exclui grama dos sprites de cultivos comuns.
O efeito de corte usa os sprites sazonais originais 14/19 do `estate_art`;
`exploration.c` avança a animação emprestada no tick, separada do desenho.

Linux Debug: **95/95 CTests**, zero falhas, 124,59 s; após ampliar fixtures
sazonais, **9/9 direcionados**, 2,66 s. Save/load foi comparado durante o ciclo e
outro processo abriu o arquivo final. Fontes, arquivos, reprodução, limitações e
próximo encaminhamento em [PF06_GRASS.md](PF06_GRASS.md) e no roadmap.


## Culturas base de verão — PF-06

Tomate 0x27, milho 0x28 e cebola 0x29 foram habilitados no mesmo campo. Catálogo,
atualização diária, plantio por ANI/grade, colheita, inventário, economia,
remessas e codec v1 são os existentes. Não há outro motor de crescimento ou
estado da loja. Os vetores do original levam a nove/quatorze/oito noites
regadas; tomate/milho rebrotam no estágio 3 com skip-day, cebola é removida.

`generate_farm_tables.py` extrai as listas base `shop_seed_menu_setup_a/b`.
`feed_shop.c` e a UI consultam `hmbtn_seed_shop_entries(season)` e
`hmbtn_seed_shop_entry(season,index)`, com o relógio da sessão emprestado à loja.
No verão a busca exige a chave 0x129 do registro 113 da mesa, diálogo 12 e
saudação 2:0x273. O balcão e os conjuntos de outras estações não abrem essa compra.
Abóbora continua condicionada à progressão ainda ausente do grupo B.

`hmbtn_estate_crop_look` adapta a seleção visual de `field_cell_look_*`: sprites
12/13/14 e faixa `stage+2` no verão; sprites 7/8/9 e faixa zero no outono.
O renderer consulta esse resultado sem aplicar regras de cultivo.
`load_area` compõe banco base da área e banco sazonal para reconstruir recursos
que antes dependiam de pixels deixados por uma visita anterior. A correção foi
necessária para carregar saves de verão em outro processo; não grava atlas,
não relaxa validação de dados e não altera gameplay.

Linux Debug: **99/99 CTests**, zero falhas, 128,11 s; **10/10 direcionados**,
5,90 s. Inclui crescimento, noite seca, rebrota, capacidade/saldo, carga comparada,
processo novo e pagamento adiado em data sem coleta. Fontes, arquivos, limites e
reprodução em [PF06_SUMMER_CROPS.md](PF06_SUMMER_CROPS.md).


## Culturas base de outono — PF-06

Berinjela 0x2C, cenoura 0x2D e batata-doce 0x2E usam o mesmo motor, catálogo,
observer diário, plantio por ANI/grade, colheita e remessas. Primeira maturidade
após 9/7/5 noites regadas; rebrota nos estágios 2/nenhum/1 respectivamente.
Skip-day e dias regados já existentes controlam a rebrota; não há outro timer.
Inventário, dinheiro e tempo permanecem nos proprietários compartilhados.

O gerador também extrai `shop_seed_menu_setup_c`. A loja aceita a chave 0x12A do
registro 114, usa saudação 2:0x274 e mantém os predicados de presença/horários.
A UI consulta a mesma lista, dimensionando a moldura ao maior nome importado;
quantidade/total aparecem depois da escolha. Espinafre continua pendente do grupo
C, cuja cadeia inclui pimentão.

`hmbtn_estate_crop_look` seleciona sprites 12/13/14 no outono, sem atualizar o
campo. O observer original adaptado limpa os cultivos na entrada do inverno,
enquanto grama mantém seu comportamento próprio. Save v1 conserva IDs/estágios/
dias/flags; nenhuma mudança de codec, inicialização ou instalação foi necessária.

Linux Debug: **103/103 CTests**, zero falhas, 134,40 s; **14/14 direcionados**,
10,68 s, após ajustar a moldura. Inclui save comparado no crescimento/rebrota,
outro processo, data sem coleta e sono real de Outono 30 para Inverno 1 seguido
de save/load da limpeza. Relatório e arquivos em [PF06_FALL_CROPS.md](PF06_FALL_CROPS.md).
Clima diário/irrigação por chuva é o próximo recorte; ainda não está implementado.


## Clima diário e irrigação — PF-06

`game/weather.c/.h` adapta `world/weather.c` (d3c0f38). A única autoridade é
`HmbtnSession.game.weather`: `today`, `tomorrow`, `random`. Casa e vila recebem
referências; não têm cópias de clima ou sincronizações bidirecionais. TV lê
previsão, ferramentas/pesca/galinhas leem o clima para suas regras existentes,
eventos consultam o clima atual; agendas recebem cinco flags derivadas, sem
forçar sol. Conversas e clima usam a mesma resolução dos aniversários alternativos.

O stream meteorológico usa o `psx_random` existente, com estado de 32 bits
explícito; reutiliza o algoritmo C e não depende de MIPS. É separado dos streams
já existentes de moradores/pesca/galinhas e do stream visual, conservando estes
consumidores. Seed inicial 1, previsão inicial calculada uma vez; carga de área,
menus e draw não recalculam. O preview de uma nova data tem inicialização explícita.

`daily.c` aplica crescimento/produção/limpeza antes de `hmbtn_weather_new_day`;
a chuva da manhã usa `hmbtn_farm_field_rain` e a mesma função de elegibilidade do
regador. A enxada durante chuva também deixa o solo regado, conforme
`field_request_dispatch`. Chuva não acrescenta crescimento instantâneo nem água
a grama estabelecida/inverno. Neve, tempestade e nevasca não usam esse callback.

`weather_effects.c` contém somente apresentação transitória: a densidade original
de quarenta partículas é ajuste visual, não limite de entidades/jogadores do PC.
Sprites originais 25/26, atrasos/movimentos e tonalidade são adaptados para o tick;
o renderer compõe os sprites antes da luz global, fades e UI, seguindo
`main_draw_ots`. O tick host projeta o deslocamento da câmera para mover apenas
partículas visíveis; draw não muda suas posições. O stream cosmético
não altera previsões e pode reiniciar após carga. As tabelas meteorológicas e
aniversários são extraídas pelo gerador, sem alterar `src/`.

Save v2 mantém as duas palavras de clima na posição de AUXI usada em v1 e
acrescenta o RNG ao final. V1 recebe seed 1 e flags meteorológicas derivadas;
não recalcula o clima/previsão gravado. As interfaces continuam emprestando
campos da nova sessão, sem ponteiros serializados. PNG, áudio/backend e BIN não
mudam. Limites, validação e arquivos em [PF06_WEATHER.md](PF06_WEATHER.md).


## Loja e culturas sazonais de Won — PF-06

`village_talk.c` adapta o comando original 81/dispatch 8 para iniciar a compra
(10) e esperar seu término. `session.c` fornece uma referência à mesma
`HmbtnFeedShop` da sessão. A loja modal tem prioridade sobre a conversa em
espera; seu fechamento retoma o bytecode. O checkpoint de inventário da conversa
avança após a compra comprometida para não desfazê-la numa eventual fronteira
posterior. Fora de uma sessão com adapter, comandos de loja continuam fronteiras
explícitas. Não há outro estoque, economia, proprietário de loja ou interpretador.

O grupo D é extraído de `shop_seed_menu_setup_d`. Antes do desbloqueio, as sete
linhas são repolho/abacaxi/pimentão/Pink Cat/Magic Red/Toy Flower/Orangecup, fonte
0/1/2/4/5/6/7. Após a visita 19, Moondrop aparece na linha visível 3 e as oito
linhas passam a corresponder à fonte 0..7. Nomes e descrições são preservados.
`HmbtnFeedShop.progress` empresta `HmbtnGameState.progress`; suporte técnico da
cultura não implica disponibilidade na loja. Orangecup é incondicional no grupo D, inclusive sem estufa e no inverno; estufa integrada abaixo. Os recibos/saída/cancelamento diferem dos de Jeff e são selecionados
pelo perfil do comerciante na mesma máquina de compra. Moedas, bolsas e baú
continuam compartilhados. `farm_field.c` habilita os IDs existentes e
`estate_art.c` resolve os sprites originais, sem novo algoritmo de crescimento.
Os IDs/estágios já cabiam em v2; v3 acrescentou o RNG de plantio, v4 a progressão de Moondrop.

A localização e o acesso seguem a agenda/conversa extraídas; o Won atual aparece
na estalagem (área 204, localização 27). Não foi implementado um balcão alternativo
ou spawn forçado. Sete percursos reais cobrem os vegetais e flores, incluindo ambas
as variantes de Magic Red: maturidade,
remessas/rebrotas quando previstas, presentes e persistência em outro processo.

Textos: diálogos e descrições são lidos em runtime dos arquivos instalados,
via `filesystem/text_resources.c`. Nomes de sementes e rótulos (incluindo
`Money`) são gerados de `src/main/text/text_bank.c` por CMake/Python e compilados.
O menu pode, portanto, ter nomes ingleses e falas portuguesas com um BIN traduzido.
Não foi implantada uma nova política de idioma ou extração dos nomes do executável.
Detalhes e validação em [PF06_WON_SEEDS.md](PF06_WON_SEEDS.md).


## Correção PF-06 — Áudio meteorológico e portas

`world_map.c` concentra a seleção de faixa/volume por área, clima e horário,
usada pelo runtime e pela carga. Chuva usa VAB/SEQ 22 em substituição à música
normal. O sampler, SDL3 e banco de efeitos existentes são preservados.
`exploration.c` resolve portas antes do arremesso, mantendo o inventário único.
A passagem do jogador conserva o recuo e os tempos ajustados pelo usuário.

`HmbtnSession.area_doors` controla as portas comuns acionadas pelas células
3/17/18 das rotas; a vila recebe uma referência. Atualização e som acontecem no
tick, sem avanço no draw. Modelos/folhas duplas têm uma única animação ativa;
a passagem do jogador assume exclusivamente seu modelo. O estado transitório
reinicia na troca de área/carga; ator/agenda/cursor de rota persistem no save v2.
Detalhes de retomada, fontes, testes e limitações em
[PF06_RAIN_DOORS.md](PF06_RAIN_DOORS.md).


## PF-06 — Câmera limitada, anexos de atores e sementes condicionais

A câmera volta a aplicar os limites também durante `door_tick`, mantendo os
movimentos de entrada do usuário. Itens carregados/exibidos são anexos do sprite
do jogador na mesma profundidade em `model_render.c`; cenário e moradores à
frente encobrem o conjunto. `house_ui` empresta ANI/atlas do cache existente,
sem outra carga de recursos ou estado de inventário.

`farm_shipping.c` deriva o desbloqueio sazonal das remessas autoritativas.
O gerador importa os grupos/limiar de `milestones.c` e linhas condicionais da
loja. `HmbtnFeedShop.economy` é uma referência à economia da própria sessão;
estoque e UI consultam a mesma condição, sem flags persistidas ou sincronização.
Morango/abóbora/espinafre usam o motor diário e a arte originais; save v2 não muda.
Fontes, critérios e validação limitada em
[PF06_CAMERA_SEED_UNLOCKS.md](PF06_CAMERA_SEED_UNLOCKS.md).


## PF-06 — Flores simples de Won

Pink Cat (`0x32` → `0x52`) e Toy Flower (`0x34` → `0x55`) reutilizam
`HmbtnFarmCrop`, campo, ações, atualização diária, inventário, compra e presentes.
Nenhum estado persistente, algoritmo de cultivo ou formato de save foi criado.
A seleção de Won conserva a entrada fonte após filtrar suporte, incluindo suas
próprias descrições `0x169`/`0x16b`. Produtos têm preço de venda zero; o caminho
existente de remessa recusa a operação sem modificar a economia.

`estate_art.c` resolve os sprites vivos 17/13 e murchos 37/41. A maturidade das
ANI desses dois cultivos usa explicitamente a faixa zero, como o repolho: o
original chega ao primeiro frame por uma indexação além das faixas declaradas.
O adaptador conserva essa aparência sem afrouxar a validação do decodificador.
Percursos compartilham o teste de Won e cobrem presente real para Lillia e
mudança de estação; detalhes e limites em [PF06_FLOWERS.md](PF06_FLOWERS.md).


## PF-06 — Magic Red, variantes e RNG persistente

`HmbtnFarmField.plant_random` pertence ao campo da sessão e inicia em 1. Somente
`hmbtn_farm_field_plant` consome essa sequência, para a semente `0x33`, depois de
validar solo/célula/estação. Adapta `field_event_dispatch`: `rand_range(25)==0`
grava `0x179` (vermelha), caso contrário `0x178` (azul). Reutiliza a aritmética
original já adaptada em `core/psx_random.c`, sem exigir sequência por frame PS1.
Outros fluxos aleatórios não mudam. O campo persiste a variante no ID normal;
não há uma segunda flag, cópia de estado ou escolha no desenho/carregamento.

Crescimento/colheita usam as definições originais no mesmo motor (outono,
3+3+2+2 dias regados, sem rebrota). Os sprites 17/25 compartilham a resolução
segura da faixa zero madura; inverno limpa as plantas. Produto azul `0x53`
não gera renda; vermelho `0x54` gera 200 G no sistema de remessas existente,
sem um contador dedicado no mapeamento original. Won tem seis entradas, com
Magic Red visível 4/fonte 5/descrição `0x16a`; Toy Flower passa à visível 5.
Na entrega de Magic Red, v3 acrescentou o RNG e leu v1/v2 com seed 1; o formato atual é v11, sem leitura de versões anteriores.
Fonte, arquivos e evidências em [PF06_MAGIC_RED.md](PF06_MAGIC_RED.md).


## PF-06 — Moondrop, visitas de Karen e progressão

`HmbtnGameState.progress.moondrop_seeds` representa o antigo bit 23 de
`estate.statusFlags`, separado de `village.flags[269/270]` (visitas concluídas).
Casa/loja emprestam esse campo. `hmbtn_event_progress_command` em
`event_variables.c` adapta 81/27 e atende visitas e conversas. A conversa mantém
um checkpoint de rollback, sem usá-lo como estado de gameplay. A entrega usou v4 para o booleano de AUXI. O codec atual v11 mantém esse campo, sem migrar versões anteriores.

Os blocos instalados 18/19 usam a mesma execução de visitas: Karen é vinculada
à identidade 3, com ANI/texturas originais e entrega pela API de inventário.
`HmbtnFarmVisit.prop` só representa a arte transitória do pacote, cuja ANI vem
da entrada 0x0831 do evento 18. Ela é publicada ao lado de Karen e removida
antes de armazenar um pacote; não é outro slot de inventário ou item persistido.
`farm_intro` carrega o modo musical pedido por esses scripts (5) pelo serviço
existente; o início da visita usa o áudio atual para evitar um modo antigo retido.

O gatilho empresta o campo em `HmbtnFarmTriggers.field`; condição 7 consulta
`hmbtn_farm_field_has_grown` por valor/estágio, ignorando chão como no original.
Uma busca booleana não exige o sorteio usado pelo original para escolher uma
posição, portanto não avança RNG. O gatilho atual continua consultando o campo externo; a estufa possui seu próprio campo, descrito abaixo.
A família atual é solteira; condições de outros papéis não são anunciadas como
implementadas. Afeição das candidatas e amizade dos demais moradores usam seus
campos próprios; a condição de espaço consulta ferramentas acessíveis.

Moondrop utiliza catálogo/ações/campo/diário/inventário/presentes atuais:
300 G, primavera, 2+2+2 manhãs regadas, produto 0x51, sem rebrota ou venda.
Sprites 12/40, faixa madura zero explícita. Precondições, ramos reais de espera,
limites e arquivos em [PF06_MOONDROP.md](PF06_MOONDROP.md).


## PF-06 — Carpintaria e galinheiro ampliado

`game/construction.c/.h` adapta `shop_lumber_deal_step` e
`milestone_arm_countdown`/`milestone_tick`. `HmbtnSession.construction` possui
`mornings_left`, `pending`, `house_level` e `barn_level`. O nível do galinheiro é exclusivamente
`HmbtnPoultry.upgraded`; madeira é `game.inventory.supplies[1]`, dinheiro/ledger
são `game.economy`. O pedido valida todos os recursos antes de debitar uma vez.
`daily.c` aplica a conclusão após clima, uma vez por data; o timer original 3
com teste `--timer < 0` corresponde a quatro manhãs, não três dias ou frames.
Ofertas/custo/prazo/limite da loja são extraídos de `src/` no configure.

O menu reutiliza `HmbtnFeedShop` e os mesmos serviços de texto/desenho. A conversa
374 de Gotz abre a loja por 81/8/9; 81/8/0 espera fechar e confirma o checkpoint
de inventário. Não há novo NPC, contador de madeira, dinheiro ou motor de loja.
A variável 38 agora lê o byte alto de `schedule[step][1]`, como
`villager_set_destination`, em vez de confundir destino com `place.value`.

`village.c:hmbtn_village_flag` deriva 0x38 (obra), 0x3A (galinheiro), 0x3B (primeira casa), 0x3C (celeiro), 0x3D (segunda casa), 0x39/0x3E (estufa) dos
proprietários; consultas compartilhadas de eventos, agendas/portas e gatilhos
usam essa interface. Os slots do vetor de flags não são espelhos de estado;
a interface recusa sua alteração direta quando há vínculo de domínio.
O comando 95 consulta os mesmos níveis. O status calcula 20 pontos por melhoria entregue,
sem armazenar outro total de construções.

`world_map.c:hmbtn_world_area_buildings` resolve os IDs lógicos 0/80/96/112:
fazenda 0..3, 4..7, 8..11, 12..15, 16..19 ou 20..23 conforme casa/galinheiro/celeiro/estufa; casa
80..83, 84..87 ou 88..91; galinheiro 112..115 ou 116..119; celeiro 96..99 ou 100..103; estufa 124..127 (lógico 124). O wrapper antigo delega a essa resolução. `scene_preview.c` carrega modelos,
zonas, colisões e texturas desse recurso; a composição sazonal usa a base da
variante, conservando as áreas especiais da abertura. A mesma seleção é aplicada
na carga de um save. Dez vagas/cochos e segunda incubadora usam `poultry.c` e
`farm_feed.c`, sem outras regras. `farm_menu.c` deriva a pontuação do estado.

As cinco ofertas originais de construção são funcionais, incluindo a estufa.
Após entregá-las, Gotz mantém a compra de madeira; perder a estufa reabre a
quinta oferta pelo mesmo controlador. Aquisição/capacidade/alimentação
bovina/ovina foram integradas no recorte base; ciclo animal completo segue parcial. Estufa e Orangecup foram integradas; demais espécies continuam abertas. Origem: [PF06_CARPENTRY.md](PF06_CARPENTRY.md) e
[PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md). A próxima fila está no roadmap.

## PF-06 — Casa ampliada, depósitos e cozinha

`game.inventory.icebox` e `.cabinet` armazenam os novos depósitos autoritativos.
`inventory.c` adapta as classes e trocas de `item_storage.c`, incluindo pilhas e
retirada de uma unidade; scripts usam a mesma interface. `house_interaction.c`
identifica geladeira (zona 4) e armário (23), reutilizando o modal do baú com
seletor de depósito. A tabela original de navegação, extraída no configure,
acomoda os três níveis de mochila. Nenhum array de itens pertence à UI.

`game.cooking` possui utensílios e registros de receita. `cooking.c` adapta o
casamento ordenado de receitas, grupos, bônus, consumo e aprendizado de
`menu/house.c`; `generate_farm_tables.py` extrai definições/nomes originais.
O domínio não depende de SDL nem de globais PS1. O menu transitório da casa
seleciona IDs e opções; confirma disponibilidade no inventário vivo e consome
mochila antes da geladeira, uma única vez. O resultado fica na mão compartilhada.
A apresentação usa o renderer de itens/textos já existente; mensagens vêm do BIN,
nomes de pratos/temperos das tabelas C originais. O parser de mensagens agora
interpreta escolhas de três/quatro opções além das duas já existentes.

`daily.c` e `house_sleep.c` usam posições originais da porta/cama conforme nível;
`runtime.c` apenas passa esse nível ao início do sono. A curva de porta ajustada
pelo usuário foi preservada. Carregar a casa ampliada não reinicializa campo,
inventário, móveis ou receitas.

Compra de utensílios pela TV, indicadores completos da ficha e animação/som do
preparo ainda não foram integrados. Nenhum utensílio é concedido ao concluir a
obra; receitas sem utensílios funcionam com ingredientes disponíveis. Detalhes,
matriz, arquivos e resultados em [PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md).


## PF-06 — Obra e abastecimento do celeiro ampliado

A terceira obra usa o controlador existente, acrescentando `barn_level` e o tipo
`HMBTN_BUILD_BARN`. `daily.c` conclui pela mesma chamada, sem outro prazo.
`world_map.c` aplica os offsets originais da fazenda e do celeiro; a cena usa a
mesma resolução para carga, restauração e localização de portas. A tabela de
nome `enlarge_barn` é extraída da fonte original no configure.

`feed.barn` e `game.inventory.supplies[0]` continuam como donos de cochos/feno.
`hmbtn_farm_feed_bins` consulta as zonas 41 da seção ativa para restringir a
apresentação aos cochos reais, sem armazenar outra capacidade. Cochos comuns
10→20 e maternidade 1→2; esse total não é o número de animais. Novos cochos usam
as mesmas ações/ANI/posições, retirada/devolução, transições e limpeza diária.

A entrega seguinte acrescentou a coleção nativa de vacas/ovelhas e aquisição/
alimentação reais descritas abaixo. A obra não concede animais gratuitamente:
Barley os vende pelo modal/editor existente. Alimentação executa antes de
`hmbtn_farm_feed_new_day`. O nível introduzido em v7 permanece no formato atual
v11 com validação dos pré-requisitos/obra pendente. Resultado, arquivos,
testes e limites em [PF06_BARN_EXPANSION.md](PF06_BARN_EXPANSION.md).


## PF-06 — Coleção de vacas/ovelhas e aquisição/alimentação

`game/livestock.c/.h` adapta os blocos de inicialização/posicionamento, consulta
livre, ciclo diário, fala/medicina e poses de `animals.c`/`livestock.c` originais.
`session.livestock` possui registros e RNG explícito; empresta `construction`
para capacidade 10/20 compartilhada entre espécies. Tipo por crescimento,
contagens e arte são derivados, sem replicar o nível do celeiro.

Barley usa o mesmo `feed_shop`/editor; parâmetros vêm do catálogo/textos originais.
Preços, cancelamento e vagas são validados antes do débito/criação. `daily.c`
visita vacas antes de ovelhas e consome os mesmos cochos antes de limpá-los,
sem segundo estoque ou observador. `poultry.c` generaliza a medicina já existente
para alvos bovinos/ovinos, mantendo o wrapper anterior para galinhas. Cross abre
mensagens originais com nome e marca conversa diária; não concede afeição a cada
abertura. `farm_menu` empresta registros e deriva listas/contagens. Os dois
contadores vazios de gado foram removidos de `stats`/codec.

`graphics/livestock_art.c` carrega o pacote original de slot 0 por TOC/offsets
extraídos no configure. Renderer e decodificador ANI aceitam escala explícita,
incluindo 0xCC8 da vaca jovem; consumidores anteriores usam escala padrão 4096.
Cenário/atores mantêm ordenação compartilhada. Movimento/ANI pertencem ao tick;
draw não atualiza regras. Sequência integral de cues/vozes e pastagem permanecem
parciais; produção de leite/lã e escovação foram integradas abaixo. Venda, reprodução e eventos de morte ainda ausentes. Registros
mortos permanecem reservados aguardando o fluxo de remoção/eventos.

Saves v11 serializam registros/RNG em LSTK e refazem vínculos, sem atlas/ponteiros.
Campo, construções e recursos existentes são preservados. Relatório, arquivos,
testes e alcance: [PF06_LIVESTOCK.md](PF06_LIVESTOCK.md).

## PF-06 — Segunda casa e lareira

`construction.house_level` agora cobre 0..2. A quarta oferta original (10.000 G,
750 madeiras) usa o mesmo débito, quatro manhãs e observador diário. Não existe
outro estado de casa nos mapas, lojas ou scripts. Nível 2 exige o celeiro entregue;
obra pendente e níveis incoerentes são recusados pelo codec v11.

`generate_farm_tables.py` extrai `house_door_positions` e `result_camera_pairs`;
`house_sleep.c` compartilha as posições com `daily.c`. Casa2 acorda em
(902,0,1804), com destino de cama (-2950,y,-2560). O handback do sono procura
modelos pela flag LINK/slot original: índices 1/2 na casa inicial, 2/3 na primeira
ampliação e 1/5 na segunda. O renderer reserva primitivas das alternativas
ocultas; não esconde cadeiras ou a outra cama para simular o sono.

`house_lumber.c` adapta retirada/devolução da zona 13 e entrega na zona 18.
O limite nativo de colheita já existente, 9.999 madeiras, foi centralizado em
`inventory.c:hmbtn_inventory_lumber_limit` e é compartilhado por corte/devolução.
O teto de compra da carpintaria permanece 999; uma retirada/devolução não reduz
um estoque nativo válido acima desse teto comercial.
Consome a mão ao terminar a animação original; no inverno da casa2, acende a
flag 0x45 e reduz cinco de fadiga somente na primeira entrega desde acordar.
Entrada em área apaga a flag, mas conserva o contador diário. Save/load preserva
ambos; acordar zera o contador. O tick host copia os três quadros originais de
textura da lareira conforme seus retângulos/durações. Draw não executa a ação.

Geladeira, armário, cozinha, calendário, TV, opções e diário usam as mesmas
interfaces/zonas. A segunda casa não concede utensílios, esposa ou filho.
Família e a aquisição de utensílios seguem pendentes. Fontes, testes e forma de
experimentar: [PF06_SECOND_HOUSE.md](PF06_SECOND_HOUSE.md).

## PF-06 — Estufa e dois campos no mesmo motor

`session.greenhouse` é uma instância distinta de `HmbtnFarmField`, com identidade
de layout `indoor`, 7×7 células e RNG de plantio próprio. `session.field` continua
45×25. Dimensões, índice, conversão mundo/célula e posição são consultas comuns;
`hmbtn_session_field` seleciona um empréstimo pelo ID lógico 0/124 e pelo estado
da construção. Não copia células para o campo externo ao entrar ou sair.
A capacidade do armazenamento em memória ainda é a do array nativo existente;
o layout define quantas células usar/salvar. Expandir além dessa capacidade
exigirá evoluir esse armazenamento, não respeitar um formato PS1.

`farm_field.c`, `farm_action.c`, `farm_carry.c`, controle e efeitos usam as mesmas
operações nos dois layouts. O campo interno ignora morte sazonal/inverno e rega
por chuva, conforme os ramos `which==1` originais; ambos avançam uma vez por data
no observador atual, mesmo ausentes da cena. Não existe um segundo algoritmo de
crescimento. Coordenadas internas usam origem (-1420,1020), células de 400;
o renderer usa o deslocamento +170/-170 de `greenhouse_draw` para os sprites.

A quinta oferta usa o mesmo `construction`, materiais/carteira/modal/prazo.
`construction.greenhouse` é a autoridade das flags 0x39/0x3E, comando 95,
100 pontos e mapas 20..23/124..127. O helper de mapas recusa entrar na estufa
ausente. `greenhouse_reset_level` foi adaptada para chance 31/100 e stream
`construction.damage_random`, salvo; perda limpa as células, conserva RNG de
plantio, reabre a oferta e mantém os demais níveis. Transporte/repatriação de
animais no interior ainda não faz parte desse recorte.

`generate_farm_tables.py` extrai UVs de `field/greenhouse.c` e associações de
sprites de `greenhouse_draw`; o host reutiliza `estate_art` no pacote 4, modelos,
texturas, colisões e portas instalados. `hmbtn_world_outdoors` também considera
a exceção original de PLACE_GREENHOUSE para não desenhar chuva/tintura interna.
O codec v11 grava os dois campos em FILD com dimensões, estação, células e RNG,
e construção/RNG de perda em AUXI; não salva quads/caches de recursos.
Orangecup está no filtro de suporte. Detalhes: [PF06_GREENHOUSE.md](PF06_GREENHOUSE.md) e [PF06_ORANGECUP.md](PF06_ORANGECUP.md).


## PF-06 — Orangecup

A habilitação de 0x35 usa exclusivamente as definições extraídas e o motor de
`farm_field.c`; Won automaticamente inclui a oitava entrada original do grupo
D pelo mesmo filtro. Compra custa 1.000 G, com nomes do C original e mensagens
dos recursos instalados. Na estufa, quatro estágios de dois dias regados geram
0x85 (60 G), sem rebrota. O distinto 0x56 não é o produto colhido e não tem preço
de venda. Sprite 24 do pacote 4 e faixas 3..6 já cabiam no leitor e na associação
extraída de `greenhouse_draw`. A entrega de Orangecup usou v10 sem mudar o esquema; a versão vigente v11 acrescenta os cuidados/ordenha abaixo, sem novo estado de cultivo.

A decompilação não impõe uma proibição específica de plantio externo: o motor
mantém a aceitação fora do inverno e a definição de estação 0. A apresentação
externa reproduz os casos presentes no C: primavera sem sprite, verão 18 e
outono 38. A incongruência entre crescimento de primavera e ausência de arte
não foi mascarada por um bloqueio novo; confirmação em execução do original
continua pendente. A estufa oferece o percurso de cultivo visível validado.
A produção de leite/lã foi integrada abaixo; próxima tarefa: venda de vacas e ovelhas.


## PF-06 — Ferramentas animais, ordenha e tosquia

`HmbtnFarmAnimal` em `session.livestock` é o único dono de `brushed_today`,
`milked_today`, `prize_winner` e do intervalo já existente `days_since_shearing`.
As flags diárias são limpas no observador diário existente; o retorno da lã
consulta a alimentação anterior antes de atualizá-la. Não há registro de
produção na cena nem cópias sincronizadas com menus. `livestock.c` adapta
`livestock_cow_cue`, `livestock_sheep_harvest` e `livestock_set_bit2_cue/_b`.
O estado atual não tem gestação ou ajudantes; os respectivos guardas devem
acompanhar a integração desses sistemas, sem flags fictícias agora.

`hmbtn_livestock_tool_tick` usa `HmbtnFarmAction`, com fase transitória de ação
animal e alvo por índice. Bancos originais 18/19/20, custo compartilhado e
conclusão ANI única; a entrega do produto usa `event_item.c`/`player_show_item`
já existente, sem outro inventário, carry ou renderizador. A apresentação
termina colocando o produto na mão uma vez; sua execução não depende de draw.
A fase permanece não salvável, como outras ações. O codec v11 salva os três
booleanos novos em LSTK, aceita somente v11 e conserva todos os demais donos.

`HmbtnFeedShop.blacksmith` seleciona o fluxo de Saibara no modal existente;
`inventory` é empréstimo de leitura para calcular o estoque a cada consulta.
O grupo compra escova/ordenhadeira/tesoura sem duplicar posse. Não aceita baú
como espaço de entrega; consulta e armazenamento usam a mesma busca de vaga
em `inventory.c`. A instrução original de melhoria é preservada; presentes,
máquinas e serviço completo de melhorias ainda não estão nesse menu parcial.
`village_talk.c` deixa o roteiro original decidir horário/disponibilidade,
abre a loja pelo dispatch 8/2 e espera seu fechamento no mesmo interpretador.

Nomes, ordem, IDs e descrições do submenu são extraídos de
`shop_tool_upgrade_menu_setup` por `generate_farm_tables.py`; preços e itens
já vêm de `farm_tables.c`. `feed_shop_ui.c` desenha as ferramentas com preço
por linha (flag 0 na fonte), usando os mesmos registros e sem mutar o domínio.
Ovelha tosquiada usa o pacote 8 já decodificado; quando a lã volta, o pacote 5.
As qualidades G consultam `prize_winner`, cuja aquisição jogável aguarda festivais.
Relatório e verificações: [PF06_ANIMAL_TOOLS.md](PF06_ANIMAL_TOOLS.md).

## PF-06 — Fidelidade do mundo e venda de animais

O destino serializado `0xfffe` de uma zona de porta é uma sentinela de porta
interna, nunca um endereço host ou uma seleção de área. `house_control.c`
reconhece a interação; `runtime.c` aplica as mesmas recusas de amizade/horário e
solicita `session.area_doors`. A pose transitória pertence ao tick e ambos os
lados usam o mesmo modelo. Portas de transição conservam seu fluxo anterior.

Partículas/luz usam pausa visual e flags do mapa, separadas da suspensão do
relógio por porta/diálogo/evento. O adapter aplica scroll também quando a câmera
é dirigida pelo roteiro; draw só consulta o estado. Coleta consulta o ponto à
frente em coordenadas do mundo, permitindo alvo na borda com os pés fora da
grade. `carry.thrown[].phase/timer` implementam HOLD/BLINK com a segunda faixa
ANI da figura, devolvendo pedras/galhos apenas a células livres do plantio e descartando fora da grade. Esses slots permanecem
transitórios e impedem save até a retirada; formato v11 inalterado.

Venda reutiliza `session.livestock`, `feed_shop` e a economia. O domínio deriva
elegibilidade/preço e limpa somente o registro confirmado; a UI consulta os
mesmos índices, nomes e ícones. `sale_cursor/top/slot/answer` já existentes são
seleção transitória. Não há coleção ou contador de vendas paralelo. Mortos
continuam ocupando vaga; reprodução futura precisa acrescentar o estado real
de gestação e seus guardas. Fontes, limites e reprodução dos testes:
[PF06_WORLD_FIDELITY.md](PF06_WORLD_FIDELITY.md).

A revisão de [29/10](PF06_INTERACTION_FIXES.md) integra a caminhada automática
de portas internas no `door_tick` existente, com sondagem de piso 0xc080 e sem
viagem de área. `area_doors` continua atualizando enquanto o controle manual
está suspenso. A seleção de Barley usa a mensagem comum 2/0x46; 2/0x6b é
cancelamento. O helper de cursor da loja aplica o deslocamento vertical de
seis pixels do original. Retorno de objetos ao plantio usa o mesmo campo,
catálogo e flag de atualização, sem coleção adicional ou mudança de save.
