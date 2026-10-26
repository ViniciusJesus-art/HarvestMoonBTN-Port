# PF-06 — Batata

Recorte entregue em 2026-10-26, sobre MA-01..06 e PF-01..05 existentes.
A PF-06 continua aberta: esta entrega completa a batata, sem declarar os demais
cultivos, clima, espécies, áreas ou progressão completos. A ordem seguinte está
somente no [ROADMAP.md](ROADMAP.md).

## O que mudou

A batata já existia no catálogo gerado e no menu de sementes. O motor nativo
adaptado do original já conseguia processar suas durações, produto e ausência
de rebrota. As lacunas verificadas eram o filtro de culturas habilitadas e a
seleção de arte, que antes só distinguia nabo e pepino.

Em produção, as mudanças ficam em dois arquivos:

- `port/game/farm_field.c`: habilita o ID 0x23 na mesma consulta de suporte usada
  por plantio, grade, atualização diária e colheita. Os algoritmos existentes
  permanecem os mesmos.
- `port/graphics/scene_preview.c`: seleciona o sprite 8 para batata, conforme as
  funções originais de apresentação; nabo continua em 7 e pepino em 9. Na
  primavera a animação é estágio + 2; no verão, o talo fora de estação usa 0.
  O desenho não aplica crescimento, coleta ou mudança de inventário.

Não foi criado outro motor de cultivos, estoque, relógio ou estado de progresso.
O suporte é ampliado usando `HmbtnSession.field` e os mesmos inventário/economia,
serviços, transições, sono e save nativo. O conteúdo 0x23/0x42 já era representável
no formato v1; nenhuma alteração de versão ou layout de save foi necessária.

## Fontes originais conferidas

Referência `d3c0f38`, mantendo `src/` intacto. Os módulos C originais continuam
fora dos targets nativos; são aproveitados através das adaptações em `port/` e
dos catálogos gerados existentes.

| Origem | Regra/recurso reaproveitado |
|---|---|
| `src/main/game/farm_tables.c`, `CROP_POTATO` | Primavera, estágio maduro 2, sem rebrota; três dias regados no estágio 0 e quatro no estágio 1; produto 0x42. A definição já era extraída pelo gerador atual. |
| Mesmo arquivo, `ITEM_POTATO_SEEDS`/`ITEM_POTATO`; `src/include/items.h` | Semente 0x23 por 150 G, produto 0x42 por 80 G; grade e animação de plantio do catálogo. |
| `src/overlay/11_GF_FARM/field/field.c`: `field_event_dispatch`, `field_daily_tick`, `field_tile_dry`, `field_tile_item_stage`/`field_tile_harvest` | Mesmo plantio em solo preparado, dias regados, secagem, maturidade, retorno ao solo na colheita e tratamento fora de estação já adaptados em `farm_field.c`. |
| Mesmo arquivo, `field_cell_look_spring`, `field_cell_look_summer`, `field_cell_look_anim` | Sprite 8; animação estágio + 2 na primavera e 0 para a planta fora de estação no verão. Arte vem do pack sazonal instalado, lido por `estate_art.c`. |
| `people/player.c`: `player_start_command_item`, `PLAYER_MODE_PLANT_SEED`; `picture_cells.c`: orientação da grade | `farm_action.c` já usa o banco 24, ANI de plantio, nove alvos e consumo de um pacote na conclusão. Apenas o filtro precisava aceitar a batata. |
| `menu/shop_dialogues.c`: menu/quantidade e compra de sementes; `estate.c`: remessa/dinheiro | Lista base na mesa central, terceira entrada (depois de grama e nabo), descrição 2:0xC4 e quantidade. Transação existente em `feed_shop.c`/`inventory.c`; produto e preço consultados pelo mesmo sistema de remessa. |
| `world/clock_events.c` e adaptação `port/game/daily.c` | Passagem diária única, crescimento antes da produção/limpeza, mesmos efeitos de sono e avanço de calendário. Sem novo callback específico por planta. |

O C original serve de algoritmo e referência de conteúdo. Não há dependência
nova de MIPS, SDK, memory card, limites de memória PS1 ou compatibilidade binária.

## Comportamento validado

- Compra pela mesa central e pelas falas/nomes/quantidade já implementados.
  Três pacotes custam 450 G. Cancelamento, dinheiro insuficiente, espaço e baú
  usam as mesmas regras da loja, sem duplicar a transação.
- Um pacote é consumido ao concluir o lançamento, com a grade original de nove
  células; o pacote não é consumido de novo nos ticks seguintes. Células fora
  do campo, inativas ou sem solo preparado não aceitam plantio.
- Sete dias regados (3 + 4) produzem uma batata. Noite seca conserva o progresso
  acumulado e não acrescenta um dia regado. A planta madura permanece até colher.
- Colher produz 0x42 na mão compartilhada e remove a planta, sem rebrota ou
  segunda colheita. Lançar na caixa registra a remessa de batata e paga 80 G uma
  vez no horário existente; ledger/saldo e pacotes restantes são preservados.
