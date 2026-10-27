# PF-06 — Magic Red e suas variantes

Entrega de 2026-10-27 sobre a árvore local existente, referência `f251e35`.
Alvos Linux/Windows; Android posterior. Adaptações somente em `port/`; `src/`,
backends SDL3, instalação pelo BIN e alterações locais anteriores preservados.
PF-06 permanece aberta; o jogo completo continua parcial e o bootstrap de
opções de diagnóstico não executa o jogo.

## Fontes aproveitadas

| Fonte original | Integração nativa |
|---|---|
| `src/overlay/11_GF_FARM/field/field.c`: `field_event_dispatch` | `game/farm_field.c`: depois de validar uma célula, sortear uma vez para `0x33` e gravar o ID da variante no campo. |
| `src/main/graphics/screen_fade.c`: `rand_range` | Reutilizar `core/psx_random.c` existente, com sequência pertencente ao campo da sessão, sem consumo por frame PS1. |
| `src/main/game/farm_tables.c`: `CROP_BLUE_MAGIC_RED`, `CROP_RED_MAGIC_RED`, itens `0x33`, `0x178`, `0x179`, `0x53`, `0x54` | Mesmo catálogo gerado, motor de crescimento, ações, inventário, presentes e remessas. |
| `field.c`: `field_daily_tick`, `field_tile_item_stage`, `field_cell_look_fall` | Mesma atualização diária e colheita; resolução dos sprites 17 e 25 no adaptador de arte. |
| `src/main/graphics/anim.c`: `anim_draw_sprt_parts` | Resolver a faixa madura zero sem repetir a indexação original além das faixas declaradas. |
| `src/overlay/11_GF_FARM/menu/shop_dialogues.c`: `shop_seed_menu_setup_d`, compra/quantidade/baú | Filtro contextual existente passa a publicar a semente; continua usando a própria entrada fonte e descrição. |
| `src/overlay/11_GF_FARM/estate.c`: `estate_item_slot_index`; `src/main/game/picture_codes.c` | Preço/mapeamento de remessa e reação de presentes existentes, sem inventar um contador para a flor vermelha. |

## Regras e propriedade do estado

A semente custa **600 G**. No plantio original, `rand_range(25) == 0` produz
`ITEM_RED_MAGIC_RED_PLANT` (`0x179`); os demais resultados produzem a azul
(`0x178`). O sorteio acontece **por célula aceita**, depois das verificações de
solo e limites, na conclusão da ação com a grade original de nove posições.
O pacote continua sendo consumido conforme a ação existente, mesmo quando
nenhuma célula aceita o plantio; nesse caso, o RNG não avança.

`HmbtnFarmField.plant_random` é o único proprietário da sequência de plantio.
O construtor inicia em 1. A fórmula já adaptada em `core/psx_random.c` usa
aritmética explícita de 32 bits; isso reaproveita o algoritmo, sem emulação,
layout MIPS ou necessidade de corresponder à sequência global por frame PS1.
Clima, moradores, pesca e galinhas conservam suas sequências anteriores.
Não se usa relógio de parede ou estado global oculto para escolher variantes.

O ID resultante já fica em `HmbtnFarmTile.value`. Crescimento, desenho, menus,
transições e carregamento não sorteiam novamente nem mantêm uma segunda cópia
da variante. Não há outro campo, estoque, economia, inventário ou interpretador.

| Propriedade | Azul | Vermelha |
|---|---|---|
| ID da planta | `0x178` | `0x179` |
| Produto colhido | `0x53` | `0x54` |
| Estação | Outono | Outono |
| Etapas regadas | 3 + 3 + 2 + 2 = 10 manhãs | 3 + 3 + 2 + 2 = 10 manhãs |
| Rebrota | Nenhuma | Nenhuma |
| Sprite de campo | 17 | 25 |
| Remessa | Sem valor; recusada pelo caminho existente | 200 G; pagamento adiado existente |

A vermelha não possui um contador dedicado em `estate_item_slot_index`.
O caminho nativo já acumula seu valor sem incrementar contadores de outras
culturas. A azul pode ir à mochila e ser dada de presente; ambas consultam o
mesmo registro de reações da figura 125. Para Lillia o valor original é **0**,
diferente do **1** usado por Pink Cat/Toy Flower. O teste usa o valor original,
e o percurso azul entrega a flor colhida pela agenda e conversa reais.
Inverno limpa as duas plantas no mesmo processamento diário existente.

As duas ANI de campo declaram seis faixas; `grown + 2` pediria a faixa 6 na
maturidade, fora do intervalo. Como nas flores anteriores, o original acaba
usando zeros do primeiro frame e exibindo o frame maduro zero. O adaptador
resolve isso explicitamente para ambas, mantendo o decodificador validado.
Foram conferidas as ANI instaladas e capturas dos dois sprites maduros.

## Loja e saves

Won agora oferece **seis das oito entradas** do grupo D. A seleção existente
fica `repolho / abacaxi / pimentão / Pink Cat / Magic Red / Toy Flower`, com
índices fonte `0/1/2/4/5/6`. Magic Red ocupa a linha visível 4/fonte 5,
descrição `0x16a`; Toy Flower passa à visível 5/fonte 6, descrição `0x16b`.
Compra, fundos, quantidade, cancelamento, baú e lotação usam a mesma máquina.
Moondrop e Orangecup continuam fora do menu até integrar suas condições.

