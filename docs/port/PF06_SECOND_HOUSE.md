# PF-06 — Segunda ampliação da casa e lareira

Entrega de 2026-10-28. Conclui a quarta obra no recorte do
[ROADMAP.md](ROADMAP.md); PF-06 continua aberta. O alvo é PC Linux/Windows,
sem execução MIPS, compatibilidade binária ou SHA-1 do executável original.

## Comportamento entregue

Depois do galinheiro, primeira casa e celeiro, Gotz oferece a segunda casa por
**10.000 G e 750 madeiras**. Usa a conversa 374 e o mesmo modal: descrição
0x137, oferta 0x13F e confirmação 0x5F dos textos instalados. Fundos, materiais
e obra ocupada são conferidos antes do débito. O pedido consome os recursos uma
vez e termina na quarta manhã, inclusive com saves/reinícios durante a obra.

`session.construction.house_level` passa de 1 para 2. Áreas lógicas continuam
0 e 80; os recursos selecionados são 16..19 para a fazenda e 88..91 para a
casa. Geometria, texturas, zonas, colisões, portas e posições vêm do original.
Carregar/transitar não cria outro inventário, campo, cozinha ou coleção animal.
Geladeira, armário, baú, cozinha/receitas, TV, calendário, opções e diário
continuam pelas interfaces existentes.

As posições de despertar/cama são extraídas das tabelas C no configure. Casa2
acorda em **(902,0,1804)** e usa destino de sono **(-2950,y,-2560)**. A conferência
visual revelou que o caminho anterior selecionava modelos fixos 1/2 para o
morph da cama. Agora usa LINK/slot do recurso original: 1/2 na casa inicial,
2/3 na primeira ampliação e 1/5 na segunda. A outra cama e as cadeiras não são
escondidas. O decoder reserva também as primitivas dos modelos ligados ocultos,
permitindo a troca real durante o sono.

O depósito de madeira (zona 13) permite retirar uma unidade para a mão e
devolvê-la. A capacidade de colheita nativa já existente, **9.999**, foi mantida
e centralizada em `hmbtn_inventory_lumber_limit`; cortar e devolver usam a
mesma consulta. A adaptação evita importar o clamp original de 999 para um
estoque nativo válido maior e perder madeira. O teto de compra do Gotz continua
999, conforme a regra da loja já implementada; não muda a capacidade do estoque.

A lareira (zona 18) aceita madeira comum ou de inverno somente na casa2 de
inverno. A animação termina antes de limpar a mão; acende a flag 0x45 e reduz
cinco de fadiga apenas na primeira entrega desde acordar. Reentrar apaga a
lareira sem restituir esse benefício. Os três quadros originais de textura,
seus retângulos, durações e encadeamento vêm da seção instalada. O tick host
apresenta a animação; desenhar não consome madeira nem altera fadiga.

## Origem e estratégia

`F` = `src/overlay/11_GF_FARM`; `P` = `port/game`.

