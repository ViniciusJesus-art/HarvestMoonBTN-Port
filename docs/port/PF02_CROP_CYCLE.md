# PF-02 — Primeiro cultivo completo e passagem dos dias

Entrega de 2026-10-25, sobre a árvore local com MA-01..06/PF-01 já presentes.
Linux/Windows continuam sendo os alvos. O percurso do nabo está integrado ao
runtime: comprar sementes → arar → plantar → regar → dormir quatro dias regados
→ colher → lançar na caixa → receber 60 G. Não exige injetar sementes no jogo;
há compra por 120 G no grupo de primavera da mesa central do supermercado, com
Jeff presente e durante o expediente (9h–17h, fechado domingo/terça).

## Fontes reaproveitadas

Referência `d3c0f38`, sem alterações em `src/`. Não há compilação direta de módulos
originais no target nativo; as funções são adaptadas em `port/`.

| Origem | Implementação nativa e adaptação |
|---|---|
| `src/overlay/11_GF_FARM/field/field.c`: `field_event_dispatch`, `field_tile_dry`, `field_daily_tick` | `game/farm_field.c`: plantio em solo arado/molhado; dias regados por estágio, secagem, estação e maturidade. Estado explícito, sem alias de struct PS1, retexturização ou reshaping no algoritmo. Escopo validado: nabo e regras de rebrota do pepino. |
| Mesmo arquivo: `field_tile_item_stage` | Consulta de produto maduro e colheita em `farm_field.c`, consumidas por `farm_carry.c`. Cultura simples vira seu solo; cultura de rebrota retorna ao estágio original e conserva o bit de pular um dia. Produto ocupa a mão compartilhada antes da retirada do campo. |
| `people/player.c`: `player_start_command_item`, `PLAYER_MODE_PLANT_SEED`, `player_build_move_candidates`; `picture_cells.c`: `picture_grid_find_cell`/`grid13_copy_oriented` | `game/farm_action.c`: banco lógico 24 (0x53−0x3B), animação de plantio da tabela, nove alvos e consumo de um pacote somente ao concluir. `farm_field.c` resolve os alvos; o gerador extrai `picture_grids[25]` de `src/main/game/farm_tables.c`, incluindo suas orientações. |
| `world/clock.c`: `clock_advance_minute` | Conservada a adaptação equivalente já existente em `farm_clock.c`: cadência, pausa, virada às 06h, estação/ano e observer por minuto. Não reescrita para mudar somente a organização. `daily.c` fornece avanço da sessão para sono/eventos, usando o mesmo algoritmo. |
| `world/clock_events.c`: `scene_overnight_step` e ordem de crescimento/produção/limpeza | `game/daily.c`: callback único da sessão; crescimento do campo antes de produção e limpeza dos cochos. `hmbtn_session_morning` prepara recuperação, reset do jogador/ações e handoff; não carrega área nem toca áudio. |
| `menu/shop_dialogues.c`: `shop_seed_menu_setup_a`, `shop_result_request`, `shop_buy_stored_item`, `shop_item_room`/`shop_item_add`; `people/player.c`: `ZONE_SHOP_COUNTER` | Zona 37 e `farm_record_find_key` → `shop_key_value_lookup` selecionam o grupo pela posição na mesa. Adaptação no `feed_shop.*` e UI existentes, sem segundo estado de loja: lista base de primavera (grama/nabo/batata/pepino), seleção e quantidade de 1 a 99. Nomes/ordem/IDs/descrições são extraídos do original; falas seguem seus bancos e IDs. `inventory.c` adapta `shop_item_room`/`shop_item_add`: equipamento/mochila, pilhas até 99 e lote inteiro no baú quando o espaço acessível é insuficiente; cobrança usa a operação PF-01. |
| `field/field.c`: `field_cell_look_spring`/`field_cell_look_summer` | Cena consulta estado e desenha ANI original de estágios no pack sazonal existente (`estate_art`). Sem aplicar crescimento durante desenho. |

## Estado e processamento diário

`HmbtnFarmCrop` é uma definição imutável do catálogo gerado (estação, estágios,
duração, rebrota e produto). `HmbtnFarmTile.watered_days` pertence à instância,
ao lado do estágio e flags existentes. Não se lê o estado reinterpretando padding
ou layout serializado. O getter usa o tamanho da tabela gerada, sem introduzir
um novo teto de duas culturas: o filtro de suporte atual delimita os sistemas
validados nesta entrega; ampliar suporte não exige outro algoritmo de campo.