O escritor nativo passa à **versão 3**, acrescentando um u32 little-endian de
`field.plant_random` ao final de AUXI, depois do RNG meteorológico de v2.
Os demais campos/seções não mudam. O leitor aceita versões **1, 2 e 3**;
v1/v2 retêm a seed inicial 1 do construtor para plantio, sem alterar as células.
O clima de v2 conserva sua sequência; v1 conserva a migração meteorológica
anterior. Não existia sequência de plantio nesses formatos antigos.
Versões desconhecidas e seções inválidas continuam recusadas.

A reconstrução de recursos não reinicializa o campo. Testes verificam o save
antes do plantio, após a escolha e durante o crescimento, com continuação
idêntica da sequência restaurada. Os dois percursos também foram retomados
pelo aplicativo nativo em outro processo. Não é suporte a saves de memory card
PS1 nem compromisso de compatibilidade binária com o executável original.

## Arquivos desta entrega

- `port/game/farm_field.c/.h`: estado proprietário e adaptação do sorteio por célula;
  habilitação da semente e dos IDs de variantes no motor comum.
- `port/game/save.c/.h`: escrita v3 e leitura v1/v2/v3.
- `port/graphics/estate_art.c`: sprites e faixa madura segura das variantes.
- `port/game/feed_shop.h`: descrição atualizada do catálogo contextual existente;
  `feed_shop.c` já filtra as entradas pelo suporte e não precisou de outra regra.
- `port/CMakeLists.txt`: regras Magic Red e dois percursos adicionais no driver de Won.
- `port/tests/magic_red_test.c` (novo): vetores de RNG/etapas, grade com células
  recusadas, espera da ação, persistência, renda e independência de sequências.
- `port/tests/save_test.c`: v3 e fixtures nos layouts v1/v2, preservação de células/clima.
- `port/tests/won_seed_test.c`: seis linhas e preço/descrição/fundos/baú da nova semente.
- `port/tests/won_playthrough.c`: seleção por item, ambos os resultados controlados,
  save antes/depois do plantio, arte/crescimento, presente azul e remessa vermelha.
- `port/tests/flower_test.c`: atualizar a expectativa de suporte para Magic Red.
- Documentação: este relatório, `ROADMAP.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`,
  `PORTING_STRATEGY.md`, `ROADMAP_STATUS.md` e `ROADMAP_PENDENTE_CODEX.md`.

## Validação limitada

Configuração e compilação Linux Debug passaram com SDL3 3.4.18 e dados locais
instalados. Seleção final: **18/18 CTests, zero falhas, 7,25 s**:

```sh
ctest --preset native-debug --output-on-failure -R '^native_(magic_red_rules|save|save_files|flower_rules|won_seed_rules|won_[0-4]_playthrough|crop_daily|seed_shop|seed_unlock_rules|inventory_rules|farm_field|farm_carry|farm_shipping|weather_doors_playthrough)$'
```

`native_won_5_playthrough` (azul) e `native_won_6_playthrough` (vermelha) passaram
na seleção inicial com retomada por aplicativo em outro processo e também em
execuções diretas finais com capturas. Os 20 alvos distintos estão validados.
A primeira execução do novo teste de regras supunha a reação de Lillia igual
à das flores anteriores; foi corrigida pelo valor da tabela original, passou
na repetição isolada e na seleção final. Não houve falha de implementação
nessa rodada inicial. A fixture de retomada foi preparada para reemprestar a
agenda após decodificar, como exigido pelo contrato existente de saves.

| Rodada | Testes/processos de entrada | Filhos adicionais | Total |
|---|---|---|---|
| Primeira seleção | 6 | 4 | 10 |
| Regras isoladas | 1 | 0 | 1 |
| Seleção final | 18 | 13 | 31 |
| Percursos diretos finais com capturas | 2 | 0 | 2 |
| Total conservador | 27 | 17 | **44** |

Limite de 50 respeitado. A suíte completa de **121 testes registrados não foi
executada**. Windows, Release, sanitizers, janela/áudio físicos e instalação
limpa não foram verificados. Capturas verificam os recursos nativos instalados,
sem alegar comparação lado a lado com uma execução PS1.

Logs locais em `/tmp/hmbtn-magic-configure.log`, `/tmp/hmbtn-magic-build.log`,
`/tmp/hmbtn-magic-first-tests.log`, `/tmp/hmbtn-magic-rules-recheck.log`,
`/tmp/hmbtn-magic-final-tests.log`. Capturas em `/tmp/hmbtn-magic-shots/`.
Nenhum recurso proprietário ou captura foi adicionado ao Git.

## Experimentar no jogo

```sh
./build/native-debug/port/hmbtn_native --data data --area-preview 204 --preview-date 2:2 --preview-time 12:00
```

Converse com Won na estalagem e escolha Magic Red. É necessário ter 600 G;
o preview conserva o saldo inicial normal de 500 G, portanto obtenha dinheiro
jogando ou continue um save com saldo suficiente. Plante no outono, regue e
durma por dez manhãs regadas. Cada célula decide sua cor no plantio; carregar
não muda o resultado. Não foi acrescentado um comando de jogo para forçar cor.

Para reproduzir os percursos controlados e suas capturas, após criar a pasta
de saída desejada, use `hmbtn_won_playthrough data 5 SAVE PASTA` para azul ou
`hmbtn_won_playthrough data 6 SAVE PASTA` para vermelha. São fixtures de teste,
com saldo e sequência declarados, usando a conversa e o plantio reais.

A próxima tarefa é Moondrop com seu desbloqueio por evento, conforme a ordem
e os critérios do [ROADMAP.md](ROADMAP.md). Não foi antecipada nessa entrega.