| Fonte original | Adaptação nativa e dependências |
|---|---|
| `F/menu/shop_dialogues.c`: `shop_silo_offers`, `shop_first_clear_flag`, `shop_lumber_deal_step`; `F/event/milestones.c`: `milestone_tick` | `P/construction.c/.h`, `feed_shop.c`, `graphics/feed_shop_ui.c`. Quarta oferta no controlador, economia e inventário existentes. Não habilita a estufa. |
| `F/world/scene.c`: `scene_resolve_selection`, `farm_layout_index`, `house_level_offsets` | `P/world_map.c`, `graphics/scene_preview.c`. Mesma resolução para entrada, mudança sazonal e restauração de save. |
| `F/farm_data.c`: `house_door_positions`; `F/people/player.c`: `result_camera_pairs`, `player_result_camera_points/start` | `cmake/generate_farm_tables.py` gera `house_layouts.inc`; `P/house_sleep.c` fornece sono e posição de despertar a `daily.c`. Sem ternários que confundam os dois níveis ampliados. |
| `F/world/scene.c`: `scene_link_slot1_reset`, `scene_link_handback` | `P/runtime.c`, `graphics/scene_decode.c`: seleção por flags/slot e reserva das alternativas. Aproveita os morphs e a sequência de sono existentes. |
| `F/people/player.c`: `player_zone_lumber_stock`, `player_act_zone_front`, `player_run_action` (`PLAYER_MODE_DELIVER_LUMBER`), reset de `lumberDeliveries`; `estate.c`: `estate_lumber_add/take`; `picture_cells.c`: `farm_clamp_status` | `P/house_lumber.c/.h`, `inventory.c/.h`, `farm_field.c`, `exploration.c`, `daily.c`, `runtime.c`. Algoritmo adaptado ao estado central e à capacidade nativa atual; sem estrutura paralela de itens. |
| `F/world/scene_view.c`: `slot_image_step`; `FarmModeScript.frames` da seção 91 | `graphics/scene_preview.c:fire_tick`. Retângulos LE validados e cópia no atlas já existente, substituindo `MoveImage`. Sem ponteiros PS1 nem um novo renderer. |
| Flag `EVENT_FLAG_HOUSE_LEVEL_2`, comando 95, pontuação de `status_pages.c` | `P/village.c`, consumidores existentes `village_talk.c`/`farm_menu.c`. Flag 0x3D e 80 pontos derivados dos níveis; escrita direta da flag gerenciada é recusada. |

Nenhum novo módulo C de `src/` é compilado diretamente: os blocos acoplados ao
estado global/SDK foram adaptados em `port/`, e as tabelas numéricas extraídas
no configure. `src/`, `sdk/`, `disc/`, `tools/`, CMake PS1, SDL3 e instalação pelo
BIN fornecido pelo usuário permanecem preservados. Capacidades são regras de
conteúdo atuais, sem compromisso de manter limites do PS1 para futuras expansões.

## Estado e persistência

| Informação | Único proprietário |
|---|---|
| Nível da casa/celeiro, tipo e prazo de obra | `session.construction` |
| Equipamento, mochila, mão, depósitos e madeira | `game.inventory` |
| Dinheiro | `game.economy` |
| Utensílios e receitas | `game.cooking` |
| Animais | `session.livestock` / `session.poultry` |
| Entregas de lenha desde acordar e ação transitória | `session.house_lumber` |
| Lareira acesa nesta visita | `village.flags[0x45]` |
| Fadiga | `session.farm_action` |

Save **v9**, sem leitores/migradores antigos. AUXI inclui o contador de lenha;
VILL já inclui a flag da lareira. O contador sobrevive às transições e à carga;
acordar o zera. Entrar em área limpa a flag, mas restaurar um save conserva sua
visita acesa. O cursor de textura é um cache visual transitório e pode reiniciar.
Não se salva durante animação de transferência de madeira.

O validador aceita níveis 0..2 com pré-requisitos coerentes, incluindo celeiro
antes da segunda casa. Recusa obra já concluída, prazo sem tipo e níveis
inválidos. Vínculos de inventário/cozinha/construções/animais são refeitos após
decodificação. Outros formatos são recusados com mensagem, sem apagar arquivos.

## Verificação executada

- `cmake --preset native-debug` e `cmake --build --preset native-debug`: aprovados.
- Seleção principal: **23/23 CTests**, **69,05 s**. Inclui construção/ambas as
  casas, animais/celeiro, mapas, inventário/status, modelos/formatos gráficos,
  alimentação/galinhas, flags, saves e retomada pelo app, portas/clima e
  atualização sem desenho. Os wrappers das cinco obras/animais carregam os
  saves em outro processo; o wrapper de save também exercita Continue real.
- Após centralizar a capacidade de madeira, **2/2 verificações dirigidas**
  (`native_second_house`, `native_farm_field`) e o percurso instalado da casa2
  foram executados novamente com sucesso. A seleção principal não foi repetida.
