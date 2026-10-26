# Ciclo de vida e registros da sessão — MA-04a

Entrega de 2026-10-25. Critérios e ordem no [ROADMAP.md](ROADMAP.md), diretrizes
em [PORTING_STRATEGY.md](PORTING_STRATEGY.md) e origem na
[PORTING_MATRIX.md](PORTING_MATRIX.md). O roadmap exige uma entrega por recorte:
**04a está concluída; MA-04 permanece aberta por 04b.** O próximo passo é MA-04b,
execução e identidade dos atores de evento, ainda não iniciada.

## Proprietário e ciclo de vida

`port/game/session.h` define `HmbtnSession`, com propriedade exclusiva de:

| Membro | Dados |
|---|---|
| `game` | Inventário, economia, relógio e cadastro migrados em MA-01..03. |
| `field` | Células do campo e metadados existentes de associação à geometria. |
| `village` | Registros dos moradores, afinidade, agendas decodificadas, flags, contadores, estado e RNG atuais do módulo. |
| `poultry` | Registros de galinhas e ovos, nascimento/saúde/alimentação/transporte e RNG atual do módulo. |
| `village_initialized` / `village_ready` | Inicialização única e disponibilidade do recurso de agendas, respectivamente. |

`hmbtn_session_create(layout, count)` aloca um objeto estável, inicializa `game`
e chama a inicialização existente do campo. O layout é copiado por valor,
sem reter o array. Não há SDL, arquivo, recurso gráfico ou áudio nessa criação.
O caminho `NULL, 0` conserva o comportamento existente de campo só de solo,
utilizado pelos testes sintéticos; o runtime fornece a tabela inicial já usada.

Galinhas e vila começam zeradas, como antes na cena. Os defaults da vila são
instalados somente no handoff em que os recursos de agendas chegam. Não houve
antecipação dos defaults/flags para a abertura. `hmbtn_session_destroy` aceita
NULL, remove empréstimos e libera o objeto, sem liberar buffers emprestados.

A sessão não pode ser copiada, movida nem serializada como struct. Consumidores
emprestam endereços dos seus membros. Dois objetos criados possuem registros e
estados independentes. A cena possui seu `HmbtnSession *` até ser destruída;
construção/destruição sem cena já estão disponíveis pela API de domínio.

## Integração e referências

`port/graphics/scene_preview.c` removeu os membros inline `game`, `field`,
`village`, `poultry` e o indicador `village_ready`. A criação da cena cria sua
sessão e vincula os consumidores aos membros dela:

- casa → inventário, relógio e cadastro em `session.game`;
- menu → economia de `session.game` e registros de `session.poultry`;
- loja → a mesma economia/inventário e o mesmo `session.poultry`;
- ações, scripts, coleta, sono e callbacks existentes → os módulos da sessão.

Não há espelhos desses módulos na cena nem sincronização entre cópias. As APIs
dos consumidores não trocaram seus algoritmos; a maioria das alterações no
adapter e nos percursos existentes é a mudança do caminho dos membros.
A inicialização de UI continua separada da inicialização do domínio.

Recriar casa/menu conserva os dados, inclusive o animal que está reservado
na mão do jogador. Carregar áreas conserva as mesmas células, moradores e
registros de animais. `field.quad` e `field.active` continuam caches derivados
da geometria atual: o carregador pode reconstruí-los com `hmbtn_farm_field_bind`.
Eles não são a persistência das células nem ponteiros host para bytes antigos.

## Empréstimos de recursos e ordem de liberação

| Dado/recurso | Proprietário | Empréstimo e validade |
|---|---|---|
| Layout inicial de campo | Tabela gerada já existente | Copiado na criação; pode ser descartado após a chamada. |
| Buffer de agendas (`schedule_block`) | Adapter de recursos/cena | `village.schedule_data` é leitura emprestada; manter válido até rebind/unbind/destruição. |
| Agenda decodificada de cada morador | Sessão | Continua nos registros mesmo ao reassociar o buffer de origem. |
| Modelo/bytes do terreno | Cena/backend atual | `field` guarda offsets/caches; `intro.ground` ainda é empréstimo da execução de eventos na cena. Reassociar campo antes de usá-lo com outro terreno. |
| Jogador e atores temporários de evento | Cena até MA-04b | `village.player`, `event_actor` e `event_actors` são empréstimos, não registros persistentes adicionais da sessão. |
| Texturas, atlas, animações decodificadas e áudio | Backends/cena existentes | Não transferidos para a sessão nesta etapa. |

`hmbtn_session_bind_schedules(session, data, size)` instala os defaults da vila
na primeira associação. Depois troca somente ponteiro/tamanho do recurso, sem
reinicializar moradores, flags, afinidade, contadores, RNG ou agendas em uso.
Recusa sessão/buffer ausentes e tamanho zero. Essa associação não valida o
conteúdo do arquivo de agendas: a leitura delimitada continua nas funções
existentes e no carregador. O teste unitário de associação usa bytes sintéticos
sem executar uma agenda; a integração usa o recurso instalado real.

`hmbtn_session_unbind_resources` retira o buffer e os empréstimos de jogador/
atores, desmarca disponibilidade e preserva o indicador de inicialização.
Os registros do domínio não são reiniciados. O adapter deve pausar o consumo,
reassociar recursos e estabelecer novamente seus empréstimos antes de continuar
as operações que dependem deles. A API não promete restaurar um evento ativo de
outro adapter: esse ciclo de vida pertence ao próximo recorte 04b.

