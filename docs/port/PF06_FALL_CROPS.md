# PF-06 — Culturas base de outono

Entrega de 2026-10-26: berinjela, cenoura e batata-doce, com compra sazonal,
plantio, crescimento, colheita/remessa, rebrota prevista, passagem ao inverno e
persistência. Linux e Windows são os alvos; esta execução validou Linux Debug.
PF-06 continua aberta. PNG permanece adiado.

## Fontes e integração

Referência de origem: `d3c0f38`. `src/`, SDK, disco, ferramentas e CMake PS1
preservados. Nenhum módulo original passou a ser compilado diretamente: esta
entrega reutiliza as adaptações C e os dados extraídos já existentes em `port/`.

| Funções/dados originais | Integração nativa |
|---|---|
| `src/overlay/11_GF_FARM/menu/shop_dialogues.c`: `shop_seed_menu_setup_c` | O gerador existente extrai grama/berinjela/cenoura/batata-doce, com nomes, ordem, IDs e descrições 0xCF/CB/CC/CD. Gameplay e UI consultam o mesmo catálogo por estação. |
| `farm_tables.c`: registro 114; `estate.c`: habilitação sazonal dos conjuntos; `shop_key_values`/`shop_result_request_alt` | A busca na mesa exige chave 0x12A → diálogo 13, estação 2 e comerciante presente; saudação 2:0x274. Balcão e conjuntos de outras estações não abrem essa compra. |
| `src/main/game/farm_tables.c`: `CROP_EGGPLANT`, `CROP_CARROT`, `CROP_SWEET_POTATO`, itens/produtos | `game/farm_field.c` habilita 0x2C..0x2E no motor atual. Tabelas de dias, produtos, preços e ações já eram extraídas, sem catálogo paralelo. |
| `field/field.c`: `field_event_dispatch`, `field_daily_tick`, `field_tile_item_stage`; `people/player.c`/`picture_cells.c`: plantio e grade | Mesmas operações de plantio, dias regados, colheita, rebrota e skip-day, chamadas pelo mesmo observer diário. Sem nova simulação ou contador de inventário. |
| `field/field.c`: `field_cell_look_fall`, `field_cell_look_winter` | `graphics/estate_art.c:hmbtn_estate_crop_look` seleciona sprites 12/13/14, faixa `stage+2`, no outono. No inverno não há sprite de cultura; o ciclo diário limpa esses cultivos. |
| `shop_seed_menu_setup_c`: colunas mais largas; labels originais | `graphics/feed_shop_ui.c` ajusta a largura da lista ao maior nome na fonte nativa, preservando nomes apenas e o painel de quantidade posterior. “Sweet potato seeds” cabe na moldura. |
| `estate.c`: remessas/valores; `world/clock.c` e eventos diários | Inventário, economia, caixa de remessas e pagamentos existentes; o cenário da cenoura protege o adiamento de coleta em Outono 9. |
| `event/milestones.c`: `milestone_check_group_c` | Condição do espinafre identificada, sem liberar artificialmente a semente. A cadeia inclui pimentão e permanece pendente. |

### Valores conferidos

| Cultura | Semente / produto | Compra / venda | Dias por estágio | Madura / após colheita |
|---|---|---|---|---|
| Berinjela | 0x2C / 0x4B | 120 G / 80 G | 3, 3, 3 | estágio 3; volta ao estágio 2 |
| Cenoura | 0x2D / 0x4C | 300 G / 120 G | 3, 4 | estágio 2; removida |
| Batata-doce | 0x2E / 0x4D | 300 G / 120 G | 3, 2 | estágio 2; volta ao estágio 1 |

Primeira colheita: nove, sete e cinco noites regadas, respectivamente. Noites
secas conservam progresso. Berinjela e batata-doce mantêm a planta, zeram dias
e ativam skip-day ao colher. No algoritmo atual adaptado do original, uma noite
seca não limpa esse bit: são quatro noites regadas para a rebrota da berinjela
e três para a da batata-doce, incluindo a atualização que descarta o incremento.
Não se pode colher repetidamente sem atingir maturidade outra vez.

Os cultivos de outono são removidos ao entrar no inverno, inclusive flags e dias;
gramas seguem as regras próprias já implementadas. Não foi acrescentado sprite
murcho de outono na neve. O plantio continua recusado no inverno.

### Desbloqueios

`shop_seed_menu_setup_c` só mostra espinafre com `ESTATE_STATUS_GROUP_C_DONE`.
`milestone_check_group_c` compara os quatro primeiros IDs do grupo (75, 76, 77,
79: berinjela, cenoura, batata-doce e pimentão) com `GROUP_SHIPMENT_THRESHOLD`
(101 nesta revisão). Pimentão/Won e a adaptação desses milestones ainda não
foram entregues. O menu base não passa a prometer espinafre ou progresso completo.

## Estado e persistência

`HmbtnSession.field` continua sendo o único proprietário das plantas;
`HmbtnGameState.inventory/economy/clock` mantém inventário, dinheiro e tempo.
Loja, ações, menus, remessas e renderização acessam essas mesmas instâncias.
Não foram adicionados sincronizações manuais, estado de cultura na cena ou
novos sistemas de compra/crescimento. Desenhar não avança os cultivos.

