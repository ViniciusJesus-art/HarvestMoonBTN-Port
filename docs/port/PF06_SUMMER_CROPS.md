# PF-06 — Culturas base de verão

Entrega de 2026-10-26: tomate, milho e cebola, com compra na mesa original,
crescimento, colheita/remessa, rebrota quando prevista e saves. Linux e Windows
continuam como alvos; validação desta entrega em Linux Debug. PF-06 permanece
aberta para os demais sistemas. PNG segue adiado.

## Origem das regras e adaptações

Referência: `d3c0f38`. Nenhum arquivo original foi alterado.

| Fonte original | Integração em `port/` |
|---|---|
| `src/main/game/farm_tables.c`: `estate_records[112..114]`; `src/overlay/11_GF_FARM/estate.c`: habilitação dos conjuntos sazonais, `estate_code_status`; `menu/shop_dialogues.c`: `shop_key_values` | Conjunto de verão na posição do registro 113, chave 0x129 → diálogo 12. `game/feed_shop.c` aceita somente o conjunto correspondente à estação suportada. Zona 36 continua sendo entrega; compras usam a zona 37 e a busca longa pelo registro. |
| `menu/shop_dialogues.c`: `shop_seed_menu_setup_a/b` | `cmake/generate_farm_tables.py` extrai as duas listas base em `seed_menu.inc`, mantendo ordem, nomes, IDs e descrições. A API `hmbtn_seed_shop_entries(season)`/`entry(season,index)` é compartilhada por gameplay e UI. Sem cópia de catálogo na apresentação. |
| `menu/shop_dialogues.c`: `shop_result_request_ready`, `shop_buy_stored_item`, escolha/quantidade | Saudação de verão 2:0x273, explicações 2:0xC7/C8/C9; escolha/quantidade/transações mantêm a máquina atual e as falas originais. Não foi criado outro menu ou saldo. |
| `farm_tables.c`: `CROP_TOMATO`, `CROP_CORN`, `CROP_ONION`, itens/produtos | O filtro de suporte de `game/farm_field.c` aceita 0x27..0x29. Metadados já eram gerados; não foi criada outra tabela de crescimento/preços. |
| `field/field.c`: `field_daily_tick`, `field_tile_item_stage`, `field_event_dispatch`; `people/player.c`/`picture_cells.c`: plantio | Mesmo algoritmo diário, ação por ANI/grade orientada, consumo único, colheita e flag skip-day. Campo, inventário, relógio, economia e remessas continuam autoritativos na sessão. |
| `field/field.c`: `field_cell_look_spring/summer/fall/winter`, `field_cell_look_anim` | `graphics/estate_art.c`: `hmbtn_estate_crop_look` resolve sprites/faixas sem mutar gameplay. A cena desenha a planta usando o estágio atual. |
| `estate.c`: remessas/contagem e calendário de coleta | `game/farm_shipping.c` e economia existentes conservados; o percurso da cebola verifica adiamento do pagamento em data sem coleta. |
| Codec nativo entregue em PF-05 | IDs, estágios e contadores já são representados no save v1. Nenhuma alteração de versão/layout. Carga recompõe recursos host e conserva a sessão. |

### Metadados e comportamentos conferidos

| Cultura | Semente / produto | Compra / venda | Dias por estágio | Maturidade / rebrota |
|---|---|---|---|---|
| Tomate | 0x27 / 0x46 | 200 G / 60 G | 2, 2, 2, 3 | estágio 4; colheita volta a 3 |
| Milho | 0x28 / 0x47 | 300 G / 100 G | 3, 4, 4, 3 | estágio 4; colheita volta a 3 |
| Cebola | 0x29 / 0x48 | 150 G / 80 G | 3, 4, 0, 0 | estágio 3; removida ao colher |

O processamento original testa uma transição por atualização diária. Por isso,
o estágio de zero dias da cebola ainda exige uma atualização: o percurso tem
**oito noites regadas**, contra nove do tomate e quatorze do milho. Os testes
fixam os vetores de estágios/dias, em vez de inferir durações de um guia externo.
Uma noite seca conserva o progresso. Tomate/milho retornam ao estágio 3 com
skip-day; uma noite seca não o limpa e quatro noites regadas realizam o descarte
do primeiro incremento e os três dias da rebrota. Colher outra vez exige maturidade.

As três culturas murcham no outono e são limpas no inverno pelo processamento
existente. A aparência viva usa sprites 12/13/14 na estação de verão e faixa
`stage+2`; a aparência murcha usa sprites 7/8/9 e faixa zero no pack de outono.
Grama continua como terreno e conserva seu estoque/regras.

### Desbloqueios e conteúdo ainda fora do recorte

O original só adiciona abóbora se `ESTATE_STATUS_GROUP_B_DONE` estiver ativo.
Em `event/milestones.c`, `milestone_check_group_b` usa remessas de tomate, milho,
cebola e abacaxi, comparadas a `GROUP_SHIPMENT_THRESHOLD` (101 no código desta
revisão). Não foi criado um booleano paralelo de desbloqueio, nem liberada abóbora
sem essa cadeia. Morango, abóbora, abacaxi/Won, demais desbloqueios e estufa seguem
pendentes. O menu de outono ainda não aceita compras nesta entrega.

## Carga de recursos independente da sessão anterior

