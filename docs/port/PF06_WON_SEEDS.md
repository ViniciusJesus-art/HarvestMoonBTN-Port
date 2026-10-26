# PF-06 — Sementes sazonais de Won e correção da chuva

Entrega de 2026-10-26, algoritmos da decompilação `d3c0f38` adaptados em `port/`.
Linux e Windows continuam os alvos. PF-06 permanece aberta; a fila principal é
[ROADMAP.md](ROADMAP.md), com desbloqueios do supermercado como próximo recorte.

## Loja e culturas

| Fonte original | Uso nativo |
|---|---|
| Conversa 400 em `A_FILE/12_event_s.bin`; `event_commands.c`: comando 81/dispatch 8, `EVENT_SHOP_SEEDS=10`, início e espera | `village_talk.c` inicia o grupo D no mesmo `HmbtnFeedShop` da sessão. O interpretador aguarda o fechamento e retoma o bytecode. A agenda e a conversa decidem o acesso; não há vendedor paralelo ou spawn forçado. |
| `menu/shop_dialogues.c`: `shop_seed_menu_setup_d` | Gerador extrai oito entradas, nomes e descrições. O catálogo nativo jogável deste recorte contém as três culturas suportadas: repolho, abacaxi, pimentão. |
| `shop_result_request_std`, `shop_buy_stored_item`, `shop_menu_open_choose`, `shop_menu_choose_next` | Perfil de mensagens de Won na mesma máquina usada pelo supermercado: escolha, descrição, quantidade, validações, débito e armazenamento compartilhados. |
| `main/game/farm_tables.c`: `CROP_CABBAGE/PINEAPPLE/GREEN_PEPPER`, itens e preços | Campo habilita os IDs já definidos. Crescimento, secagem, skip-day, consumo de sementes, ferramentas, colheita, estoque, remessas e pagamento usam os módulos existentes. |
| `field/field.c`: `field_cell_look_spring/summer/fall` e `farm_data.c`: packs | Arte sazonal original 11/16/16; nenhuma nova imagem ou implementação de cultivo. |

| Cultura | Semente/produto | Compra/venda | Dias por estágio | Maturidade/rebrota |
|---|---|---|---|---|
| Repolho | `0x26` / `0x45` | 500 G / 250 G | 4 + 5 + 5 | 14 dias regados; sem rebrota |
| Abacaxi | `0x2B` / `0x4A` | 1000 G / 500 G | 5 + 5 + 5 + 5 | 20 dias; retorna ao estágio 3, duração 5 |
| Pimentão | `0x30` / `0x4F` | 150 G / 40 G | 2 + 1 + 2 + 2 | 7 dias; retorna ao estágio 3, duração 2 |

A duração da rebrota é a do estágio original; a flag de skip-day após colher
segue o algoritmo já portado. Os testes verificam essa flag e o primeiro dia
regado que a consome, sem modificar as durações para compensar.

Mensagens específicas: abertura `2:164`, escolha `2:46`, oferta `2:16E`,
quantidade `2:4D`, descrição `2:165..167`, recebimento `2:170`, saída `2:16D`,
cancelamento da quantidade `2:16F` seguido de `2:171`, envio ao baú `3:261`
seguido de `2:170`. Fundos/espaço usam as mesmas recusas originais `4F/54/C1`.
Não reutilizar indiscriminadamente as falas de Jeff para Won.

A conversa empresta a loja através da sessão. A loja modal é atualizada antes
da conversa em espera; ao fechar, o bytecode prossegue e restaura a pose do
morador. O checkpoint de inventário avança após a compra para que uma fronteira
posterior não reverta itens já pagos. Não se tornou um segundo inventário de
execução. Adaptadores ausentes continuam fronteiras explícitas.

Na base atual Won aparece na estalagem, **área 204/localização 27**; o percurso
usa sua posição real da agenda. Não pressupor que todos os interiores sejam o
salão da estalagem ou que festivais mantenham a presença normal do comerciante.
Saves v2 já representam os IDs/estágios; não houve nova versão ou mudança no BIN.

### Particularidade visual do repolho