A sessão continua sendo dona do campo, inventário, economia, animais e relógio.
Não há novo calendário/campo espelhado. `daily_date` registra a última fronteira
diária processada; avisos repetidos da mesma data não repetem a atualização diária.

A sequência mantém os consumidores atuais de remessas/vila/horas. Na fronteira
às 06h: campo cresce usando a irrigação anterior → galinhas processam alimentação/
produção e idade → transporte é reconciliado → cochos são limpos → coleta silvestre
é renovada. Os contadores e flags da vila conservam seu callback atual. Avanço
por vários dias entrega cada fronteira, secando o solo a cada dia; não aplica
crescimento várias vezes com a mesma água. Não foram conectados callbacks fictícios
para clima, família, ajudantes ou espécies ausentes.

A preparação de manhã é explícita e idempotente até o host confirmar os recursos
com `hmbtn_session_morning_ready`. Carga de casa, música, cache de TV, fades e
morph da cama permanecem no adapter atual. Foi removido o antigo caminho separado
que secava o campo na cena após o avanço de tempo. A mudança de data agora aplica
o campo junto aos demais efeitos diários, antes da apresentação da manhã.
A separação global de update/render continua em PF-04.

Inicialização de jogo e trocas de área conservam os registros; o teste real cruza
casa/fazenda por várias noites. Saves continuam ausentes, sem formato novo ou
leitura/escrita simulada; PF-05 precisará serializar dias regados e estado diário
explicitamente, sem dump de structs.

## Comportamentos e limites

- Nabo: dois estágios de dois dias regados, sem rebrota. Noite seca não perde os
  dias já acumulados; não acrescenta um novo dia de crescimento. Colheita/pagamento
  só ocorrem uma vez e usam o inventário/economia PF-01.
- Pepino: regra original de estágio de rebrota e skip-day testada sem dados.
  A loja permite comprar os quatro itens base de primavera. Isso não amplia
  o suporte de plantio além do filtro de culturas validado; não declara lojas
  ou cultivos completos.
- Fora da estação, o primeiro estágio sem desenvolvimento volta a solo arado;
  plantas desenvolvidas deixam talos no verão seguinte, conforme o original.
  O inverno limpa as culturas cobertas e recusa plantio no campo aberto.
- O pacote usa a grade original de nove células; só células válidas/aradas aceitam
  sementes. Como no original, concluir o lançamento consome o pacote mesmo quando
  nenhuma célula aceita. Efeitos só ocorrem ao terminar ANI, nunca por desenhar.
- A compra na mesa segue lista → item → quantidade → compra/retorno, com as
  falas originais, informações por Triângulo e cancelamentos em cada etapa.
  Cancelar não cobra; saldo ou espaço insuficiente não alteram o inventário.
  O catálogo base de primavera está disponível; morango depende de progressão
  ainda não representada. Grupos de verão/outono/estufa continuam pendentes.
- Estufa, chuva/temporal, ervas aleatórias, grama/produção de feno, demais culturas,
  ferramentas de área e ampliações de catálogo continuam em PF-06. Não houve novo
  consumo de RNG para fingir esses sistemas. Recursos LE/endereços de disco e o
  fluxo BIN permanecem nos leitores existentes.

## Testes

`native_crop_daily`, sem SDL: vetores das tabelas, semear e irrigar, dias secos,
quatro dias regados, rebrota/skip bit, maturidade persistente, mudança de estação,
inverno/ano, pausa com frames preservados, fast-forward por vários dias,
idempotência de dia/manhã, alimentação e ovos antes da limpeza. Também cobre
nove alvos, consumo único de pacotes, alvo inválido e compra: horários/dias,
altura, mão ocupada, insuficiência, lotação, cancelamento, pilhas e spill para baú.

`native_crop_playthrough`, com os dados locais: compra efetiva por input e recursos
originais, banco/ANI de plantio, arado e recarga/uso do regador, quatro pernoites
pelas portas/cama existentes, sprites de broto/maturidade, colheita e lançamento
numa primitiva real da caixa. O teste também rejeita o balcão de entrega e os grupos de verão/outono,
identificando a posição/chave de cada registro original. Observa saldo/ledger/remessas e confirma
pagamento único durante o sono; nunca força `animation_done`. Posicionamento e
limpeza de um pequeno terreno são fixtures de teste, não comandos de runtime.

Configuração/build e suíte completa registrados no [roadmap](ROADMAP.md) e
[status](ROADMAP_STATUS.md). Capturas locais do teste foram inspecionadas; não
foram adicionadas ao Git. Windows/cross-build, Release, Android, sanitizers,
instalação limpa e execução manual com janela/áudio reais não foram executados.