O save v1 já representa esses IDs, estágios e flags. Não mudou o codec, a versão,
a inicialização do jogo ou o fluxo de instalação pelo BIN. Os testes salvam no
crescimento, antes da rebrota com skip-day e durante seu progresso; destroem a
cena antiga e comparam a sessão recarregada em codificação canônica. Outro
processo retoma o arquivo gerado. Um arquivo separado de inverno verifica que
as plantas removidas não reaparecem após carregar.

O teste de inverno não é só alteração visual: a fixture prepara Outono 30,
dorme na casa pelo fluxo atual, atravessa a virada para Inverno 1, retorna à
fazenda, verifica campo/recursos e salva/carrega esse estado.

## Arquivos modificados

Produção:

- `port/cmake/generate_farm_tables.py`: extrair também `shop_seed_menu_setup_c`.
- `port/game/feed_shop.c` e `.h`: terceiro grupo do catálogo e saudação de outono.
- `port/game/farm_field.c`: aceitar as sementes no motor existente.
- `port/graphics/estate_art.c`: seleção dos três sprites vivos no outono.
- `port/graphics/feed_shop_ui.c`: lista dimensionada aos nomes importados.

Validação:

- `port/tests/summer_crop_test.c`: driver de regras compartilhado por verão e
  outono, com opção `--fall`; vetores originais, noites secas, rebrota por estágio,
  observer idempotente, virada da estação, grama preservada, plantio em quatro
  sentidos e chave correta da mesa. O nome histórico do arquivo foi mantido.
- `port/tests/seed_shop_test.c`: nomes/descrições/transações e navegação do terceiro
  grupo, mantendo as verificações de primavera/verão, saldo e capacidade.
- `port/tests/crop_playthrough.c`: mesmo driver de nabo/batata/verão para as três
  culturas novas. A rebrota varia por fixture, sem algoritmo de jogo duplicado.
- `port/tests/summer_process_playthrough.py`: wrapper compartilhado das culturas
  sazonais, incluindo abertura de saves de crescimento/rebrota e inverno em
  processos novos. Arquivos temporários isolados; nome histórico mantido.
- `port/CMakeLists.txt`: `native_fall_crop_rules`, `native_eggplant_playthrough`,
  `native_carrot_playthrough`, `native_sweet-potato_playthrough`.

Documentação: este relatório, `ROADMAP.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`,
`PORTING_STRATEGY.md`, `ROADMAP_STATUS.md` e `ROADMAP_PENDENTE_CODEX.md`.

## Validação e reprodução

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
ctest --test-dir build/native-debug --output-on-failure \
  -R 'native_(seed_shop|crop_daily|summer|fall|eggplant|carrot|sweet-potato|tomato|corn|onion|grass|potato|crop_playthrough)'
```

Os percursos usam recursos, textos, ANI, terreno, portas/cama e remessas reais
instalados pelo BIN. A fixture começa em Outono 1 (quinta-feira); Outono 3 é uma
data de evento em que Jeff foi encontrado fora da loja. Não foi removida a
verificação de presença/horários para fazer o teste passar. A fixture fornece
1000 G para a compra de três pacotes e considera concluídos os eventos de Thomas
e o convite de Duke para isolar o cultivo; esses eventos conservam seus testes.

Configuração e compilação aprovadas. Após o ajuste da moldura, a suíte final
aprovou **103/103 CTests**, zero falhas, **134,40 s**: 62 testes sem depender dos
recursos opcionais e 41 com `HMBTN_TEST_GAME_DATA` configurado. Os quatorze
direcionados passaram (**14/14**, 10,68 s), incluindo regressões dos cultivos
anteriores. Capturas do menu/quantidade corrigidos, berinjela madura, batata-doce
rebrotada e inverno foram inspecionadas em SDL dummy/software. Preview da loja
no outono executado por três frames. Recompilação inicial reexibiu quatro avisos
const preexistentes na fixture de mapa; build incremental final sem novos avisos.
Produção conserva `-Werror`.

Para testar a loja:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --area-preview 180 --preview-date 2:5 --preview-time 10:00
```

Estações no CLI: 0=primavera, 1=verão, 2=outono, 3=inverno; dias 1..30. Interaja
com os saquinhos de outono na posição correspondente da mesa central. Escolher
um nome abre a quantidade e o total; o balcão mantém sua função.

Para gerar saves reproduzíveis, sem alterar recursos ou seu slot habitual:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_crop_playthrough data --eggplant /tmp/hmbtn-eggplant.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/hmbtn-eggplant.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/hmbtn-eggplant.hms.winter.hms
```

Também disponíveis `--carrot` e `--sweet-potato`. O arquivo principal de
berinjela/batata-doce retoma rebrota com um dia de progresso; o de cenoura retoma
crescimento. Capturas opcionais usam um diretório já existente no último argumento.

Não executados nesta entrega: Windows/cross-build, Release, sanitizers, Android,
sessão manual de janela/áudio e instalação limpa. SDL dummy/software e capturas
não demonstram paridade audiovisual integral. A vegetação sazonal geral ainda
é parcial; o teste de inverno verifica os cultivos deste recorte. Sem PNG,
novas espécies, clima completo, estufa ou rede.
