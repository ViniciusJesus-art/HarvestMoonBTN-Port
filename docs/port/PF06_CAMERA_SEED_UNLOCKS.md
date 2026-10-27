# PF-06 — Câmera nas portas, item carregado e sementes desbloqueáveis

Entrega de 2026-10-26, referência `f251e35`. Adaptações somente em `port/`;
recursos instalados pelo BIN e infraestrutura SDL3 preservados. O jogo completo
continua parcial; o bootstrap de diagnóstico não executa o jogo.

## Câmera e oclusão do item

`game/runtime.c` e `game/exploration.c` deixavam de aplicar `hmbtn_world_camera`
enquanto `door_tick` estava ativo. O movimento da passagem copiava a posição do
jogador para a câmera, produzindo o salto quando o jogador estava além dos
limites de acompanhamento. A limitação agora é aplicada também nesses ticks.
Recuo, velocidade, tempo da porta e fades existentes foram preservados.
Referência: `src/overlay/11_GF_FARM/people/player.c: player_update_position`,
que segue a posição e aplica `scene_clamp_result` na mesma atualização.

O item era desenhado por `house_ui_picture` depois de todos os modelos/atores,
como uma camada de tela. Agora `graphics/scene_preview.c` o anexa ao sprite do
jogador, por `HmbtnWorldAttachment`/`hmbtn_model_render_instances_attached` em
`graphics/model_render.c/.h`. O anexo é desenhado logo depois do corpo, na mesma
profundidade; portas, paredes e atores à frente encobrem o grupo inteiro.
A ordenação do restante do cenário permanece no renderer existente. Mostras de
ferramenta, pesca, evento e medicina usam o mesmo caminho quando ligadas ao
personagem. Projéteis/boia e marcadores independentes não foram migrados aqui.

`graphics/house_ui.c/.h` expõe uma referência à ANI/atlas do cache já existente;
menus e mundo reutilizam os mesmos recursos, sem outro carregador de imagens.
Não há cópia do inventário nem avanço de gameplay durante desenho.
Referência: `player_draw`/`player_draw_carried_item` no original compartilham a
profundidade do jogador. A escolha nativa desta entrega mantém o item acima do
corpo, como solicitado, sem colocá-lo acima do cenário.

O caso real da porta do supermercado na área 40 fica além do limite Z=300 da
câmera. O teste verifica a posição limitada a cada tick de recuo, avanço e
viagem; não modifica os limites do recurso nem libera entrada em loja fechada.
Há também cobertura da porta da casa na região livre da câmera. Capturas
confirmam o item encoberto ao entrar e a câmera estável no limite.

## Desbloqueios do supermercado

`event/milestones.c: milestone_check_group_a/b/c` testa quatro produtos por
estação, todos com **pelo menos 101 unidades enviadas**. O grupo C contém dados
extras no array original: apenas suas quatro primeiras entradas são usadas.
O gerador nativo importa os grupos/limiar e as linhas condicionais originais de
`menu/shop_dialogues.c: shop_seed_menu_setup_a/b/c`, com IDs, descrições e nomes.

`game/farm_shipping.c: hmbtn_farm_shipping_seed_unlocked` adapta a consulta sobre
os contadores autoritativos em `HmbtnGameState.economy.shipments`, usando o mesmo
mapeamento de remessas já existente. O original recalcula seus bits derivados na
inicialização, carga de área e atualização de loja; o menu nativo consulta os
contadores ao abrir/mostrar seu estoque, que permanece pausado durante compra.
Não há outra contagem nem flags persistidas para manter sincronizadas.

`HmbtnFeedShop.economy` empresta a economia da própria sessão. O reset da loja
preserva esse empréstimo e o das galinhas. `hmbtn_seed_shop_count/choice` fornece
a quinta linha apenas quando liberada; UI, descrição, quantidade, pagamento,
cancelamento e envio ao baú seguem a loja compartilhada. As APIs de catálogo
base continuam retornando as quatro linhas incondicionais.

| Estação/grupo | Produtos exigidos | Semente liberada | Compra/venda | Crescimento |
|---|---|---|---|---|
| Primavera/A | Nabo, batata, pepino, repolho | Morango `0x25` → `0x44` | 150 G / 30 G | 8 atualizações regadas; retorna ao estágio 1, com etapas de 3+2 e skip-day original |
| Verão/B | Tomate, milho, cebola, abacaxi | Abóbora `0x2a` → `0x49` | 500 G / 250 G | 14 atualizações regadas; planta removida na colheita |
| Outono/C | Berinjela, cenoura, batata-doce, pimentão | Espinafre `0x2f` → `0x4e` | 200 G / 80 G | 5 atualizações regadas; planta removida na colheita |

