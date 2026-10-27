# PF-06 — Primeira ampliação da casa

Entrega de 2026-10-27. A ordem executável continua no [ROADMAP.md](ROADMAP.md).
Registro histórico da entrega com save v6. A obra/cochos do celeiro usam agora
save v7; detalhes em [PF06_BARN_EXPANSION.md](PF06_BARN_EXPANSION.md).
Primeira casa funcional no recorte abaixo; PF-06, a cadeia da estufa e a aquisição
completa dos utensílios de cozinha continuam abertas.

## Percurso entregue

Após ampliar o galinheiro, Gotz oferece a primeira casa na conversa original 374.
O modal existente cobra **4.700 G e 370 madeiras**, após confirmação. Fundos ou
materiais insuficientes, cancelamento e obra em andamento não debitam parcialmente.
O observador diário conclui a obra na quarta manhã, inclusive com save/load
durante o prazo. Não existe um segundo relógio de construção.

Os IDs lógicos de fazenda e casa continuam 0 e 80. O carregador escolhe os
recursos originais 8..11 e 84..87 conforme a estação e o nível concluído.
Entradas, colisões, zonas, modelos e texturas vêm desses recursos. O despertar
e o movimento até a cama usam as posições da casa ampliada. TV, calendário,
baú de ferramentas e diário/sono continuam acessíveis nos novos lugares.

A geladeira e o armário são depósitos reais: mão e mochila acessam os mesmos
registros que scripts, cozinha e save. Abrir um móvel segurando algo não o
atira no chão. A navegação usa a tabela original e respeita a capacidade atual
da mochila. Empilhamento, separação de uma unidade, classes aceitas e recusa
atômica são adaptados de `item_storage.c`; o baú de ferramentas conserva suas
regras existentes.

A cozinha permite escolher ingredientes da mochila/geladeira, preparar sem
receita, consultar e repetir receitas aprendidas, variar ingredientes, cancelar,
receber prato ou fiasco na mão e anotar/substituir receitas. O algoritmo testa
as definições na ordem original e aplica grupos, utensílios obrigatórios e
bônus de ingredientes/temperos. Ingredientes ausentes, mão ocupada e utensílios
não possuídos recusam a operação sem consumo. A seleção guarda IDs transitórios;
a disponibilidade é conferida no inventário autoritativo antes da transação.

Não são concedidos utensílios gratuitos ao ampliar a casa. Preparos sem
utensílios já funcionam; a compra/entrega de utensílios pela TV ainda precisa ser
integrada. Os testes de temperos usam posse explicitamente preparada na fixture.

## Origem e adaptação

Todos os caminhos abaixo são relativos à raiz. `F` significa
`src/overlay/11_GF_FARM`; `P` significa `port/game`.

| Origem | Uso nativo | Substituição das dependências |
|---|---|---|
| `F/menu/shop_dialogues.c`: `shop_first_clear_flag`, `shop_lumber_deal_step`, `shop_silo_offers` | `P/construction.c`, `feed_shop.c`, `port/graphics/feed_shop_ui.c` | Próxima oferta derivada dos níveis autoritativos; mesmo modal, economia e estoque de madeira. Descrição 2:0x135, oferta 2:0x13D e confirmação 2:0x5F extraídas do BIN. |
| `F/event/milestones.c`: `milestone_arm_countdown`, `milestone_tick` | `P/construction.c`, `daily.c` | Tipo pendente e quatro manhãs no observador diário existente. Flags 0x38/0x3A/0x3B e comando 95 consultam os níveis; não os espelham. |
| `F/world/scene.c`: `scene_resolve_selection`, `farm_layout_index` | `P/world_map.c`, `port/graphics/scene_preview.c` | Resolução conjunta da variante de casa/galinheiro e estação; sem mudar o ID lógico dos eventos. |
| `F/farm_data.c`: `house_door_positions`; `F/people/player.c`: `player_result_camera_points` | `P/daily.c`, `house_sleep.c`, `runtime.c` | Despertar (-90,0,541) → (791,0,258); alvo da cama passa de X=-1350 a -2950, Z=-960. Não altera a curva de movimento/portas ajustada pelo usuário. |
| `F/menu/item_storage.c`: despacho, resolução de slots, trocas, empilhamento e `item_markers` | `P/inventory.c`, `house_interaction.c`, `port/graphics/house_ui.c` | Registros host e ponteiros emprestados substituem globais PS1. Tabela de navegação extraída no configure, incluindo níveis de mochila. |
| `F/menu/house.c`: `cooking_find_record`, `cooking_claim_recipe_tools`, `cooking_claim_entry_slots`, `cooking_sum_wanted_cues`, `cooking_clear_pattern_value`, `farm_cue_add_delta` | `P/cooking.c` | Algoritmo adaptado em domínio sem SDL; buffers locais para testar receitas, tabelas e IDs nativos em vez de endereços PS1. |
| `F/menu/house.c`: `cooking_timer_decrement`, `cooking_hold_result`, `cooking_record_store`, disponibilidade e estados do menu | `P/cooking.c`, `house_interaction.c`, `port/graphics/house_ui.c` | Consumo no inventário compartilhado, resultado único na mão, registros persistentes e interface transitória. Mensagens originais 3:0x3B5..0x3C4; parser comum passa a ler escolhas de três e quatro opções. |
| `src/main/game/cooking_recipes.c`, `F/menu/house.h` | `port/cmake/generate_farm_tables.py`, `P/cooking.c` | Extração das 96 definições ordenadas, listas de bônus, grupos e nomes. Não reescrever uma tabela paralela no port. |
| `F/menu/status_pages.c`: pontuação e receitas | `P/farm_menu.c`, `port/graphics/farm_menu_ui.c` | Pontos derivados dos níveis de construção; quantidade de receitas derivada dos registros. Removida a contagem persistente duplicada de receitas. |

