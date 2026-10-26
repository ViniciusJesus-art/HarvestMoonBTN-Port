# PF-01 — Regras compartilhadas de inventário e dinheiro

Entrega de 2026-10-25. Implementações adaptadas em `port/`; fontes originais
preservadas, referência `d3c0f38`. Não há compilação direta de módulos `src/`
no target nativo. A entrega reutiliza os algoritmos em interfaces de domínio,
sem criar uma camada PS1, outro interpretador ou um inventário paralelo.

## Origem e resultado

| Fonte original | Regra nativa e integração |
|---|---|
| `src/overlay/11_GF_FARM/estate.c`: `estate_gold_change` | `port/game/economy.c`: `hmbtn_economy_gold_change`. Compras de ração/galinha/medicina, venda de galinha, liquidação de remessas e recompensa da torta chamam essa operação. A única atribuição inicial fora dela é o saldo de novo jogo em `game_state.c`. Rollover de contas continua no calendário da remessa. |
| `src/overlay/11_GF_FARM/event/event_commands.c`: `event_items_take` | Travessia já equivalente no host foi conservada em `port/game/inventory.c`. `hmbtn_inventory_count/take` são o núcleo; os antigos `hmbtn_event_items_*` delegam a ele para manter os consumidores. Consulta de medicina e consumo da torta também usam a mesma travessia. |
| Mesmo arquivo: `event_item_store` | `hmbtn_inventory_store_tool`: equipamento → mochila acessível → baú se autorizado. Conversas e compra de medicina usam essa regra. O comando resolve operandos, valida o catálogo atual e calcula modificador; a regra recebe um registro e retorna localização, sem carregar imagem ou mostrar pose. |
| `menu/storage_swap.c`: troca/empilhamento; `menu/item_storage.c`: compactação; `people/player.c`: troca rápida | Algoritmos nativos já baseados nessas funções foram conservados no módulo `inventory.c`; `farm_menu.c` contém a interação do menu. Compactação é compartilhada com o baú da casa. Troca simples do baú conserva sua semântica própria, sem impor os filtros da mochila a ela. |
| `people/player.c`: coleta; `estate.c`: coleta do mundo; `animals/chicken.c`: ovos | Os percursos atuais de campo, coleta silvestre e ovos usam `hmbtn_inventory_hold` para ocupar a mão vazia. Seleção do alvo, remoção do mundo e animação conservam sua responsabilidade e ordem. Esta função é uma pequena interface nativa sobre a atribuição existente, não um novo algoritmo original portado. |

## Contratos e diferenças deliberadas

- `HmbtnGameState.inventory` e `.economy` continuam sendo os proprietários dos
  dados, dentro de `HmbtnSession`. As novas funções não armazenam cópias.
- `estate_gold_change` mantém retornos normal/teto/insuficiente, débito recusado
  sem alteração, reset opcional até zero e contabilização integral de receita
  mesmo no teto. A adaptação calcula magnitudes sem overflow assinado, inclusive
  nos extremos de `int64_t`; ledger usa aritmética módulo 2^32. Estado inválido
  (`gold` acima do teto escolhido) retorna erro sem alteração. O host mantém
  esse invariante; não reduzir `gold_limit` abaixo do saldo em uso.
- `HmbtnEconomy.gold_limit == 0` seleciona 9.999.999 G para o jogo base. Uma sessão
  nativa pode definir outro teto; não há constante de saturação espalhada pelos
  consumidores. Isso não amplia automaticamente o layout gráfico das contas:
  os campos de sete dígitos e os formatos de conteúdo atuais continuam existentes.
- Capacidade da mochila é consultada por `hmbtn_inventory_capacity`; o baú usa
  seu tamanho nativo. Permanecem os níveis 0/1/2 e os arrays atuais 8/64, porque
  esta entrega não amplia conteúdo. A regra de armazenamento recebe registros
  sem impor o teto de 380 IDs; leitores, tabelas, menus e comandos ainda têm
  limites do recorte original e precisarão evoluir ao acrescentar conteúdo.