`game/farm_field.c` apenas habilita essas definições no motor existente;
`graphics/estate_art.c` resolve os sprites 10/15/15 e o murchamento correspondente,
conforme `field_cell_look_spring/summer/fall`. Não introduz outro cultivo, diário,
estoque, economia ou algoritmo de rebrota. Os valores acima vêm de
`src/main/game/farm_tables.c` e `picture_cells.c`, sem inferir durações de guias.

Save v2 já persiste remessas, sementes e estágios; o formato não mudou.
Construtores/rebind da carga emprestam a economia restaurada para a loja.
As condições específicas de flores de Won, variantes aleatórias, estufa e clima
integral continuam pendentes; PF-06 permanece aberta.

## Validação com limite de execuções

Configuração/build Linux Debug passaram. Seleção final de **20/20 CTests**
passou, zero falhas, **17,85 s**: renderer, desbloqueios e três percursos, lojas,
inventário, saves, evento de item, nabo/tomate, chuva/portas, pesca, transporte de
galinhas, interiores e remessas. O caso adicional da câmera no limite foi
acrescentado depois e passou na execução direta do percurso de portas.

- Renderer: o anexo cobre o dono; parede e morador à frente cobrem ambos;
  anexos inválidos são rejeitados. Mesmo renderer para chamadas com/sem anexos.
- Regras: 100/101 em cada produto de cada grupo; grupos independentes; consultas
  não alteram flags; linha/descrição/custo, falta de dinheiro/espaço e baú;
  save/load empresta a economia restaurada, sem ponteiros de outra sessão.
- Recursos reais: última unidade enviada por arremesso na caixa; loja na mesa
  antes/depois do desbloqueio, descrição, cancelamento, compra, noites secas e
  regadas, cada estágio/ANI, colheita/remessa/pagamento, rebrota do morango,
  carga durante crescimento e retomada pelo aplicativo em outro processo.
  Apenas histórico de remessas/purse/solo inicial são fixtures declaradas.
- Capturas software inspecionadas: porta da casa, entrada do supermercado no
  limite e menu com morango. Inventário continua íntegro durante a passagem.

Respeitado o limite solicitado: 28 execuções de CTest, incluindo repetições,
e quatro diretas para capturas/caso de borda. Incluindo os 16 processos filhos
dos percursos e cargas, foram **48 processos de teste/verificação**, abaixo de
50. As falhas intermediárias foram de fixtures: altura do jogador na mesa e
uma data em que o lojista estava ausente; corrigidas nos testes, preservando
as regras originais de acesso da produção.

A suíte completa de 115 CTests não foi executada nesta entrega. Windows,
Release, sanitizers, sessão de janela/áudio físicos e instalação limpa não
foram validados. Nenhum recurso proprietário foi adicionado ao Git.

## Reprodução

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(model_renderer|seed_unlock_rules|seed_unlock_[012]_playthrough|weather_doors_playthrough)$'
./build/native-debug/port/hmbtn_native --data data --area-preview 40 \
  --preview-date 0:3 --preview-time 12:00
```

O preview usa um jogo novo; as sementes extras precisam das remessas. Não foi
adicionado desbloqueio artificial de produção. Os testes de percurso aceitam
`data <0|1|2> <save.hms> [diretório-de-capturas]` para reproduzir cada estação.
O próximo recorte está exclusivamente no [roadmap](ROADMAP.md).

## Arquivos alterados

- Câmera: `port/game/runtime.c`, `exploration.c`.
- Renderização/cache: `port/graphics/model_render.c/.h`, `scene_preview.c`,
  `house_ui.c/.h`.
- Desbloqueios/cultivo: `port/game/farm_shipping.c/.h`, `feed_shop.c/.h`,
  `session.c`, `farm_field.c`, `port/graphics/estate_art.c`.
- Extração/build: `port/cmake/generate_farm_tables.py`, `port/CMakeLists.txt`.
- Testes: `port/tests/model_test.c`, `weather_doors_playthrough.c`, novos
  `seed_unlock_test.c` e `seed_unlock_playthrough.c`. Percursos reutilizam o
  driver de carga em outro processo existente.
- Documentação: este relatório, estratégia, arquitetura, matriz, roadmap,
  status e backlog complementar em `docs/port/`.
