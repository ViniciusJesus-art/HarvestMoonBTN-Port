# PF-04 — Atualização independente da apresentação

Implementação de 2026-10-26 sobre a árvore local com MA-01..06 e PF-01..03.
Alvos: Linux/Windows, Android depois. O estado jogável avança por ticks explícitos;
desenhar não agenda simulação nem consome entrada. A entrega não adiciona
saves/conteúdo e não declara o jogo completo. `--diagnostics` continua sendo
somente o bootstrap de diagnóstico.

## O que foi preservado e extraído

A referência original continua `d3c0f38`: `farm_main.c`/`farm_frame.c`,
`event_script.c`/`event_commands.c`, `doors.c`, `clock_events.c`, `title.c` e
as adaptações nativas existentes. Foram conservadas as decisões, ordem de
processamento, regras e recursos dos percursos anteriores; não foi escrito
outro interpretador, sistema de transições ou atualização diária.
Nenhum módulo de `src/` passou a ser compilado diretamente.

`port/game/runtime.c/.h` recebe a coordenação antes mantida em
`port/graphics/scene_preview.c`:

- tick de abertura, infância, primeira visita, eventos, conversas e nomeação;
- predicados de início, comandos específicos de visitas/animais, recompensa e
  entrega de itens, conclusão e retomada dos controles/rotinas;
- portas, animação, viagem/troca de área, sono, fade e preparação da manhã;
- mensagens, resultados dos serviços, preparação/espera de ações e animações;
- a mesma intercalação das fases livres já extraídas para `exploration.c`;
- entrada de área, limpeza de flags transitórias e handoffs de recursos;
- metadados de execução (mensagem/célula carregadas, ticks, música lógica e
  progresso do volume) em `HmbtnSession.runtime`.

`HmbtnScenePreview` conserva buffers, atlas, texturas, decodificadores, caches,
leitores e áudio. Seus callbacks fornecem recursos e efeitos. Transformações
visuais decodificadas de portas/modelos e morphs são empréstimos atualizados
no tick; não avançam no desenho e não exigem um renderer para a coordenação.
Os leitores de formato original e seus limites de validação foram preservados.

## Entradas e propriedade

| Interface | Responsabilidade |
|---|---|
| `hmbtn_session_tick(session, frame, input, error, size)` | Um tick completo da sessão. Nenhum SDL, arquivo aberto, framebuffer ou função de composição no coordenador. |
| `HmbtnSessionFrame` | Empréstimos de seção/geometria/ANI e callbacks de recursos/efeitos. O provider atualiza as referências após carregar outra área. Não possui inventário, dinheiro ou progressão. |
| `hmbtn_scene_preview_tick` | Adapter público de um tick: atualiza animações decodificadas de cenário, fornece o frame ao gameplay e mantém a saída de áudio. |
| `hmbtn_scene_preview_draw` | Pixels/caches/upload/desenho; a assinatura não recebe tempo nem botões. |
| `hmbtn_title_preview_tick` / `hmbtn_title_preview_draw` | Atualização e apresentação separadas de título/cadastro. Flow/editor continuam em `title_flow.c`/`new_game.c`. |
| `HmbtnUpdateClock` em `game/update_clock.c` | Acumulador usado pela aplicação: coleta entrada, entrega bordas uma vez e agenda ticks independentemente do draw. |
| `scene_inspection.h` / `hmbtn_title_preview_inspect` | Visões emprestadas para hosts/ferramentas/testes, sem expor o struct privado da cena. Fixtures editam o estado autoritativo por acesso explícito à sessão. |

O contexto temporário de frame só fica em `session.active_frame` durante o
chamado público e é retirado também na falha. Callbacks persistentes de eventos
referenciam a sessão, sem reter ponteiro para stack nem para o renderer.
Rebind de recursos restaura os handlers de gameplay com esse contrato; o dono
host conserva os buffers e sua duração. Não há cópias de inventário/calendário
ou sincronização manual entre sistemas.

As operações explícitas de inspeção/carregamento/rebind existem para ferramentas
e integração. O fluxo normal do jogador usa portas/eventos via tick. Elas não
adicionam opções técnicas aos menus do produto.

## Agendamento e título

`core/app.c` recebe a entrada SDL, alimenta `HmbtnUpdateClock`, executa zero ou
mais ticks e depois apresenta uma vez. O ritmo lógico de aproximadamente
29,97 Hz foi conservado como comportamento de referência do jogo; não é um
limite de FPS de apresentação ou uma necessidade de emular VSync/PS1.
A apresentação mantém o padrão existente de até 60 Hz. O tamanho de lote de
catch-up é limitado a oito ticks para evitar uma pausa host virar uma longa
simulação acumulada, como na proteção anterior da cena.

Uma borda recebida entre ticks continua pendente até o próximo tick. Em um
lote de vários ticks ela só aparece no primeiro; botões segurados continuam
nos demais. Perder foco limpa a entrada pendente da aplicação. Os acumuladores
e caminhos de entrada antigos dos dois renderizadores foram removidos.

