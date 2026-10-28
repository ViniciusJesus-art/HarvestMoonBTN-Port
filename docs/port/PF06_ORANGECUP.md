# PF-06 — Orangecup

Entrega de 2026-10-28. Linux/Windows são os alvos; validação executada neste
host Linux. PF-06 continua aberta. A próxima tarefa está no [roadmap](ROADMAP.md).
O bootstrap `--diagnostics` não executa o jogo; estes percursos usam o runtime
jogável parcial e os recursos instalados a partir do BIN do usuário.

## Comportamento entregue

Won oferece Orangecup por **1.000 G por pacote**, em qualquer estação, mesmo
sem estufa. O catálogo D passa de seis para sete opções antes do desbloqueio de
Moondrop, e de sete para oito depois. Mantém a posição da fonte, nome, descrição
0x16C, escolha de quantidade, cancelamento, verificação de fundos/espaço,
armazenamento no baú e recibos próprios de Won. A conversa real abre o mesmo
modal e retoma o mesmo interpretador; não foi criada outra loja.

Na estufa, o plantio usa a animação 7 do banco 24, a grade original de nove
alvos e consome um pacote por ação. Cada um dos quatro estágios exige dois
dias regados: **oito noites regadas até a colheita**, sem rebrota. Noite seca não
avança o crescimento; chuva não irriga o interior; mudança de estação/ano não
mata a planta. Arte original no pacote 4, sprite 24, faixas 3..6.

O produto definido por `CROP_ORANGE_CUP.harvest` é **0x85**, com venda de **60 G**.
Ele passa pela mão, mochila, portas e remessa existentes. O outro item de nome
Orangecup, **0x56**, tem preço zero e não substitui o produto colhido. A remessa
não aumenta os contadores das culturas usados para liberar sementes sazonais.
O pagamento acontece uma vez, pelo processamento compartilhado da remessa.

Estado permanece em `session.greenhouse`/`field`, `game.inventory`,
`game.economy` e `construction`. Não há novo campo, estoque, algoritmo de
crescimento, RNG ou espelho de progresso. O formato de save continua **v10**:
IDs e estágios já cabem no codec existente. Não há migradores de versões antigas.
A perda da estufa limpa também a Orangecup; reconstruir não ressuscita plantas.

## Auditoria: a restrição externa presumida não está no C

Referência `6fa97ab`, arquivos originais sem alterações:

- `src/overlay/11_GF_FARM/menu/shop_dialogues.c:shop_seed_menu_setup_d` inclui
  Orangecup incondicionalmente. Somente Moondrop consulta o desbloqueio de história.
- `src/overlay/11_GF_FARM/people/player.c:player_start_command_item` e
  `player_build_move_candidates` usam a mesma ação/grade das demais sementes.
- `src/overlay/11_GF_FARM/field/field.c:field_event_dispatch` não contém uma
  recusa específica de Orangecup no exterior. A recusa de inverno é geral.
- `src/main/game/farm_tables.c:CROP_ORANGE_CUP` declara estação 0, estágios
  2/2/2/2, sem rebrota e produto 133. `field_daily_tick` usa essa estação no
  exterior e ignora a restrição sazonal no interior.
- `field_cell_look_spring` não contém Orangecup; `field_cell_look_summer`
  contém sprite 18/faixa `kind+2`; `field_cell_look_fall`, sprite 38/faixa 0.
  `greenhouse_draw` contém sprite 24/faixa `kind+2`.

Portanto, não foi implementada a proibição externa inicialmente presumida no
roadmap. O plantio externo segue aceito fora do inverno, cresce na primavera
segundo a tabela, e segue o tratamento sazonal comum nas demais estações.
A assimetria entre crescimento de primavera e ausência de sprite foi preservada,
com os casos externos de verão/outono adaptados em `estate_art.c`. Isso significa
que **não há promessa de cultivo externo visível na primavera**. O percurso
visível de cultivo validado é o da estufa. Confirmar essa particularidade com
execução do jogo original ainda está pendente; a evidência atual é a decompilação,
não uma captura de console/emulador. Não inventar uma regra para ocultar essa
incongruência nem confundi-la com limitação de hardware a transportar para PC.

## Arquivos desta entrega