Nenhum C de `src/` passa a ser compilado diretamente no executável host nesta
entrega. Há algoritmos C adaptados, dados extraídos e scripts/recursos instalados
interpretados pelo runtime. Não há ABI/MIPS, build PS1 ou SHA-1 no aceite PC.
As quantidades iniciais de conteúdo — 64 posições por depósito, pilhas de 99,
80 registros — preservam as regras atuais; não representam um compromisso de
limitar futuras expansões ao hardware do PS1.

## Donos do estado e saves

| Informação | Única autoridade |
|---|---|
| Equipamento, mochila, mão, baú, geladeira, armário e suprimentos | `HmbtnGameState.inventory`; novos `icebox[64]` e `cabinet[64]` |
| Utensílios possuídos e receitas aprendidas | `HmbtnGameState.cooking`; registros com ID, qualidade, opções e ingredientes |
| Nível da casa, tipo e prazo da obra pendente | `HmbtnSession.construction` |
| Nível/capacidade do galinheiro | `HmbtnSession.poultry.upgraded`, já existente |
| Dinheiro e débitos | `HmbtnGameState.economy`, já existente |
| Cursores, ingredientes selecionados e fase do menu | `HmbtnHouseInteraction.kitchen`; não guarda outro inventário |

O codec **v6 aceita somente v6**, conforme a política de desenvolvimento.
Serializa os dois depósitos, utensílios, receitas, nível e obra pendente;
reconstrói os empréstimos ao carregar e mantém publicação transacional do slot.
Valida classes e contagens dos depósitos, coerência entre prazo/tipo/níveis e
registros de receita contra o algoritmo/tabelas atuais. Não grava ponteiros,
caches de UI ou contadores derivados de receitas/pontos.

## Verificação

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado.
- Seleção final de CTest: **23/23 aprovados, 18,24 s**, com recursos instalados.
  Regras de ampliação, construção, saves/arquivos, inventário, sessão, calendário,
  variáveis, casa, cadastro, mapa, diário, menus; percursos de save, casa ampliada,
  carpintaria, Moondrop, chuva/portas, conversas, galinhas, loja e interiores.
- Novos CTests: `native_house_upgrade` e `native_house_upgrade_playthrough`.
  O primeiro cobre falhas atômicas, prazo/níveis, flags, mapas, depósitos,
  receitas e roundtrip. O segundo usa Gotz, quatro noites reais, reload no prazo,
  móveis, cancelamento/consumo/resultado, receitas, temperos/fiasco, cama ampliada,
  quatro estações e retomada do save pelo aplicativo em outro processo.
- Vetores concretos: bambu+arroz → bamboo rice/qualidade 30; pão+uva → raisin
  bread/30; pepino+sal → pickles/20, com faca → 23; pepino sem opções → fiasco/1.
  Mochila é consumida antes da geladeira, sem retirar duas vezes.