A inspeção das capturas detectou um detalhe que o ciclo sem enquadrar a planta
não havia exposto: o ANI original do repolho tem cinco ranges. O C original
solicita range 5 na maturidade; `anim_draw_sprt_parts` não valida esse índice e
lê o primeiro frame como um range, usando seu `dx=0` como offset. O resultado
visual é o frame inicial, a cabeça madura do repolho.

O port resolve explicitamente **animação 0 na maturidade** do repolho, sem
reproduzir uma leitura fora da tabela nem afrouxar o decoder de ANI. Estágios
intermediários continuam 3/4; abacaxi/pimentão usam `stage+2`. Há testes do recurso
real, de todos os estágios visíveis e da cabeça madura. Isso adapta o
comportamento do original, mantendo os limites seguros do leitor nativo.

### Estoque ainda parcial

O grupo D original tem oito entradas: repolho, abacaxi, pimentão, moondrop,
pink cat, magic red, toy flower e orangecup. A quarta depende de
`ESTATE_STATUS_BIT23`; as outras entradas/condições foram auditadas. Este recorte
não simula flores ou estufa e **não declara Won completo**. A lista vendida no
host é, por enquanto, o subconjunto das três culturas acima, como os catálogos
sazonais atuais também mantêm fora suas culturas condicionais pendentes.

Morango/abóbora/espinafre continuam bloqueados até portar os milestones reais.
Agora os grupos A/B/C têm seus quatro produtos com cultivo suportado; o limiar
original é **101 remessas de cada produto**, não 100 nem soma do grupo. Essa
implementação ficou como próxima tarefa; nenhuma flag foi ligada artificialmente
em produção para anunciar desbloqueios.

## Chuva: cor e deslocamento originais

A paleta carregada do BIN já é azulada. O erro era a composição: o port desenhava
os sprites da chuva depois da luz global. `main/main.c: main_draw_ots` desenha a
world OT antes de `otTags[1]`, onde `screen_fade.c: screen_fill_quad` aplica a luz,
e antes de `otTags[0]`, a UI. A composição nativa agora segue essa ordem: mundo e
partículas → luz subtrativa → fades → UI. Não foi inventada uma paleta mais azul;
os texels originais recebem o filtro que faltava.

`people/player.c: player_update_position` calcula o scroll pela projeção do novo
centro da câmera; `world/weather.c: particle_step` soma esse deslocamento às
partículas visíveis. `scene_preview.c` agora compartilha a construção da câmera
entre tick e draw; o tick aplica a diferença de projeção por
`hmbtn_weather_effects_scroll`. Movimento preso ao limite da câmera não produz
scroll artificial. Partículas ainda em atraso não se movem pelo scroll e cargas
de área não arrastam partículas por um teleporte. Draw continua sem alterar
posições, idades ou RNG meteorológico.

Testes verificam um pixel opaco real da chuva contra paleta e iluminação
originais, movimento real do personagem/câmera, partículas visíveis/atrasadas,
scroll zero, pausa, RNG isolado e draw repetido. Danos de tempestades, tremor,
trovões e meteorologia integral continuam pendentes.

## De onde vêm os textos e a tradução

A origem atual é **mista**:

- Falas, descrições e instruções: `port/filesystem/text_resources.c` lê em runtime
  os registros de `A_FILE/02_messege0.bin` até `06_messege4.bin`, instalados pelo
  BIN do usuário. A tradução presente nesses registros aparece no port.
- Nomes das sementes: `port/cmake/generate_farm_tables.py` lê
  `src/main/text/text_bank.c: text_bank.names.seeds` e gera `seed_menu.inc` na
  compilação. Os nomes não vêm automaticamente do BIN traduzido.
- Rótulos de menu, como `Money`, unidades e moeda: `port/CMakeLists.txt` gera
  `feed_shop_labels.inc` das tabelas C do mesmo text bank.
- Glyphs/imagens vêm dos recursos instalados; isso não traduz os códigos de texto
  de um rótulo que já foi incorporado ao executável nativo.

As capturas mostram nomes ingleses e instruções portuguesas com os dados locais.
Não foi adicionado um tradutor, uma política de idioma ou extração de rótulos do
executável original nesta entrega. O fluxo BIN e a separação entre lógica e UI
foram preservados.

## Validação e reprodução

