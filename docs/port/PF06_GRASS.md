# PF-06 — Grama e feno

Entrega de 2026-10-26. Linux/Windows são os alvos; esta execução validou Linux
Debug. PF-06 continua aberta. Suporte a PNG permanece adiado.

## Fontes e adaptação

Referência de origem: `d3c0f38`. Nenhum arquivo de `src/` foi modificado.

| Código original | Integração nativa e comportamento |
|---|---|
| `src/main/game/farm_tables.c`: `CROP_GRASS`, `ITEM_GRASS_SEEDS`, regras 12/13/14 | Catálogo gerado existente: semente 0x36, preço 500 G, estação ANY, estágios 0..3, dias 3/4/4, rebrota no estágio 1, sem produto para a mão. Não foi criada outra tabela. |
| `src/overlay/11_GF_FARM/field/field.c`: `field_event_dispatch`; `people/player.c` e `picture_cells.c`: plantio/grade | `port/game/farm_field.c` habilita 0x36 no plantio atual. `farm_action.c` reutiliza banco 24, ANI e grade orientada de nove células e consome um pacote na conclusão, como para outros cultivos. |
| `field/field.c`: `field_daily_tick` | Mesmo processamento diário em `farm_field.c`, chamado por `daily.c`: grama cresce sem rega; ANY atravessa verão/outono; inverno preserva a planta e reinicia grama estabelecida no estágio 1. |
| `field/field.c`: `field_tile_harvest`, `field_grown_cell_request`; `estate.c`: `estate_fodder_add` | Foice usa as regras de ferramenta existentes. Apenas a grama madura aceita o corte; acrescenta uma unidade a `game.inventory.supplies[0]`, volta ao estágio 1, zera dias e ativa skip-day. Estoque cheio ainda permite cortar/rebrotar. |
| `field/field.c`: `field_gate_open`, regras do catálogo | Grama estabelecida recusa rega. Estágios 0/1 aceitam enxada; estágio 2 recusa enxada/foice; estágio 3 aceita foice. Todos são caminháveis. |
| `field/field.c`: `field_reshape_grown`, `field_cell_retexture`, `picture_cells_tick` | Oito formas por estágio, derivadas da mesma máscara de nove vizinhos; UV/CLUT existentes, página 29, paleta de outono e terreno de inverno. Grama não vira sprite de pepino. |
| `field/field_item_requests.c`: `item_request_fill` | Efeito de corte usa ANI 14 na primavera e 19 no verão/outono, banco sazonal `estate_art`, faixa 1 a cada três ticks; outros efeitos mantêm seus recursos. |
| `estate.c` e `people/player.c`: retirada/devolução/colocação de ração | `port/game/farm_feed.c` continua usando estoque 0, item 0x68 e os cochos existentes, sem cópia ou sincronização adicional. |

O limite atual de 999 unidades é a regra inicial de estoque já usada pelo
celeiro, não um requisito de hardware PS1. Não há novo contador, motor diário,
interpretador ou sistema de alimentação. A capacidade não foi generalizada para
conteúdo futuro nesta entrega.

## Estado, apresentação e persistência

As instâncias continuam em `HmbtnSession.field.cells[]`. `watered_days` representa
`FarmGrowth.days`; para grama os dias avançam sem rega, apesar do nome herdado do
primeiro cultivo nativo. O inventário autoritativo continua sendo
`HmbtnGameState.inventory`; o corte e o celeiro acessam `supplies[0]`.

O original conserva a forma visual no nibble baixo de `growthFlags` e solicita
alterações de GPU. O adaptador nativo calcula as formas no retexture, a partir
do campo, sem modificar o estado de gameplay. Foram adaptados os predicados de
máscara do caminho `merge=0`; o modo de remodelamento `merge=1` não tem consumidor
neste recorte. O bit skip-day continua sendo estado da simulação e é persistido.

O codec v1 já grava ID, solo, estágio, flags e dias de cada célula, além dos
suprimentos. Nenhuma versão, layout de arquivo ou caminho de inicialização/carga
foi alterado. O percurso salva durante o crescimento e após o corte, destrói a
cena anterior e compara os bytes canônicos da sessão recarregada antes de retomar.
Outro processo abre o arquivo final pelo `--load-save`.