- Fora da estação, aplica-se o mesmo tratamento de plantas desenvolvidas/talos
  e sementes sem desenvolvimento; inverno recusa plantio no campo aberto.
- Save no estágio 1 com um dia regado acumulado, após quatro dias regados no
  total: a sessão/recursos anteriores são destruídos e uma nova cena é criada
  do arquivo. O estado persistido é comparado integralmente e o cultivo
  continua até a colheita. Outro processo também inicia com esse save.

Grama ainda pode ser comprada pelo menu existente, mas seu plantio/produção de
feno continua pendente. Pepino mantém as regras de rebrota já testadas; esta
entrega não declara seu percurso completo de compra/colheita validado.
Clima, estufa, ferramentas de área e demais culturas continuam nos próximos
recortes da PF-06.

## Testes e resultado

`native_crop_daily` foi ampliado com os vetores da batata: catálogo/preços, estágio
3 + 4 dias, noites secas, maturidade persistente, ausência de rebrota, estação,
inverno, limites/inatividade de células e atualização diária idempotente.
O teste da grade e consumo único de pacotes agora percorre nabo e batata.
Os vetores anteriores de nabo/pepino continuam presentes.

`port/tests/crop_playthrough.c` usa o mesmo driver para nabo e batata. Não há
uma cópia independente do percurso. O cenário de batata seleciona a terceira
entrada real da mesa, pede descrição/quantidade, compra três pacotes e usa as
animações/terreno reais para arar, plantar, recarregar o regador e regar. Insere
uma noite seca, dorme pelas portas/cama existentes e salva/carrega no crescimento
antes de concluir colheita, lançamento e pagamento único. O driver também
verifica os textos de falta de saldo, cancelamento, capacidade e baú.

`native_potato_playthrough` é registrado quando `HMBTN_TEST_GAME_DATA` está
configurado. Seu runner `potato_process_playthrough.py` usa pasta temporária,
executa o cenário e inicia o executável nativo em outro processo com
`--load-save` no arquivo de crescimento. Nenhum teste escreve no slot do usuário.

Validação Linux x86-64 Debug, SDL3 local:

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado, sem novos avisos nesta
  compilação incremental.
- Quatro testes direcionados (loja de sementes, regras diárias, nabo e batata):
  **4/4 aprovados**, zero falhas, **1,43 s**.
- `ctest --preset native-debug`: **93/93 aprovados**, zero falhas, **124,82 s**.
  São 59 independentes de dados opcionais e 34 percursos com recursos locais.
- Capturas da quantidade, descrição, broto, maturidade e venda geradas pelo
  percurso em `/tmp`; quantidade de três pacotes e planta madura inspecionadas.
  O renderizador usa SDL dummy/software nesses testes.

Windows/cross-build, Release, Android, sanitizadores, instalação limpa e inspeção
manual com janela/áudio reais não foram executados. Os CTests não demonstram jogo
completo ou fidelidade integral. `--diagnostics` continua sendo bootstrap e não
executa gameplay. Nenhum recurso proprietário, imagem ou save foi adicionado ao Git.

## Reprodução manual

Para começar na loja aberta, com sessão nova e dinheiro inicial:

```sh
./build/native-debug/port/hmbtn_native --data data --area-preview 180 --preview-date 0:4 --preview-time 12:00
```

Interagir com o conjunto de primavera na mesa central, escolher “Potato seeds”
e a quantidade. Voltar à fazenda, equipar o pacote, arar o solo, plantar e regar;
o relógio e a cama processam os dias. C usa a ferramenta, Z interage/colhe,
X cancela/corre, V abre a mochila/pede informações conforme a interface atual.
O save usa a opção existente de dormir e registrar o diário.

Para gerar as capturas usadas nesta entrega, mantendo os dados fora do Git:

```sh
mkdir -p /tmp/hmbtn-potato-captures
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SDL_RENDER_DRIVER=software ./build/native-debug/port/hmbtn_crop_playthrough data --potato /tmp/hmbtn-potato-growing.hms /tmp/hmbtn-potato-captures
```

## Arquivos alterados

- `port/game/farm_field.c`, `port/graphics/scene_preview.c`.
- `port/tests/crop_daily_test.c`, `port/tests/crop_playthrough.c`.
- Novo `port/tests/potato_process_playthrough.py`; `port/CMakeLists.txt` registra
  o novo cenário com dados instalados.
- Este relatório, `ROADMAP.md`, `ROADMAP_STATUS.md`, `ARCHITECTURE.md`,
  `PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, em `docs/port/`.

Alterações anteriores fora desse recorte foram preservadas. Nenhum arquivo em
`src/`, `sdk/`, `disc/`, `tools/` ou CMake PS1 foi modificado. A instalação continua
usando o BIN fornecido pelo usuário. Próximo recorte selecionado no roadmap:
grama e produção de feno, usando campo e suprimentos existentes.