- `native_second_house` cobre custo/recusas/ocupação, débito único, quatro
  manhãs/reloads/idempotência, flag/comando 95/status, quatro estações, sono e
  despertar, madeira comum/invernal, lareira, capacidade 9.999, persistência de
  inventário/qualidade de receita/utensílios/animal e rejeições do codec.
- `house_upgrade_playthrough.c` atende ambas as casas sem duplicar o roteiro.
  Exercita Gotz real, obra/noites, save/reload, mapas/portas, móveis/depósitos,
  preparo/cancelamento/receitas/temperos/fiasco, cama/diário, quatro estações e
  lareira/entrada com madeira na mão. A captura confirma invariância do domínio.
- Estados prévios de obra, dinheiro, animais, ingredientes e temperos são
  fixtures explícitas. Não são concessões feitas pela obra numa partida normal.
  As compras reais de animais permanecem no teste específico de Barley.
- Capturas das duas camas, menus, interior e lareira foram conferidas. A seleção
  do modelo agora também tem asserts dos índices reais/visibilidade/morph.
- **49 processos de teste no total**, contando wrappers/filhos, verificações
  iniciais e execuções diretas para capturas. Inclui uma tentativa encerrada
  por diretório de captura ausente, corrigido antes de repetir o percurso.

Windows/cross-build, Release, sanitizers, suíte inteira, Android, instalação
limpa e sessão interativa com janela/áudio não foram executados. A configuração
SDL avisou sobre `libdecor` opcional ausente; não impediu o build Linux.
Persistem avisos de descarte de `const` no teste existente
`port/tests/world_map_playthrough.c`, que não foi alterado nesta entrega.

## Como experimentar

No jogo normal, complete as três obras anteriores e encomende a quarta com Gotz.
Nenhum mapa de prévia concede automaticamente esses níveis.

Para repetir o percurso e gerar um save de desenvolvimento ao fim dele, a partir
da raiz do repositório, com os recursos instalados em `data`:

```sh
mkdir -p build/native-debug/house2-shots
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_house_upgrade_playthrough \
  data 2 build/native-debug/house2.hms build/native-debug/house2-shots
./build/native-debug/port/hmbtn_native --data data \
  --load-save build/native-debug/house2.hms
```

Modo `2` escolhe a segunda ampliação no teste; modo `0` conserva o percurso da
primeira. O save final contém a casa2 no inverno e é uma fixture, não uma
partida obtida integralmente por progressão. Não contém nem distribui os recursos
do jogo; eles continuam vindo da instalação local pelo BIN.

## Arquivos alterados

- Integração: `port/CMakeLists.txt`, `port/cmake/generate_farm_tables.py`.
- Domínio em `port/game/`: `construction.c/.h`, `feed_shop.c`, `world_map.c`,
  `village.c`, `house_sleep.c/.h`, `house_lumber.c/.h` (novos), `daily.c`,
  `runtime.c`, `exploration.c`, `session.h`, `save.c/.h`, `inventory.c/.h`,
  `farm_field.c`.
- Apresentação em `port/graphics/`: `feed_shop_ui.c`, `scene_preview.c`,
  `scene_decode.c`.
- Testes em `port/tests/`: `second_house_test.c` (novo),
  `house_upgrade_playthrough.c`, `construction_test.c`, `barn_upgrade_test.c`,
  `model_test.c`.
- Documentação em `docs/port/`: este relatório, `ROADMAP.md`, `ARCHITECTURE.md`,
  `PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `PF05_NATIVE_SAVES.md`.

## Limites e próxima entrega

Família/casamento/filho, aquisição de utensílios pela TV, cozinha com toda a
apresentação original, ciclo bovino/ovino completo e estufa continuam pendentes.
Zonas de entrega familiar e a outra cama no recurso não significam família
implementada. Não há inventário, nível de casa ou campo duplicados nesta entrega.

A próxima implementação é a **estufa**, conforme a ordem e o aceite publicados
no roadmap; Orangecup vem depois do campo interno utilizável. Não foi criada
uma nova migração geral de arquitetura como pré-requisito.
