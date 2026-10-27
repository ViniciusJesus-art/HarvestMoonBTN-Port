# PF-06 — Carpintaria e ampliação do galinheiro

Entrega em 2026-10-27, como primeira dependência da estufa/Orangecup.
A fila principal e os próximos critérios estão no [ROADMAP.md](ROADMAP.md).
O recorte madeira/galinheiro está funcional; a cadeia inteira e PF-06 continuam abertas.

Este é o registro histórico da entrega com save v5. A primeira casa foi integrada
posteriormente em [PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md), com save v6
atual. A próxima obra agora é o celeiro; as limitações da primeira casa citadas
abaixo descrevem o estado anterior.

## O que foi rastreado antes de implementar

- `src/overlay/11_GF_FARM/menu/shop_dialogues.c`: `shop_lumber_menu_setup`,
  `shop_lumber_deal_step`, `shop_menu_choose`, `shop_first_clear_flag` e
  `shop_silo_offers` definem o menu e as cinco obras, em ordem: galinheiro,
  primeira casa, celeiro, segunda casa, estufa. Valores: 5000/420, 4700/370,
  6800/500, 10000/750 e 30000/580 (G/madeira).
- `src/include/items.h` e `src/main/game/farm_tables.c`: madeira `ITEM_LUMBER`
  0x3F, preço de compra 50 G. O nome vem do banco C da interface; diálogos são
  os registros do banco 2 instalado pelo BIN, inclusive sua tradução.
- `src/overlay/11_GF_FARM/event/milestones.c`: `milestone_arm_countdown` inicia
  `stampTimer=3`; `milestone_tick` conclui quando `--stampTimer < 0`.
  `world/clock_events.c` chama durante a passagem da noite: quatro manhãs.
  Não é um contador de frames nem o pedido de melhoria de ferramenta.
- Conversa instalada 374: Gotz (24) precisa chegar ao destino de trabalho 6
  sem item nas mãos para executar dispatch 81/8/9; a própria conversa decide.
  81/8/0 aguarda o modal; 81/8/9 com x=1 consulta obra em andamento.
  `event_commands.c:event_cmd_dispatch` confirma que a consulta significa obra,
  não horário de abertura ou presença do comerciante.
- `people/villager.c:villager_set_destination`: destino é `route >> 8`.
  O adaptador de variável 38 lia incorretamente `place.value` (51 na loja).
  Corrigido para o byte alto da rota (6), com vetor de teste específico.
- `world/scene.c:scene_resolve_selection` e `farm_layout_index`: primeiro nível
  do galinheiro seleciona a fazenda 4+estação e o interior 116+estação.
- `menu/status_pages.c`: cada nível representado contribui 20 para o indicador
  de construções. O indicador é derivado, sem outro estado de melhoria.

`shop_greenhouse_step`/`greenhouse.c` tratam melhorias de ferramentas/acessórios,
apesar do nome; não são o fluxo que compra/constrói a estufa.

## Estado e adaptação

`HmbtnSession.construction` guarda somente as manhãs restantes. O único nível
concluído é `HmbtnPoultry.upgraded`, já usado por vagas/incubação. Compra e pedido
usam `game.inventory.supplies[1]` e `game.economy`; todas as validações precedem
o débito. Não há inventário/carteira de carpintaria ou sincronização manual.
`daily.c` conclui a obra na atualização compartilhada, após clima, uma vez por
data; trocar de área ou carregar não avança o prazo.

As ofertas e o prazo são extraídos de C em `generate_farm_tables.py`. O domínio
C11 adapta os ramos originais, retirando globais, janelas PS1 e acesso a campos
PS1. Nenhum módulo C de `src/` passou a ser compilado diretamente.
O estoque máximo 999 da venda e o lote 99 preservam as regras desse comerciante;
não reduzem o tipo/capacidade geral do inventário nativo e não são teto de
hardware para conteúdos futuros.

O modal é o `HmbtnFeedShop` existente: nomes/descrições, quantidade, confirmação,
respostas e fechamento. A loja não cria outro ator nem força a agenda.
Madeira comprada vai diretamente ao estoque; a compra bem-sucedida encerra a
loja como no original. Obra custa 5.000 G e 420 madeiras e também encerra.
Nova tentativa durante a obra mostra a resposta original sem débito.
Após concluir, a linha da casa fica oculta até integrar o próximo recorte real;
comprar madeira continua disponível.

`hmbtn_village_flag` deriva flags 0x38/0x3A para eventos, rotas e gatilhos; esses
slots do vetor de flags não são usados como cópias do prazo/nível. A interface
recusa sua alteração quando ligados ao domínio. Comando 95 consulta o mesmo
nível. `hmbtn_farm_menu_building_score` deriva o status; o antigo campo
`menu.stats.building_score` foi retirado do estado e do codec.

`hmbtn_world_area_coop` mantém os IDs lógicos de gameplay 0/112 e escolhe os
recursos da melhoria. São carregados mapas, colisão, zonas e texturas reais;
composição sazonal parte da base da variante. O interior oferece dez cochos e
segunda incubadora, aproveitados por `poultry.c`/`farm_feed.c`, sem nova lógica.
O save restaura o nível antes da carga desses recursos.