## Arquivos desta entrega

- Novos: `port/game/daily.c`, `port/tests/crop_daily_test.c`,
  `port/tests/crop_playthrough.c` e este relatório.
- Domínio: `farm_field.c/.h`, `farm_action.c/.h`, `farm_carry.c`, `inventory.c/.h`,
  `feed_shop.c/.h`, `session.c/.h`, `exploration.c/.h`, todos em `port/game/`.
- Integração: `port/graphics/scene_preview.c`, `feed_shop_ui.c`,
  `port/cmake/generate_farm_tables.py` e `port/CMakeLists.txt`.
- Percursos anteriores ajustados para o callback público da sessão:
  `calendar_playthrough.c`, `economy_playthrough.c`, `farm_feed_playthrough.c`,
  `fishing_playthrough.c`, `pie_quest_playthrough.c`. `exploration_playthrough.c`
  agora compara também dias regados.
- Documentação: roadmap, status, matriz, arquitetura, estratégia e backlog
  complementar em `docs/port/`. Relatórios históricos anteriores foram preservados.

## Correção da seleção na mesa — 2026-10-25

A validação inicial da compra posicionava o jogador na zona 36 e, por isso,
aprovou uma interação incorreta. `world/scene.h` identifica 36 como entrega ao
morador e 37 como interação de loja. A correção usa a sequência original de
`player_act_zone_front`: probe de 224 unidades (com velocidade) para a primeira
zona ativa; depois `player_facing_point(mode=1)`, de 400 unidades, para procurar
o registro em `farm_record_find_key` com limites estritos de 150 em X/Z.
`zone.facing` e `zone.param` não escolhem o produto nesse comando.

As posições já extraídas de `src/main/game/farm_tables.c` são compartilhadas
com o desenho. Na loja (local 30), as chaves 0x128, 0x129 e 0x12A identificam
os três grupos na mesa. O gerador agora lê `shop_key_values` diretamente do
original; os diálogos correspondentes são 11/12/13. O recorte de nabo responde
somente ao grupo de primavera, sem substituir os outros grupos pelo mesmo item.

O desenho usa os sprites 15/16/17 do pack 8 e offsets originais de
`estate_search_records`; `estate_record_tables_refresh` limita a exibição à
estação do grupo enquanto a estufa não existe. Interagir com a mesa não vira
Jeff remotamente. Os testes existentes foram ampliados com rejeição da zona de
entrega, grupos diferentes, limite de busca, velocidade, ANI/posição reais e
captura da mesa antes da compra. O ciclo de cultivo continua no mesmo percurso.

Arquivos adicionais nesta correção: `port/game/exploration.c`, `feed_shop.c/.h`,
`port/graphics/scene_preview.c`, `port/cmake/generate_farm_tables.py`,
`port/CMakeLists.txt`, `port/tests/crop_daily_test.c`, `crop_playthrough.c` e os
registros de roadmap/matriz/status. Fontes originais e alterações anteriores
fora desse recorte permanecem preservadas. Esta correção não amplia o catálogo
de compras além do nabo nem conclui PF-03.

Validação desta correção: configuração/build `native-debug` aprovados; **83/83
CTests**, zero falhas, **51,22 s**. Os dois testes de cultivo ampliados também
passaram isoladamente. Captura da mesa com os sacos originais visíveis foi
inspecionada; recursos/imagens não foram adicionados ao Git.


## Correção do menu e das falas — 2026-10-26

A implementação anterior usava a descrição de nabo (2:0xC3) como abertura e
uma escolha simplificada de comprar/não comprar. Esse fluxo não correspondia
ao original e foi substituído, mantendo a interação na mesa já corrigida.

Fontes conferidas em `src/overlay/11_GF_FARM/menu/shop_dialogues.c`:
`shop_seed_menu_setup_a`, `shop_result_request`, `shop_menu_open_choose`,
`shop_menu_choose_next`, `shop_buy_stored_item`, `shop_quantity_input`,
`shop_menu_draw`, `shop_item_room` e `shop_item_add`. Adaptações em
`port/game/feed_shop.c`, `inventory.c` e `port/graphics/feed_shop_ui.c`.

- `seed_menu.inc` é gerado a partir dos quatro `ADD_ENTRY` incondicionais de
  `shop_seed_menu_setup_a` e de `text_bank.names.seeds`: grama 0x36, nabo 0x22,
  batata 0x23, pepino 0x24. A flag original 1 pede somente nomes na lista;
  não há preço por linha ou opção comprar/não comprar nesse menu.