A destruição de `HmbtnScenePreview` encerra a sessão antes de liberar agendas,
recursos de ator, terreno e demais buffers. Não há callback assíncrono novo
nem nova rotina de I/O. A inicialização parcial/falha de alocação continua
passando pelo mesmo caminho de liberação, com destruição de sessão NULL segura.

## O que permanece para MA-04b e etapas seguintes

`HmbtnFarmIntro`, `HmbtnFarmVisit`, conversas/cursores, atores de roteiro,
callbacks `game_command`/`game_context`, controle e a coordenação dos ticks ainda
ficam na cena. O mecanismo atual de cópias/overrides de ator não foi alterado
nesta entrega. A MA-04 só terminará quando 04b resolver a propriedade dessa
execução e explicitar o retorno à rota sem fontes autoritativas concorrentes.

MA-05..08 continuam responsáveis por contratos de serviços e separação de
atualização/apresentação. Criar uma sessão sem SDL não significa que o gameplay
completo já avança sem desenhar. O bootstrap `--diagnostics` continua sem
executar o jogo; o runtime tem os mesmos recortes jogáveis parciais.

Saves em disco permanecem ausentes. A entrega não muda formatos de recursos,
instalação pelo BIN do usuário, preços, algoritmos de campo/animais/moradores,
sequências de RNG ou calendário. Referências originais: revisão `4402de2`,
`src/main/game/farm_state.h` (`FarmState`), `src/main/game/farm_state.c`
(`farm_state_init`) e `src/overlay/11_GF_FARM/farm_init.c`/`farm_main.c`.
Foram movidas inicializações host existentes, sem compilar novas funções de
`src/`. Nenhum arquivo de `src/`, `sdk/`, `disc/`, `tools/` ou CMake PS1 foi alterado.

## Validação

Linux x86-64, Debug, SDL3 3.4.18 local e recursos instalados em `data/`:

- `cmake --preset native-debug`: aprovado; avisos opcionais SDL já registrados
  na referência inicial, incluindo libdecor ausente.
- `cmake --build --preset native-debug`: aprovado.
- Os 70 testes anteriores passaram após a migração: 44,20 s.
- `ctest --preset native-debug --output-on-failure`: **72/72 aprovados**, zero
  falhas, 44,16 s: 47 testes independentes de `HMBTN_TEST_GAME_DATA` e 25 percursos
  opcionais com os recursos locais. Sem o parâmetro, a cobertura é menor.
- `git diff --check -- port docs/port`: aprovado.

`native_session_state` cria/destrói sessões sem SDL, verifica independência,
layout copiado, defaults/handoff da vila, recriação dos controllers de casa/menu,
preservação de inventário/economia/calendário/cadastro, células e registros de
moradores/animais. Desassocia/reassocia o recurso de agendas sem reset e verifica
que o armazenamento emprestado continua pertencendo ao chamador.

`native_session_playthrough` cria duas cenas/sessões com recursos instalados,
obtém uma galinha pelo fluxo de ação existente e conserva a identidade do seu
registro através de UI recriada, pausa, desenho e carregamento de várias áreas.
Confere campo, afinidade/flags/contadores e estado compartilhado. Reassocia um
novo buffer real de agendas, destrói a outra cena e libera a galinha, mantendo
um único registro. Usa o acesso privado a `scene_preview.c` já existente nos
percursos; MA-08 ainda substituirá isso pela API pública de tick da sessão.

Permanecem aprovados os percursos de eventos/visitas/torta/ingredientes,
transporte/ciclo de galinhas e demais regressões. Isso protege o comportamento
atual, sem concluir sua migração de propriedade em MA-04b.

Não foram executados nesta etapa: Windows real/cross-build, Release, Android,
ASan/UBSan, instalação limpa ou inspeção manual de janela/áudio. A falta de
runtimes que impediu sanitizers em MA-01 permanece registrada; não houve nova
tentativa. Os testes não demonstram jogo completo nem paridade integral.

## Arquivos alterados nesta entrega

Lista em relação ao início da MA-04a, preservando alterações anteriores não
relacionadas. São 32 arquivos em `port/` (quatro novos: session.c/.h e dois testes)
e dez documentos (um novo). O ajuste dos testes existentes só muda o acesso
aos registros migrados.

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
docs/port/SESSION_STATE.md
port/CMakeLists.txt
port/game/game_state.h
port/game/session.c
port/game/session.h
port/graphics/scene_preview.c
port/tests/calendar_playthrough.c
port/tests/economy_playthrough.c
port/tests/farm_feed_playthrough.c
port/tests/farm_interiors_playthrough.c
port/tests/farm_visit_playthrough.c
port/tests/feed_shop_playthrough.c
port/tests/first_morning_playthrough.c
port/tests/fishing_playthrough.c
port/tests/greg_rod_playthrough.c
port/tests/ingredient_delivery_playthrough.c
port/tests/inventory_playthrough.c
port/tests/pie_quest_playthrough.c
port/tests/poultry_helpers.h
port/tests/poultry_lifecycle_playthrough.c
port/tests/poultry_medicine_playthrough.c
port/tests/poultry_playthrough.c
port/tests/poultry_status_playthrough.c
port/tests/poultry_transport_playthrough.c
port/tests/session_playthrough.c
port/tests/session_state_test.c
port/tests/shipping_playthrough.c
port/tests/shop_shortage_playthrough.c
port/tests/village_gifts_playthrough.c
port/tests/village_playthrough.c
port/tests/village_talk_playthrough.c
port/tests/world_clock_playthrough.c
port/tests/world_map_playthrough.c
```