| Arquivo | Alteração |
|---|---|
| `port/game/farm_field.c` | Habilita a definição já extraída de Orangecup no filtro comum de suporte. |
| `port/graphics/estate_art.c` | Adapta os casos externos existentes na fonte; o caso da estufa já estava na associação extraída. |
| `port/CMakeLists.txt` | Registra as regras e percurso de Orangecup, com carga em processo novo. |
| `port/tests/orangecup_test.c` | Vetores originais de crescimento, chuva/seca, limites, estações, RNG, inventário, preço e persistência. |
| `port/tests/orangecup_playthrough.c` | Compra/conversa reais, cultivo/arte/ano novo, colheita/transporte/venda, saves e perda/reconstrução. |
| `port/tests/won_seed_test.c` | Atualiza contagens/ordem; estende descrição/fundos/baú/lotação à Orangecup; disponibilidade nas quatro estações. |
| `port/tests/won_playthrough.c`, `moondrop_playthrough.c` | Atualizam a contagem esperada do catálogo sem mudar os percursos existentes. |
| `port/tests/greenhouse_test.c` | Inclui Orangecup no cultivo de todas as estações. |
| `port/tests/crop_daily_test.c`, `flower_test.c`, `magic_red_test.c` | Substituem expectativas de semente ainda não habilitada; preservam recusa de item não semente e demais regras. |
| `docs/port/ROADMAP.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `ROADMAP_STATUS.md`, este relatório | Estado real, fontes, evidências, limite externo e próxima tarefa. |

Nenhuma nova unidade original é compilada diretamente: os algoritmos originais
necessários já estavam adaptados; esta entrega habilita seu conteúdo e os casos
de apresentação faltantes. Preservados `src/`, `sdk/`, `disc/`, `tools/`, build
PS1, SDL3, áudio, instalação e todas as alterações locais anteriores.

## Verificações executadas

- `cmake --preset native-debug` e `cmake --build --preset native-debug` aprovados.
  A configuração mantém o aviso opcional de libdecor ausente.
- Referência antes das mudanças: **4/4** (estufa, Won, cultivo diário, saves).
- Seleção final: **18/18 CTests**, **10,60 s**, **34 processos** contando wrappers
  e filhos. Regras: Orangecup, estufa, saves, desbloqueios sazonais, Magic Red,
  flores, Won, cultivo diário, carry e remessa. Percursos: estufa, Orangecup,
  Moondrop e Won 0/1/3/4/6 (repolho, abacaxi, Pink Cat, Toy Flower e Magic Red
  vermelha), com retomada do app em processos novos.
- `native_orangecup` verifica cada estágio com roundtrip nas quatro estações,
  último quadrado da estufa, passagem de estação, noite seca/chuva, independência
  de campos/RNG, produto, mochila e regras externas da fonte.
- `native_orangecup_playthrough` compra o pacote com Won ainda sem estufa;
  verifica descrição/cancelamento/fundos; usa uma fixture explícita das obras
  anteriores e dinheiro. Executa ações reais de arar/semear/regar/colher,
  uma noite seca e oito regadas, atravessando inverno/Ano Novo. Verifica ANI,
  faixas/sprites originais, mão/mochila/portas, pagamento único e save/reload
  durante crescimento e depois da colheita. Sorteio controlado destrói uma
  Orangecup em crescimento, seguido de reconstrução e quatro noites reais.
- Desenhar/capturar os estágios estáveis não altera o save codificado. As
  capturas de menu, descrição, quantidade, quatro estágios e colheita foram
  conferidas. Mensagens vêm do BIN instalado; rótulos vêm da tabela C extraída,
  como nas outras sementes.
- **49 processos de teste no total** nesta entrega, incluindo referência,
  tentativas de desenvolvimento, repetições e filhos. Duas falhas iniciais dos
  novos testes foram corrigidas nos drivers: rebind de agendas após decodificar
  e captura de menu sem tentar salvar durante conversa ativa. A recusa de salvar
  em diálogo continua preservada. Limite de 50 respeitado.
- O percurso direto final gera uma fixture com fruto maduro; sua carga pelo
  aplicativo em outro processo também passou, além dos percursos via CTest.
- Não executados: suíte inteira, percursos Won 2/5 nesta entrega, Windows real,
  Release, sanitizers, Android, instalação limpa ou sessão manual de janela/áudio.
  A validação visual foi por capturas SDL dummy/software. Transporte/repatriação
  de animais na estufa, reprodução/máquinas/pastagem e PF-06 seguem pendentes.

Logs locais: `/tmp/hmbtn-orangecup-configure.log`,
`/tmp/hmbtn-orangecup-build.log`, `/tmp/hmbtn-orangecup-baseline-tests.log`,
`/tmp/hmbtn-orangecup-ctest.log`, `/tmp/hmbtn-orangecup-playthrough.log` e
`/tmp/hmbtn-orangecup-resume.log`. Recursos e capturas não entram no Git.

## Como experimentar

O percurso gera saves de desenvolvimento e pode ser executado com recursos
instalados, sem editar uma partida do usuário:

```sh
mkdir -p /tmp/hmbtn-orangecup-shots
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_orangecup_playthrough \
  data 0 /tmp/hmbtn-orangecup.hms /tmp/hmbtn-orangecup-shots
```

Com o argumento opcional de capturas, ele também grava um save antes de colher:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --load-save /tmp/hmbtn-orangecup.hms.ripe.hms
```

Esse save coloca o personagem na estufa, junto à Orangecup madura. É uma fixture
com obras/dinheiro/data configurados para teste, não uma progressão completa
obtida pelo jogador. O save principal `/tmp/hmbtn-orangecup.hms` termina após a
reconstrução, com o campo vazio. Arquivos em `/tmp` podem ser removidos pelo sistema.

Para regressão automática, `ctest --preset native-debug -R orangecup` executa
as regras e o wrapper com carga do app em outro processo, se os recursos de teste
estiverem configurados. A fila de desenvolvimento continua no roadmap:
**compra de ferramentas animais, cuidados e produção de leite/lã**.