- Busca conta registros, não unidades de uma pilha. `which=-1` conserva o início
  no índice um, conforme o original. Geladeira/armário ausentes são rejeitados,
  sem simular armazenamento disponível. Remoção não carrega gráficos nem altera
  poses; os consumidores mantêm a apresentação já existente.
- **Correção delimitada:** a troca rápida percorria oito células físicas mesmo
  com mochila menor. Agora compacta/rotaciona apenas as células acessíveis;
  registros inativos permanecem intactos. Nível inválido falha antes de mutação.
  Não houve mudança incidental de RNG, preços, horário de remessa ou animações.

## Inicialização, áreas e persistência

Criar o novo jogo continua inicializando os dados uma única vez. Menus/casa e
transições continuam emprestando os registros da sessão. Checkpoints de conversa
retêm o rollback anterior; a nova operação de armazenamento não tem efeitos de
apresentação ou finanças que escapem desse rollback. Itens em voo, reservas de
animais e itens temporários de eventos mantêm seus contratos existentes.

Saves em disco continuam ausentes; não houve formato de save alterado nem suporte
simulado. Persistência em memória entre áreas/manhã/UI continua protegida pelos
percursos existentes. PF-05 definirá a serialização versionada, incluindo a
política de configuração do jogo; não fazer dump dos novos structs.

## Validação

Dois testes novos, ligados apenas a `hmbtn_port_core`:

- `native_economy_rules`: vetores revisados de `estate_gold_change`, zero,
  insuficiência/reset, teto exato/excedido, ledger integral, extremos definidos,
  configuração nativa acima do teto original e compra/remessa na mesma carteira,
  com execução única e referências vivas do menu.
- `native_inventory_rules`: ordem de armazenamento em todos os níveis atuais,
  mochila/baú cheios, metadados preservados, recusa sem alteração, consulta/remoção
  por registro, sentinelas, capacidade inválida, mão ocupada, ida/volta do ciclo
  com item em slot inativo e medicina vista pela loja/menu/consulta de script.

Os testes anteriores de script cobrem resolução de operandos, modificadores,
notificação, spill e rollback. Os percursos com recursos instalados protegem
lojas, coleta, ovos, Greg/vara, presentes, torta/ingredientes, áreas e manhã.
Resultado final registrado no [roadmap](ROADMAP.md) e no
[status](ROADMAP_STATUS.md). Windows/cross-build, Release, Android, sanitizers,
instalação limpa e inspeção manual de janela/áudio não foram executados nesta
entrega. O fluxo BIN e os backends SDL3/áudio/recursos foram preservados.

## Arquivos desta entrega

- `port/game/economy.c`, `economy.h`, `inventory.c`, `inventory.h` (novos).
- `port/game/game_state.c`, `game_state.h`, `event_item.c`, `village_talk.c`,
  `feed_shop.c`, `farm_shipping.c`, `farm_menu.c`, `farm_menu.h`,
  `house_interaction.c`, `farm_carry.c`, `farm_gather.c`, `poultry.c`.
- `port/graphics/scene_preview.c`: chamadas às operações compartilhadas nos
  callbacks atuais; a coordenação de eventos ainda aguarda PF-03/04.
- `port/tests/economy_rules_test.c`, `inventory_rules_test.c` (novos),
  `economy_checks.h` e `port/CMakeLists.txt`.
- Este relatório, `ROADMAP.md`, `ROADMAP_STATUS.md`, `PORTING_MATRIX.md`,
  `ARCHITECTURE.md`, `PORTING_STRATEGY.md`, `ROADMAP_PENDENTE_CODEX.md`.

As demais alterações locais anteriores foram preservadas. PF-02 é a próxima
entrega; a implementação do cultivo/ciclo diário não foi iniciada aqui.