Há dois detalhes do algoritmo original protegidos por testes: após cortar,
a primeira atualização diária limpa skip-day sem somar dias, seguida de 4 + 4
dias para amadurecer novamente (nove viradas no percurso nativo); no inverno,
a grama estabelecida reinicia em estágio 1/dia 0 e passa pelo incremento diário,
terminando em estágio 1/dia 1 a cada manhã. Na primavera o progresso retoma.
Não foram substituídos por durações inferidas de guias externos.

## Arquivos desta entrega

Produção:

- `port/game/farm_field.c`: suporte, crescimento, ferramenta, estoque e terreno.
- `port/game/field_effects.c` e `.h`: efeito de corte com ANI sazonal emprestada.
- `port/game/exploration.c` e `.h`: tick do efeito no fluxo existente.
- `port/graphics/scene_preview.c`: terreno/efeito corretos, sem sprite de cultura comum.

Validação:

- `port/tests/grass_daily_test.c`: metadados, ausência de rega, maturidade,
  corte único, recusa de repetição, rebrota/skip-day, save comparado dia a dia,
  capacidade, estações/inverno, regras de ferramentas/colisão, vizinhança/borda,
  UV/página/paleta e efeito com duração limitada.
- `port/tests/grass_playthrough.c`: compra de um pacote na mesa original,
  plantio de nove células por ANI, onze noites sem rega, save/load intermediário,
  corte, estoque compartilhado, retirada/devolução/cocho no celeiro, rebrota e
  segundo corte; recursos sazonais de verão/outono/inverno.
- `port/tests/grass_process_playthrough.py`: arquivos temporários isolados e
  retomada em outro processo. Recursos opcionais vêm do BIN instalado pelo usuário.
- `port/tests/field_helpers.h`: helpers de ferramenta/posição extraídos do
  percurso existente, usados por nabo/batata/grama; não implementam regras de jogo.
- `port/tests/crop_playthrough.c` e `farm_menu_test.c`: uso dos helpers e da
  assinatura atual dos efeitos, conservando as verificações anteriores.
- `port/CMakeLists.txt`: dois CTests; percurso com recursos opcional.

Planejamento: este relatório, `ROADMAP.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`,
`PORTING_STRATEGY.md`, `ROADMAP_STATUS.md` e `ROADMAP_PENDENTE_CODEX.md`.

## Validação reproduzível

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
ctest --test-dir build/native-debug --output-on-failure -R native_grass
```

Referência: **95/95 CTests**, zero falhas, **124,59 s**: 60 testes sem depender
dos recursos opcionais e 35 com dados instalados configurados em
`HMBTN_TEST_GAME_DATA`. Após ampliar as fixtures sazonais/visuais, recompilação e
nove direcionados passaram (**9/9**, 2,66 s): grama, campo, menu/efeitos,
nabo/batata, crescimento e alimentação. Capturas de crescimento recarregado,
maturidade, corte de outono e terreno de inverno foram inspecionadas com SDL
dummy/software. O cenário considera já concluído o pedido de ingredientes de
Thomas para isolar as vinte noites de cultivo; esse evento conserva seus testes
próprios. Build incremental final sem novos avisos. A recompilação anterior
reexibiu os quatro avisos const preexistentes da fixture de mapa.

Para repetir o percurso e gerar um save local testável:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_grass_playthrough data /tmp/hmbtn-grass.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/hmbtn-grass.hms
```

O save resultante está na primavera, após rebrota e segundo corte, com uma
unidade de feno no estoque e a planta reiniciada. As fixtures sazonais seguintes
não sobrescrevem esse arquivo. Para jogar desde uma sessão de desenvolvimento,
continuam disponíveis `--area-preview`, `--preview-date` e `--preview-time`.

Não executados nesta entrega: Windows/cross-build, Release, ASan/UBSan,
Android, sessão manual de janela/áudio e instalação limpa. Sem PNG, novas espécies,
clima/ervas aleatórias, estufa ou ferramentas melhoradas. A fixture de inverno
confere terreno e estado da grama; não declara paridade de toda a vegetação sazonal.