## Saves durante o desenvolvimento

Conforme a solicitação de 2026-10-27, compatibilidade entre versões foi retirada.
V5 aceita somente v5, salva o prazo em AUXI e mantém inventário, economia,
cultivos, animais, RNGs e progresso atuais. V1..v4/futuras são recusadas com
mensagem para jogo novo; arquivos antigos não são apagados. As regras de
segurança/transação do arquivo atual continuam. A suíte de codec testa rejeição
das versões antigas em vez de gerar/migrar seus layouts.

## Como conferir

Com os dados instalados pelo BIN:

```bash
./build/native-debug/port/hmbtn_native --data data --area-preview 212 --preview-date 0:4 --preview-time 12:00
```

É uma sessão nova para visitar Gotz na agenda de trabalho; não concede dinheiro
ou madeira para encomendar. A data usa estação 0 e dia 4, conforme o CLI. O menu
abre pela conversa com mãos vazias. Triângulo mostra descrição, X confirma,
setas escolhem quantidade, círculo cancela. As falas de custo estão no BIN.

O percurso automatizado fornece explicitamente dinheiro/madeira/ração como
fixture, conversa com o Gotz real e compra o restante; não chama o pedido de
obra diretamente, não cria NPC e não força elegibilidade. Para reproduzir só o
novo alvo, em uma rodada de testes própria:

```bash
ctest --preset native-debug -R '^native_(construction|carpenter_playthrough)$' --output-on-failure
```

O teste com recursos faz compra → reload → encomenda → reload → tentativa
ocupada → quatro noites reais (reload no meio) → fazenda ampliada → entrada no
galinheiro → alimentação do cocho 10 → ovo na incubadora 2 → reload → casa.
O wrapper inicia o aplicativo em outro processo com o save final.
A origem do ovo é uma fixture declarada; não se anuncia um novo sistema de ovos.

## Validação desta rodada

- Configuração e build Linux Debug passaram com CMake/preset existentes.
- `native_construction`: preço, ordem de falhas, cancelamento, estoque cheio,
  recursos exatos, transação atômica, ocupação, prazo, idempotência diária,
  flags derivadas, variável 38, dez vagas, status e save/reload passaram.
- `native_save`: ida/volta da versão atual e rejeição de versões antigas passaram.
- Seleção intermediária: 13/13 CTests, incluindo o percurso novo e regressões
  de eventos, lojas, economia, cultivos, mapa e galinhas.
- Seleção final: 14/16 CTests, 67,42 s. Passaram carpintaria, Moondrop, inventário,
  calendário, sessão, save em outro processo/Continue, tick independente do
  desenho, conversas, loja de ração e primeira manhã.
- Falharam `native_weather_doors_playthrough` e
  `native_farm_interiors_playthrough`: esperam velocidade de porta +=5; a linha
  atual `runtime.c` usa +=8, alterada durante esta rodada e preservada. O diff
  contra a cópia inicial confirma essa única mudança nesse arquivo nesta rodada.
  Não adaptar/reverter essa preferência fora do escopo da carpintaria. Esses
  testes precisam ser reconciliados com a temporização desejada em outra rodada.
- Total: **49 processos de teste**, incluindo wrappers, filhos e repetições,
  abaixo do limite 50. Os primeiros percursos localizaram Gotz às 10h na sua
  rotina de passeio; a fixture passou a usar o destino real de trabalho às 12h.
  Outro ajuste forneceu a ração declarada, inicialmente ausente na fixture.
- Capturas em `/tmp/hmbtn-carpentry-shots`: menu, descrição, quantidade, oferta,
  confirmação, fazenda ampliada e interior. Menu/oferta/interior inspecionados.
- Suíte completa de 124 testes, Windows, Release, sanitizers, instalação limpa
  e janela/áudio físicos não executados. Não se declara toda a suíte verde.

## Arquivos desta entrega

Produção/build:

- `port/game/construction.c/.h` (novos), `session.c/.h`, `daily.c`;
- `port/game/feed_shop.c/.h`, `village_talk.c`, `event_variables.c`,
  `village.c/.h`, `farm_visit.c`, `exploration.c`;
- `port/game/world_map.c/.h`, `farm_menu.c/.h`, `save.c/.h`;
- `port/graphics/scene_preview.c`, `feed_shop_ui.c`, `farm_menu_ui.c`;
- `port/cmake/generate_farm_tables.py`, `port/CMakeLists.txt`.

Testes: `port/tests/construction_test.c` e `carpenter_playthrough.c` novos;
`port/tests/save_test.c` atualizado.

Documentação: este arquivo, `PORTING_STRATEGY.md`, `ARCHITECTURE.md`,
`PORTING_MATRIX.md`, `ROADMAP.md`, `ROADMAP_STATUS.md`,
`ROADMAP_PENDENTE_CODEX.md` e a atualização de política em `PF05_NATIVE_SAVES.md`.
Alterações anteriores preservadas, incluindo Moondrop e a edição concorrente
em `runtime.c`; esse arquivo não foi editado pelo agente nesta entrega.
`src/`, `sdk/`, `disc/`, `tools/`, CMake PS1 e fluxo BIN permanecem intactos.
