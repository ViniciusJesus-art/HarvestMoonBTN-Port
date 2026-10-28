# PF-06 — Estufa: construção e cultivo

Entrega de 2026-10-28, após a segunda casa. A fila e os critérios continuam no
[ROADMAP.md](ROADMAP.md). Conclui o recorte de construção/cultivo da estufa;
**Orangecup e PF-06 permanecem abertas**.

## Comportamento entregue

Gotz oferece a quinta construção por **30.000 G e 580 madeiras**, após entregar
as quatro anteriores. Usa o mesmo roteiro 374, modal, carteira, estoque e prazo
de quatro manhãs. Descrição 0x138, oferta 0x140 e confirmação 0x5F vêm dos
recursos instalados; o rótulo `hothouse` vem do C original. Cancelar, faltar
dinheiro/madeira ou haver obra pendente não provoca um débito parcial.

`construction.greenhouse` é o único estado de existência da estufa. Dele derivam
as flags 0x39/0x3E, o quarto resultado do comando 95, a pontuação total de 100 e
a seleção de recursos: fazenda **20..23**, estufa **124..127**. A área lógica
interna é 124. As portas originais apontam para o ID sazonal; a resolução e o
auxiliar de percurso dos testes respeitam esse detalhe. Entrar na estufa ausente
é recusado pela seleção de mapas; após perda, o mapa externo não tem sua entrada.

O interior tem **49 células, 7×7**, separadas do campo externo. Ferramentas,
grade das sementes, plantio, rega, crescimento, rebrota, colheita e remessa usam
os sistemas existentes. Os ramos internos da atualização original dispensam
morte por estação/inverno e rega por chuva. As plantas continuam crescendo uma
vez por data mesmo quando o jogador está em outra área. Não foi criado um
segundo algoritmo de cultivo nem um inventário para a estufa.

Modelo, colisões, quads, UVs/CLUTs, portas e pacote de sprites **4** são os
originais. Os quadros de vegetais e flores usam os índices de `greenhouse_draw`;
não se escolhe o pacote externo pela estação. Solo, grama e efeitos usam a
apresentação existente com coordenadas do campo interno. A exceção original de
`weather_fx_frame` impede chuva e tintura de clima no interior. O relógio segue
a tabela de locais original, que o suspende ali. Desenhar não altera o domínio.

Tempestades/nevascas têm a chance original de **31/100** de destruir uma estufa
existente. A perda limpa suas células, conserva seu stream de plantio, atualiza
flags/pontos/mapa e reabre a quinta oferta. Reconstruir usa o mesmo controlador
e custa novamente os mesmos materiais/dinheiro. Não derruba casa/celeiro,
depósitos, receitas ou animais que permanecem nos locais já suportados.

## Estado e arquitetura

| Informação | Proprietário e acesso |
|---|---|
| Construção, existência e RNG de perda | `session.construction`; lojas, mapas, scripts e status consultam esse estado. |
| Células externas | `session.field`, mesma instância já existente. |
| Células internas | `session.greenhouse`, outra instância de `HmbtnFarmField`, com `indoor=true`. |
| Campo ativo | `hmbtn_session_field` / `_const`: empréstimo conforme área e construção, sem copiar/sincronizar células. |
| Regras de cultivo | `farm_field.c`, compartilhadas; tamanho, índice e coordenadas consultam o layout. |
| Mão, ferramentas, sementes, suprimentos e depósitos | `game.inventory`, sem novos arrays de inventário. |
| Carteira/remessa, calendário e receitas | `game.economy`, `game.clock`, `game.cooking`, como antes. |
| Recursos, modelos, atlas e geometria | Host existente; quads são caches transitórios, não estado salvo. |

A memória ainda usa a capacidade do array nativo existente de campo. O layout
interno usa/salva somente 49 registros, e o externo, 1.125. Isso não é um layout
binário PS1. Uma área futura maior exigirá ampliar/evoluir esse armazenamento;
não há uma promessa de suporte ilimitado a conteúdo por esta entrega.

O save passa a **v10**, aceitando somente v10, conforme a decisão de dispensar
compatibilidade durante o desenvolvimento. FILD grava os dois campos por uma
visita comum ao codec, com dimensões verificadas, estação, células e RNGs de
plantio. AUXI grava existência, pedido e RNG de perda. Dimensões, pré-requisitos,
obra incompatível, interior sem construção e células internas sem estufa são
validados. Não se serializam offsets de quads, atlas ou ponteiros host.