- Banco 2: abertura 0xBE; ajuda da lista 0x46; item escolhido 0xC0; ajuda da
  quantidade 0x4D; compra 0xBB; sair da lista 0xBF; cancelar quantidade
  0xBA → 0xC2. Descrições 0xCF/0xC3/0xC4/0xC5 aparecem por Triângulo,
  voltando à mesma seleção/quantidade. Textos e retratos são os recursos do BIN.
- Setas esquerda/direita variam 1, cima/baixo variam 10, com retorno circular
  de 1 a 99, como `shop_quantity_input`. X confirma; Círculo cancela.
  Há repetição das setas enquanto seguradas. No teclado atual: Z confirma,
  X cancela e V pede informação. O custo é preço original × quantidade.
- Falta de dinheiro: 2:0x4F, volta à quantidade; falta de capacidade para o
  lote: 2:0xC1, volta à quantidade; nenhuma capacidade ao escolher o item:
  2:0x54, sai. Quando o lote precisa ir ao baú, a fala é **3:0x207**, seguida
  de 2:0xBB. O adaptador de mensagens agora respeita o banco solicitado.
- `hmbtn_inventory_add_seeds` centraliza quantidade e empilhamento; a API de
  um pacote delega a ela. Se equipamento/mochila não comportam o lote inteiro,
  ele vai ao primeiro registro vazio do baú, sem preencher parcialmente a
  mochila. Falhas não modificam estado. Dinheiro e inventário são validados
  antes da compra; espera/carregamento de mensagens não repete a transação.
- O desenho usa o mesmo catálogo do gameplay. O painel de quantidade fica
  abaixo de toda a lista na apresentação nativa, evitando que a posição
  derivada da linha selecionada sobreponha e recorte os outros nomes no
  renderizador de software. Não foi alterada a infraestrutura de renderização.

O estado continua em `HmbtnSession.feed_shop` e no inventário/economia
centralizados. Não foram criados estoques ou saldos paralelos. Inicialização,
trocas de área e sono preservam os pacotes comprados; não há saves em disco.
Morango, demais estações/estufa e expansão do suporte de plantio continuam
pendentes em PF-06; comprar batata/grama não significa que seu cultivo já
esteja implementado. PF-03 não foi iniciada.

`native_seed_shop` (novo, sem SDL/recursos) cobre catálogo e seleção, IDs/bancos
em cada transição, quantidade/repetição/cancelamento, quatro preços/itens,
falta de dinheiro/espaço, atomicidade, capacidade ativa, empilhamento e lote
inteiro no baú. `native_crop_playthrough` foi ampliado para abrir as falas
reais, escolher nabo, recusar 99 por saldo, cancelar e retornar, comprar três
por 360 G e plantar um, conservando os outros dois ao longo do ciclo completo.
Depois do ciclo, uma fixture de lotação separada verifica os textos reais de
falta de espaço e o aviso do banco 3. Os testes não pressupõem a mensagem
correta apenas porque existe um menu visível.

Arquivos desta correção: `port/CMakeLists.txt`,
`port/cmake/generate_farm_tables.py`, `port/game/feed_shop.c/.h`,
`port/game/inventory.c/.h`, `port/game/exploration.c`,
`port/graphics/feed_shop_ui.c`, `port/tests/seed_shop_test.c`,
`port/tests/crop_daily_test.c`, `port/tests/crop_playthrough.c` e os documentos
PF-02/roadmap/status/arquitetura/matriz. Alterações preexistentes preservadas;
`src/`, `sdk/`, `disc/`, `tools/` e CMake PS1 intactos.

Validação da correção de menu: `cmake --preset native-debug`,
`cmake --build --preset native-debug` e `ctest --preset native-debug`
aprovados; **84/84 testes**, zero falhas, **51,58 s**. São 55 testes
independentes dos dados opcionais e 29 percursos com `data/`. Os seis testes
direcionados passaram; após a suíte, a asserção adicional de persistência dos
dois pacotes ao final das noites/transições passou no percurso isolado.
Capturas reais de abertura, nomes, descrição, quantidade, compra e baú foram
inspecionadas com SDL dummy/software. Windows/cross-build, Release, Android,
sanitizers, instalação limpa e sessão manual com janela/áudio reais não
executados. Nenhuma captura ou recurso proprietário foi adicionado ao Git.