O percurso revelou uma falha anterior: um save de verão aberto numa cena nova
podia falhar no primeiro desenho com `Model polygon reads a missing texture or
palette (page 8, clut 3ea0, uv 128,242)`. O banco da fazenda de verão deixa parte
dessa região sem upload, enquanto uma sessão que começou na primavera retinha
esses pixels. Assim, apenas testar transições na mesma cena não encontrava a falha.

`graphics/scene_preview.c:load_area` agora compõe o banco base da própria área
antes do banco sazonal quando o recurso difere da área lógica. Os dados do
recurso atual sobrescrevem suas regiões normalmente; o preenchimento não depende
da memória de uma visita anterior. Isso conserva o comportamento visual atual e
permite reconstrução da cena num novo processo. Não grava atlas em saves, não
relaxa validação de recursos e não altera arquivos proprietários ou estado de jogo.

## Arquivos modificados

Produção:

- `port/cmake/generate_farm_tables.py`: extração dos menus base de primavera/verão.
- `port/game/feed_shop.c` e `.h`: catálogo por estação, chave/saudação de verão;
  mesma máquina de compra e relógio emprestado da sessão.
- `port/game/farm_field.c`: habilitar as três sementes no motor existente.
- `port/graphics/feed_shop_ui.c`: leitura do catálogo sazonal compartilhado.
- `port/graphics/estate_art.c` e `.h`: seleção visual rastreável por cultura/estação.
- `port/graphics/scene_preview.c`: uso do seletor e composição de bancos de área
  durante carga, inclusive retomada de save.

Validação:

- `port/tests/summer_crop_test.c`: vetores das três culturas, noites secas,
  idempotência diária, rebrota, estação, ferramentas/plantio nos quatro sentidos
  e chave correta da mesa, sem SDL/recursos instalados.
- `port/tests/seed_shop_test.c`: nomes, descrições, navegação e transações de verão,
  com regressões de primavera/quantidade/estoque/cancelamento conservadas.
- `port/tests/crop_playthrough.c`: o mesmo driver de nabo/batata parametrizado
  para tomate/milho/cebola. Compra três pacotes, verifica recusa nas demais
  posições/balcão, falas/quantidade/espaço/saldo, planta, rega, dorme, salva/carrega,
  colhe/remete/recebe e percorre rebrota de tomate/milho. Confere sprites/estação.
- `port/tests/field_helpers.h` e `grass_playthrough.c`: uma operação de save/load
  comparado, extraída da grama e compartilhada com os percursos de cultivo.
- `port/tests/summer_process_playthrough.py`: retoma o arquivo gerado por cada
  cultura em outro processo. Arquivos temporários próprios, sem save global.
- `port/CMakeLists.txt`: quatro novos CTests (regras e três percursos opcionais).

Documentação: este relatório, `ROADMAP.md`, `ARCHITECTURE.md`,
`PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `ROADMAP_STATUS.md` e
`ROADMAP_PENDENTE_CODEX.md`.

## Reprodução e validação

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
ctest --test-dir build/native-debug --output-on-failure \
  -R 'native_(summer|tomato|corn|onion|seed_shop|crop_daily|grass|crop_playthrough|potato)'
```

O percurso usa terreno, pacotes, scripts de texto, ANI, portas/cama e caixa de
remessas instalados a partir do BIN do usuário. O teste fornece 1000 G à fixture
para comprar três pacotes de milho e considera concluído o pedido de Thomas para
isolar o ciclo longo; esses sistemas mantêm seus testes próprios. O save carrega
sem a cena antiga e é comparado byte a byte na codificação canônica. Tomate/milho
verificam save durante o crescimento e após a colheita; o último arquivo gerado
retoma a rebrota. Cebola verifica que a coleta da remessa de Verão 12 fica pendente
e só paga no próximo dia de coleta, uma vez.

Configuração e compilação aprovadas. **99/99 CTests**, zero falhas, **128,11 s**:
61 testes sem dependência dos recursos opcionais e 38 com
`HMBTN_TEST_GAME_DATA` configurado para os dados locais. Os dez direcionados
passaram (**10/10**, 5,90 s). A suíte completa incluiu essas versões finais.
Capturas do menu, maturidade de milho/cebola e murchamento de tomate no outono
foram inspecionadas com SDL dummy/software. O comando de preview abaixo também
foi executado com três frames. Recompilar os headers reexibiu os quatro avisos
const preexistentes da fixture de mapa; build incremental final sem novos avisos.
Produção mantém `-Werror`.

Para testar manualmente a loja no verão:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --area-preview 180 --preview-date 1:5 --preview-time 10:00
```

No CLI, as estações são 0=primavera, 1=verão, 2=outono, 3=inverno; os dias são
1..30. Interaja com o conjunto de saquinhos de verão na mesa central. A lista mostra
nomes; escolher a semente abre quantidade/total. O balcão mantém sua função.
Para gerar um save de cultivo que também pode ser aberto na aplicação:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_crop_playthrough data --tomato /tmp/hmbtn-tomato.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/hmbtn-tomato.hms
```

As opções `--corn` e `--onion` repetem os outros percursos. Capturas opcionais
vão no último argumento, apontando para um diretório já existente. Nenhum recurso
extraído ou captura foi adicionado ao Git.

Windows/cross-build, Release, sanitizers, Android, sessão manual de janela/áudio
reais e instalação limpa não foram executados nesta entrega. A validação com
SDL dummy/software não demonstra paridade visual/sonora integral. Sem PNG, novos
animais, clima, estufa, ferramentas melhoradas ou rede.