O RNG de perda é explícito e salvo, usando a fórmula compartilhada em
`core/psx_random.c`. Ele não consome o RNG de plantio nem depende de frames.
Assim como os demais streams nativos, não reproduz a sequência global do PS1.

## Fontes e reaproveitamento

`F` = `src/overlay/11_GF_FARM`; `P` = `port/game`.

| Origem original | Implementação nativa / estratégia |
|---|---|
| `F/menu/shop_dialogues.c`: `shop_silo_offers`, `shop_first_clear_flag`, `shop_lumber_deal_step`; `event/milestones.c`: `milestone_tick` | `P/construction.c/.h`, `feed_shop.c`, `graphics/feed_shop_ui.c`. Acrescenta a quinta oferta ao fluxo existente, sem outro motor de loja ou prazo. |
| `F/world/scene.c`: `farm_layout_index`, `scene_resolve_selection`, `greenhouse_level_offsets` | `P/world_map.c/.h`, `graphics/scene_preview.c`. IDs estáveis e offsets originais; mesma seleção em viagem e carga de saves. |
| `F/field/field.h`: dimensões/origens; `field.c`: `greenhouse_clear_cells`, `field_daily_tick`, `field_event_dispatch`, `field_gate_open`, `field_tile_item_stage` | `P/farm_field.c/.h`, `session.c/.h`, `daily.c`. Generaliza o motor nativo já adaptado; mesmas definições/estágios, preservando os ramos externos e a proteção interna. |
| `F/field/greenhouse.c`: `greenhouse_world_to_cell`, `greenhouse_probe_sweep`, `greenhouse_update_cells`, `greenhouse_retexture_quad`, `greenhouse_quad_uvs` | `P/farm_field.c`, gerador, host. Coordenadas internas, quads limitados, tabela UV extraída e espelhamento somente na primeira linha. |
| `F/people/player.c`: handlers de campo/ferramentas/colheita; `field_item_requests.c` | `P/farm_action.c`, `farm_carry.c`, `field_effects.c/.h`, `exploration.c`. Resolve o campo ativo e suas coordenadas antes de usar os mesmos algoritmos, animações, consumo e inventário. |
| `F/field/field.c`: `greenhouse_draw`; `farm_data.c`/`farm_load.c`: pacote de itens 4 | `graphics/estate_art.c/.h`, `scene_preview.c`, gerador. Associações de sprites extraídas; resolução madura limitada compartilha o helper existente, sem relaxar validação ANI. |
| `F/world/weather.c`: `weather_fx_frame`, `greenhouse_reset_level`; `world/clock_events.c` | `P/world_map.c`, `construction.c`, `daily.c`, `exploration.c`, `graphics/scene_preview.c`. Proteção visual do interior, perda 31/100, limpeza e reconstrução; sem emular GPU ou RNG global PS1. |
| Flags `GREENHOUSE_BUILT`/`GREENHOUSE_LEVEL_1`, `event_cmd_store_levels`, pontuação de `status_pages.c` | `P/village.c`, `village_talk.c`, `farm_menu.c`; codec nativo em `save.c/.h`. Derivados do dono, sem contagem/flag de existência paralela. |

Nenhum novo módulo original passa a ser compilado diretamente: os blocos acima
dependem de globais e SDK, sendo adaptados em `port/`; tabelas são extraídas no
configure e os recursos continuam instalados pelo BIN do usuário. Preservados
`src/`, `sdk/`, `disc/`, `tools/`, CMake PS1, SDL3, áudio e fluxo de instalação.

## Verificações

- Configuração e build Linux Debug aprovados. A configuração manteve o aviso
  opcional de `libdecor` ausente; não impediu a compilação.
- **22/22 CTests**, **20,24 s**, na seleção principal. Inclui estufa, obras,
  ambas as casas, animais, regras/campo/carry/grama, clima/portas, inventário,
  modelos, flags e saves/Continue em outro processo.
- `native_greenhouse`: fundos/materiais/ocupação/débito, quatro datas/reloads e
  notificação repetida; IDs sazonais/flags/comando 95/status; limites e conversão
  da grade; todos os seeds já suportados nas quatro estações, seca, rebrota,
  grama no inverno, rega protegida; campos/RNGs/save distintos; perda, limpeza
  sem reiniciar plantio e rejeições do codec.