No título, RNG, sprites, timers, scroll e cadastro avançam no tick, sem compor
pixels. A apresentação usa amostras visuais capturadas na ordem original,
incluindo a posição anterior ao avanço de ANI/scroll e o frame visual do cursor.
Múltiplos desenhos não sorteiam números nem avançam animações. A transferência
de cadastro continua copiando apenas `HmbtnRegistration` antes de destruir os
recursos do título, aplicando esses valores à abertura recém-criada.

## Recursos, falhas e limites

Os tickets e resultados de serviços MA-05 foram conservados. Esperas de
mensagem/ANI/áudio e cargas de eventos continuam sem repetir efeitos. Ausência
de um provider obrigatório ou falha de leitura retorna erro; não inventa
conclusão de área/ação. Cargas síncronas de área seguem sendo síncronas: ao
receber `false`, o host deve parar/tratar o erro, como a aplicação já faz;
esta entrega não promete retomada arbitrária de toda falha de I/O.

O frame usa dados nativos decodificados de geometria/ANI; não é uma emulação
PS1. Providers de apresentação podem manter caches e carregar arte. O draw
muda somente apresentação/caches, sem modificar estado de gameplay ou as poses
decodificadas. A coordenação permanece parcial quanto ao conteúdo: variáveis,
eventos, áreas, cultivos e animais ainda só têm o suporte declarado na matriz.
A separação de execução não os torna completos nem implementa coop/rede.
Saves e retomada de processo continuam em PF-05, sem gravação simulada.

## Validação e reprodução

Novos testes:

- `native_update_clock`: bordas antes do tick, consumo único em lotes, held,
  perda de foco, clamp de catch-up e mesma contagem com lotes de 30/60/144 Hz.
- `native_runtime_update_playthrough`: três sessões com os mesmos inputs e
  zero/uma/duas apresentações por tick (aproximadamente 0/30/60 por segundo).
  Cobre convite de Duke sim/não, porta e transição real para a casa, sono,
  pagamento único e atualização diária do campo. Compara estado em cada tick;
  antes/depois de draw forçado compara os bytes da mesma sessão e as poses
  decodificadas para detectar qualquer mutação. Os bytes são somente uma
  asserção temporária em memória, nunca formato de save ou sessão copiada em uso.
  Também injeta falha de área e verifica erro, liberação do frame e ausência
  de transação/entrada na área de destino.
- `native_title_update_playthrough`: título e cadastro completo nos três ritmos,
  nomes/aniversário pelas entradas reais, RNG/ANI/scroll iguais e desenho sem
  mutação. Testa transferência para uma abertura nova usando as APIs públicas,
  sem inserir o resultado diretamente no estado de edição.

Todos os percursos existentes passaram para APIs públicas; nenhum teste inclui
`scene_preview.c` ou outro `.c` de produção. `tests/scene_fixture.h` reúne somente
chamadas dessas APIs e fixtures, sem implementar gameplay. `poultry_helpers.h`
e os drivers de inventário/economia/calendário/eventos/lojas/cultivos foram
ajustados. O teste de exploração também passou a usar o tick completo.

O comando de teste manual do convite não mudou:

```sh
./build/native-debug/port/hmbtn_native --data data --area-preview 0 --preview-date 2:14 --preview-time 06:00
```

Recursos continuam sendo instalados pelo BIN do usuário. Nenhum recurso, imagem
ou SDK foi incluído no Git. Diretórios `src/`, `sdk/`, `disc/`, `tools/`, CMake
PS1 e mudanças preexistentes fora do escopo permanecem preservados.

Resultado final em Linux x86-64 Debug, SDL3 3.4.18 local:

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado.
- `ctest --preset native-debug`: **89/89 aprovados**, zero falhas, **113,53 s**:
  57 independentes de dados opcionais e 32 percursos com recursos locais.
- Um aviso de constância em uma fixture de vila foi corrigido usando seu provider
  público. Build final sem avisos; esse percurso foi executado novamente:
  **1/1 aprovado**, 3,97 s. Nenhuma alteração de produção após a suíte completa.
- Capturas da loja geradas pelo percurso de cultivo, SDL dummy/software; tela
  de quantidade inspecionada, preservando a mesa, nomes e quantidade da PF-03.
  Capturas ficam somente em `/tmp`, sem recursos proprietários no repositório.

Não foram executados Windows/cross-build, Release, Android, ASan/UBSan,
instalação limpa ou inspeção manual em janela/áudio reais. O teste do acumulador
não liga SDL; os comparativos de runtime/título usam os backends reais com
SDL dummy/software e recursos instalados, sem exigir janela visível. Esses
resultados não demonstram fidelidade integral nem execução do jogo completo.

## Arquivos

- Núcleo: novos `port/game/runtime.c/.h` e `update_clock.c/.h`;
  `session.h` e `exploration.c/.h` integram propriedade/contexto de callbacks.
- Host: `port/core/app.c`, `graphics/scene_preview.c/.h`,
  `graphics/title_preview.c/.h` e novo `graphics/scene_inspection.h`.
- Build: `port/CMakeLists.txt`.
- Testes: os três novos citados, `scene_fixture.h` e migração dos percursos
  existentes em `port/tests/*playthrough.c`/`poultry_helpers.h`.
- Documentação: este relatório, roadmap, status, arquitetura, estratégia e matriz.