- Capturas de casa, depósitos, ingredientes, lista de receitas, temperos e
  resultado inspecionadas. Desenho verifica estado de gameplay/obra invariantes.
- Testes antigos de portas passaram a verificar a aceleração observada e a curva,
  em vez de fixar 5; preservado o incremento 8 previamente ajustado pelo usuário.
- Foram **48 processos de teste** na entrega, incluindo wrappers/filhos e
  tentativas iniciais corrigidas, abaixo do limite de 50. Não se executou toda a
  suíte. Windows, Release, sanitizers, Android, instalação limpa e sessão manual
  com janela/áudio não foram executados.

Os primeiros testes apontaram erros nas fixtures (minério 0x7B versus trufa 0x5C,
espera de fechamento do modal); foram corrigidos antes da seleção final.

Para reproduzir o novo percurso com os recursos instalados:

```sh
ctest --preset native-debug -R '^native_house_upgrade(_playthrough)?$' --output-on-failure
```

Para experimentar visualmente sem jogar toda a progressão, o percurso também
aceita um destino de save e uma pasta opcional de capturas. Ele usa fixtures
explicitamente descritas, não uma nova aquisição gratuita no jogo normal:

```sh
mkdir -p /tmp/house-shots
./build/native-debug/port/hmbtn_house_upgrade_playthrough data 0 /tmp/house-current.hms /tmp/house-shots
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/house-current.hms
```

`--area-preview 80` isoladamente continua criando estado inicial; não concede
ampliações. Saves anteriores exigem jogo novo. O BIN continua fornecido pelo
usuário e nenhum recurso/captura proprietário foi adicionado ao Git.

## Arquivos desta entrega

- Domínio: `port/game/construction.c/.h`, `cooking.c/.h` (novos), `game_state.h`,
  `inventory.c/.h`, `daily.c`, `house_interaction.c/.h`, `house_sleep.c/.h`,
  `world_map.c/.h`, `village.c/.h`, `village_talk.c`, `feed_shop.c`, `farm_menu.c/.h`,
  `new_game.c`, `session.c`, `exploration.c`, `runtime.c`, `save.c/.h`.
- Apresentação/geração: `port/graphics/scene_preview.c`, `house_ui.c`,
  `feed_shop_ui.c`, `farm_menu_ui.c`; `port/cmake/generate_farm_tables.py`,
  `port/CMakeLists.txt`.
- Testes: `port/tests/house_upgrade_test.c`, `house_upgrade_playthrough.c` (novos),
  `construction_test.c`, `save_test.c`, `inventory_checks.h`, `inventory_rules_test.c`,
  `new_game_test.c`, `house_interaction_test.c`, `weather_doors_playthrough.c`,
  `farm_interiors_playthrough.c`.
- Documentação: este relatório, `ROADMAP.md`, `ROADMAP_STATUS.md`,
  `ROADMAP_PENDENTE_CODEX.md`, `ARCHITECTURE.md`, `PORTING_STRATEGY.md`,
  `PORTING_MATRIX.md`, `PF05_NATIVE_SAVES.md`, nota histórica em `PF06_CARPENTRY.md`.

As alterações anteriores de cultivos, visitas e carpintaria foram preservadas.
`src/`, `sdk/`, `disc/`, `tools/` e CMake PS1 não foram alterados.

## Limites e próxima entrega

A cozinha ainda não está completa: falta adquirir/receber utensílios pela TV,
reproduzir todos os indicadores da ficha de receita e a apresentação sonora/
animação do preparo. Nomes de pratos/temperos vêm das tabelas C originais e
continuam em inglês; falas vêm dos textos extraídos do BIN instalado. Não há
tradução artificial nova. Alimentos/lojas ainda não portados limitam os pratos
alcançáveis em uma partida normal; extrair as receitas não os torna acessíveis.

Após esta casa, a linha de celeiro permanece oculta até existir integração real,
mantendo madeira disponível. A próxima implementação é o **celeiro**, com
auditoria prévia de espécies, capacidade, cochos e objetos para definir um
percurso funcional; não concluir a obra apenas trocando mapas. Segunda casa,
estufa/Orangecup, família e demais sistemas seguem pendentes no roadmap.