- `native_greenhouse_playthrough`: conversa real de Gotz, cancelar e falhar por
  fundos/madeira, concluir obra, entrar pelas portas originais, 49 quads, arar,
  semear, regar e colher nabo no inverno, levar para fora e vender por 60 G;
  save/reload antes/durante/depois, quatro estações e pacote de sprites correto.
- O mesmo percurso força uma tempestade com sorteio de perda conhecido e
  reconstrói por pedido de produção e quatro noites. Verifica a preservação de
  depósitos/qualidade de receita e vaca/ovelha no celeiro. A primeira compra de
  obra é pelo roteiro real; a reconstrução usa a API de domínio já exercitada.
- Uma execução direta final acrescentou asserts da CLUT/página nos quads reais
  após arar, semear e regar, e passou. Capturas dos estágios, catálogo e interior
  foram conferidas. Capturar compara o save antes/depois para detectar mutações
  de gameplay causadas pelo desenho.
- Fixtures explícitas fornecem obras anteriores, dinheiro/madeira, animais e
  uma semente; a troca para inverno e o catálogo de arte são configurações de
  teste. Não são concessões da construção no jogo normal. O cultivo de nabo,
  alimentação, noites, remessa e recompra da obra usam as ações/domínios reais.
- **50 processos de teste no total**, incluindo wrappers/filhos e tentativas
  de desenvolvimento. Uma expectativa de duração do teste foi corrigida para
  contar um dia mínimo por estágio, inclusive duração zero; a lógica de cultivo
  foi preservada. O auxiliar de viagem passou a resolver IDs sazonais.

Não executados: Windows/cross-build, Release, sanitizers, suíte inteira,
instalação limpa, Android e sessão manual com janela/áudio. As alterações
anteriores presentes no workspace foram mantidas. Nada foi commitado.

## Como experimentar

Na partida normal, conclua as quatro obras anteriores, encomende a estufa com
Gotz e espere quatro manhãs. A porta fica no lado oeste da fazenda. Sementes já
suportadas podem ser cultivadas ali durante todo o ano, com rega manual.

Para gerar um save e capturas de desenvolvimento a partir da raiz:

```sh
mkdir -p build/native-debug/greenhouse-shots
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_greenhouse_playthrough \
  data 0 build/native-debug/greenhouse.hms build/native-debug/greenhouse-shots
./build/native-debug/port/hmbtn_native --data data \
  --load-save build/native-debug/greenhouse.hms
```

O save final é uma fixture com estufa reconstruída, não uma progressão completa
obtida desde o título. `--area-preview 124` sozinho inicia sem a construção;
use a partida ou o save com a obra entregue. Recursos proprietários não foram
adicionados ao Git.

## Arquivos desta entrega

- Build/extração: `port/CMakeLists.txt`, `port/cmake/generate_farm_tables.py`.
- Domínio em `port/game/`: `construction.c/.h`, `session.c/.h`, `daily.c`,
  `farm_field.c/.h`, `farm_action.c`, `farm_carry.c`, `field_effects.c/.h`,
  `exploration.c`, `runtime.c`, `feed_shop.c`, `village.c`, `village_talk.c`,
  `farm_menu.c`, `world_map.c/.h`, `save.c/.h`.
- Apresentação em `port/graphics/`: `scene_preview.c`, `estate_art.c/.h`,
  `feed_shop_ui.c`.
- Testes em `port/tests/`: `greenhouse_test.c` e `greenhouse_playthrough.c`
  (novos); `poultry_helpers.h`, `world_map_test.c`, `construction_test.c`,
  `second_house_test.c`, `house_upgrade_test.c`, `barn_upgrade_test.c`.
- Documentação em `docs/port/`: este relatório, `ARCHITECTURE.md`, `ROADMAP.md`,
  `PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `PF05_NATIVE_SAVES.md`.

## Limites e próxima tarefa

Orangecup permanece fora do filtro de suporte. Transporte de animais para
interiores adicionais e a repatriação especial de `farm_put_back_blocked_animals`
não foram integrados; o recorte animal atual continua nos locais já suportados.
Danos completos do clima externo, plantas/ervas aleatórias, família, utensílios
e ciclo bovino/ovino completo continuam pendentes. A estufa não conclui esses
sistemas por ter um mapa e células funcionais.

A próxima entrega é **Orangecup**, com compra real e percurso de cultivo no motor
agora compartilhado. A ordem e o aceite estão no roadmap, sem nova migração
geral de arquitetura como pré-requisito.
