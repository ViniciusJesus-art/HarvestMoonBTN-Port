# Arquitetura atual do porte

Estado inspecionado em 2026-10-25, incluindo alterações locais existentes.
Este documento descreve a implementação atual, não a arquitetura desejada.
A direção está em [PORTING_STRATEGY.md](PORTING_STRATEGY.md), o inventário de
funções em [PORTING_MATRIX.md](PORTING_MATRIX.md) e as próximas tarefas no
[ROADMAP.md](ROADMAP.md).

## Escopo e situação funcional

Linux e Windows são os alvos atuais; Android é futuro. O executável nativo já
executa o título, cadastro, chegada à fazenda, infância, casa e recortes jogáveis
de fazenda/vilarejo. Há movimento, portas, ferramentas iniciais, coleta, menus,
relógio, vendas, lojas de alimentação, ciclo parcial das galinhas, conversas,
presentes, afinidade, pesca e alguns eventos de história. **O jogo completo ainda
não executa no host.** Saves, clima completo, cultivos completos, demais animais,
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

`core/app.c` cria a janela, processa entrada e apresenta até 60 frames/s. A
simulação do título/cena acumula tempo para aproximadamente 29,97 ticks/s.
`graphics/title_preview.c` compõe o título; `game/title_flow.c` e `new_game.c`
adaptam o fluxo de edição. Os dados de cadastro ficam em `HmbtnGameState.registration`,
emprestados ao editor. Ao confirmar, a aplicação copia somente `HmbtnRegistration`
para a cena inicial, após liberar os recursos do título.

`graphics/scene_preview.c` possui `HmbtnScenePreview`, que cria/destroi uma
`HmbtnSession`, carrega áreas/eventos e integra os módulos do gameplay. `hmbtn_scene_preview_draw` também avança a
simulação; `opening_tick` coordena controle, conversas, lojas, pesca, ferramentas,
portas, sono e eventos. `world_clock_events` e `morning` conectam economia,
moradores, galinhas, cochos e campo às mudanças de tempo. Assim, a separação
entre lógica e apresentação **ainda não está concluída**.

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
| Equipamento, mochila, mão, baú, capacidade e suprimentos | `HmbtnGameState.inventory` (MA-01) | Casa empresta `HmbtnInventory *`; alguns consumidores ainda recebem contexto de casa para dados não migrados. |
| Dinheiro, contadores de remessas e contas | `HmbtnGameState.economy` (MA-02) | Menu empresta `const HmbtnEconomy *`; remessa pendente e controle de coleta pertencem à mesma economia. Regras financeiras ainda aguardam MA-16. |
| Data/hora e contador de frames | `HmbtnGameState.clock` (MA-03) | Casa/loja/conversas emprestam referência const ao mesmo relógio; `sync_clock` foi removida. Regras de avanço continuam em `farm_clock.c`. |
| Nomes e aniversário do jogador | `HmbtnGameState.registration` (MA-03) | Editor recebe referência mutável; gameplay e apresentação recebem leituras. Buffer em edição é transitório. |
| Atores e eventos | Moradores/galinhas persistentes na sessão; `HmbtnFarmIntro`/`HmbtnFarmVisit` ainda na cena | Execução e empréstimos de atores de evento aguardam MA-04b; comandos específicos por roteiro continuam. |
| Campo, moradores e galinhas | `HmbtnSession.field`, `.village`, `.poultry` (MA-04a) | Registros têm ciclo de vida sem SDL; carregamento/coordenação ainda ficam no adapter da cena. |
| RNG | Estados em moradores, pesca e galinhas | Moradores/pesca usam fórmula original com estados locais; galinhas usam outro PRNG host. |

O estado original `FarmState` não é o estado de runtime do PC. Copiar seu layout
integral também não resolveria dependências de ponteiros, headers PSY-Q e buffers.
Ele é referência conceitual para o estado central, iniciado com inventário em MA-01, economia em MA-02 e calendário/cadastro em MA-03.

`game/event_script.c` valida blocos e comandos de 16 bytes, interpreta saltos e
impõe limite de instruções. Handlers estão separados em `farm_intro.c`,
`village_talk.c`, `farm_visit.c` e callbacks de `scene_preview.c`. Existem
aritmética/comparações e acessos a estado repetidos, condições restritas a IDs
de eventos e limites explícitos para comandos não suportados. Isso permite
executar roteiros reais já cobertos, mas ainda não é um interpretador completo.

Vários percursos em `port/tests/` incluem `scene_preview.c` diretamente para
acessar estado e tick internos. Isso é evidência do acoplamento atual, não o
contrato desejado. Os percursos com recursos de MA-01..04a mantêm esse acesso privado
provisório até a API pública de sessão prevista em MA-08.

## Inventário centralizado — MA-01

`port/game/game_state.h` define os tipos de domínio `HmbtnStoredTool`,
`HmbtnInventory` e `HmbtnGameState`, sem headers de UI/SDL. Cada
`HmbtnSession` possui um `HmbtnGameState`; a cena cria a sessão, e somente seu
construtor chama `hmbtn_game_state_init`. `HmbtnHouseInteraction` recebe uma referência emprestada,
sem os antigos campos/arrays de inventário. Mudanças de área e manhã não a recriam.

A seleção de slots mutável/const fica em `game_state.c`; operações de menu,
consulta/remoção/apresentação de itens e gatilhos recebem o inventário compartilhado.
Os consumidores que ainda precisam de clima/hora/counters da casa acessam o mesmo
inventário por sua referência, sem outra cópia ativa. As regras de troca/ciclo
permanecem em `farm_menu.c`, aguardando MA-15.

Descrições recebem o item selecionado como leitura, sem uma casa copiada com
equipamento alterado. Conversas mantêm um checkpoint para desfazer alterações
quando atingem ações não portadas; esse checkpoint não é fonte de consultas de
gameplay. Preservam-se também os registros temporários de itens em ações/voo.
Detalhes de inicialização, save/load, cobertura e arquivos em
[INVENTORY_STATE.md](INVENTORY_STATE.md). A propriedade de field/village/poultry já é da sessão (MA-04a); execução de
eventos e coordenação ainda continuam na cena, aguardando MA-04b..08.

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
foram preservados. Ainda há aritmética financeira em lojas/scripts; consolidá-la
com `estate_gold_change` é MA-16, sem nova implementação paralela.
Detalhes, caminhos e testes em [ECONOMY_STATE.md](ECONOMY_STATE.md).

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
são relógios atuais paralelos. Clima/TV, execução de eventos e coordenação ainda aguardam suas etapas. Nenhum algoritmo de avanço ou variável ainda não suportada foi
ampliado nesta entrega. Detalhes e testes em [CALENDAR_STATE.md](CALENDAR_STATE.md).

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
`hmbtn_session_unbind_resources` retira referências ao buffer, ao jogador e aos
atores temporários do adapter, preservando os registros do domínio. A cena
encerra a sessão antes de liberar seus recursos. O objeto de sessão não deve
ser copiado/movido, porque seus consumidores emprestam endereços de seus membros.

`HmbtnFarmField.quad`/`active` continuam caches de associação à geometria;
recarregar terreno pode reconstruí-los, sem reiniciar as células persistentes.
A sessão não possui texturas, áudio, renderer ou modelo de terreno. A execução
atual de eventos, atores de roteiro, callbacks e tick permanecem na cena:
MA-04b e MA-05..08 ainda estão abertas. A MA-04 inteira **não está concluída**.
Detalhes, arquivos e testes em [SESSION_STATE.md](SESSION_STATE.md).

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