Linux x86-64 Debug, SDL3 local 3.4.18, dados instalados presentes:

- `cmake --preset native-debug`: sucesso.
- `cmake --build --preset native-debug`: sucesso; produção com `-Werror`.
- `ctest --preset native-debug`: **109/109 aprovados**, zero falhas,
  **144,24 s**; 64 independentes dos dados opcionais e 45 com dados instalados.
- `native_won_seed_rules`: oito entradas/descrições, subconjunto suportado,
  mensagens próprias, seleção, limites de quantidade, cancelamento, fundos,
  lotação, encaminhamento ao baú e débito/armazenamento únicos.
- `native_won_0/1/2_playthrough`: conversa original, compra real na estalagem,
  descrições/quantidade, fim do bytecode, ferramentas e rega, estágios esperados,
  todos os estágios visíveis, colheita/remessa/pagamento, rebrotas, saves comparados
  e retomada pelo aplicativo em outro processo.
- `native_weather` / `native_weather_playthrough`: scroll e pixel de chuva,
  somados aos testes anteriores de previsão/calendário/irrigação/persistência.
- Regressões existentes de lojas, inventário, cenas, saves v1/v2, economia,
  agricultura, moradores, galinhas, eventos e separação update/draw incluídas.

Capturas locais inspecionadas em `/tmp/hmbtn-won-shots/{cabbage,pineapple,pepper}`
e `/tmp/hmbtn-rain-fixed-shots/`. Snapshot anterior em
`/tmp/hmbtn-won-weather-before/`. Logs `/tmp/hmbtn-won-final-{configure,build,tests}.log`
e `/tmp/hmbtn-won-final-targeted.log`. Não são arquivos distribuídos pelo projeto.

Não executados: Windows/cross-build, Release, sanitizers, Android, instalação limpa
e sessão manual de janela/áudio. Capturas SDL dummy/software não comprovam
fidelidade audiovisual integral. Avisos const preexistentes da fixture de mapa
permanecem; nenhum novo aviso de produção foi introduzido.

Para encontrar Won em uma sessão nova na estalagem:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --area-preview 204 --preview-date 0:5 --preview-time 12:00
```

Fale com ele de mãos vazias. Esse preview é uma sessão de desenvolvimento nova,
não substitui seu save. Para chuva azulada e movimento pela câmera:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --area-preview 0 --preview-date 0:5 --preview-time 10:00 \
  --preview-weather 1:0
```

Percursos/capturas opcionais; índice 0=repolho, 1=abacaxi, 2=pimentão:

```sh
mkdir -p /tmp/won-captures
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_won_playthrough data 0 \
  /tmp/won-cabbage.hms /tmp/won-captures
ctest --preset native-debug -R 'native_(won|weather)' --output-on-failure
```

## Arquivos desta entrega

Relativos à raiz, comparados ao snapshot anterior; alterações anteriores preservadas:

| Área | Arquivos |
|---|---|
| Build/extração | `port/CMakeLists.txt`, `port/cmake/generate_farm_tables.py` |
| Compra/bytecode | `port/game/feed_shop.c/.h`, `village_talk.c/.h`, `session.c`, `exploration.c` |
| Cultivo/apresentação | `port/game/farm_field.c`, `port/graphics/estate_art.c`, `feed_shop_ui.c` |
| Chuva/câmera/composição | `port/game/weather_effects.c/.h`, `port/graphics/scene_preview.c` |
| Testes novos | `port/tests/won_seed_test.c`, `won_playthrough.c`, `won_process_playthrough.py` |
| Regressões/fixtures | `port/tests/weather_test.c`, `weather_playthrough.c`, `field_helpers.h`, `crop_playthrough.c` (refil real compartilhado) |
| Documentação | `docs/port/PF06_WON_SEEDS.md`, `PF06_WEATHER.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`, `ROADMAP.md`, `ROADMAP_STATUS.md`, `ROADMAP_PENDENTE_CODEX.md` |

`src/`, `sdk/`, `disc/`, `tools/`, CMake PS1, backends, instalação e recursos do
usuário não foram alterados. Nenhum recurso proprietário foi acrescentado ao Git.
PNG, conteúdo novo e coop continuam fora deste recorte.
