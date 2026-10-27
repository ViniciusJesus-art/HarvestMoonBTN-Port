# PF-06 — Pink Cat e Toy Flower

Entrega de 2026-10-27 sobre a árvore local existente, referência `f251e35`.
Alvos Linux/Windows; implementação/adaptações em `port/`, sem modificar `src/`
ou o fluxo de instalação pelo BIN. PF-06 permanece aberta: o jogo completo
continua parcial e o bootstrap de diagnóstico não executa o jogo.

## Fontes e comportamento

| Fonte original | Aproveitamento nesta entrega |
|---|---|
| `src/main/game/farm_tables.c`: `CROP_PINK_CAT`, `CROP_TOY_FLOWER`, sementes/produtos | Catálogo já gerado fornece preços, estação, etapas, produto, ausência de rebrota e preço de venda zero. |
| `src/overlay/11_GF_FARM/menu/shop_dialogues.c`: `shop_seed_menu_setup_d` | Grupo D já extraído; seleção filtra suporte conservando a entrada fonte e sua descrição. Mesma máquina de compra/quantidade/pagamento/baú. |
| `src/overlay/11_GF_FARM/field/field.c`: `field_daily_tick`, `field_tile_item_stage`, `field_event_dispatch` | Habilitar os dois IDs no motor já adaptado; nenhuma reescrita do algoritmo de plantio/crescimento/colheita. |
| Mesmo arquivo: `field_cell_look_spring/summer/fall` | Resolver sprites vivos e restos sazonais originais em `estate_art.c`. |
| `src/main/graphics/anim.c`: `anim_draw_sprt_parts` | Resolver a aparência madura explicitamente, sem a indexação original fora das faixas declaradas. |
| `src/overlay/11_GF_FARM/estate.c`: `estate_item_slot_index`; tabelas de produtos/reações | Produtos sem preço de venda não geram remessas; inventário e conversa de presentes existentes já aceitam as flores. |

| Cultura | Semente → produto | Preço da semente | Estação | Dias regados | Etapas | Sprites vivo/murcho |
|---|---|---|---|---|---|---|
| Pink Cat | `0x32` → `0x52` | 200 G | Verão | 6 | 2 + 2 + 2 | 17 / 37 |
| Toy Flower | `0x34` → `0x55` | 500 G | Primavera | 12 | 3 + 3 + 3 + 3 | 13 / 41 |

Manhã seca não avança crescimento. A colheita remove a planta, sem rebrota;
os dois produtos têm preço de venda zero. A recusa de remessa já existente
preserva dinheiro e contadores. Mão, mochila, save e presentes usam o mesmo
inventário autoritativo. As duas flores podem ser dadas a Lillia, usando as
reações e conversas originais. A passagem à estação seguinte conserva o resto
murcho previsto no campo; inverno o limpa.

O menu contextual de Won agora tem cinco linhas: repolho, abacaxi, pimentão,
Pink Cat e Toy Flower. Os índices visíveis `0/1/2/3/4` correspondem aos índices
fonte `0/1/2/4/6`. Descrições das flores são `0x169` e `0x16b`: não se calcula
mais uma descrição presumindo que o índice visível seja o índice fonte.
Moondrop (`0x31`, progressão), Magic Red (`0x33`, variante aleatória) e Orangecup
(`0x35`, estufa) continuam fora da seleção. Não há estoque ou motor paralelo.

## Arte original com leitura segura

As ANI instaladas de Pink Cat e Toy Flower declaram, respectivamente, cinco e
seis faixas de animação. O cálculo genérico `stage + 2` pede os índices cinco e
seis na maturidade, além dessas faixas. O renderizador original não verifica
esse limite e acaba lendo os zeros iniciais do primeiro frame como deslocamento,
mostrando o frame zero maduro. Esse caso já era tratado explicitamente para
repolho no port.

`hmbtn_estate_crop_look` aplica a mesma resolução explícita para as duas flores:
faixa zero na maturidade, `stage + 2` nas demais etapas. Os recursos e seus
limites não são alterados; o decodificador continua recusando índices inválidos.
Capturas das duas flores maduras e do menu foram inspecionadas.

## Estado, saves e arquivos

Nenhum campo persistente foi acrescentado. As definições já existiam no catálogo;
o campo armazena os mesmos IDs/estágios e o inventário armazena os mesmos itens.
O formato de save continua v2, com leitura v1 preservada. As referências ao
inventário central são restabelecidas pelo carregamento existente. Os percursos
verificam retomada no aplicativo em outro processo. Backends SDL3, recursos,
áudio, câmera/portas e correções anteriores foram preservados.

Arquivos de implementação alterados nesta entrega:

- `port/game/feed_shop.c` e `.h`: seleção contextual de cinco linhas com entrada fonte.
- `port/game/farm_field.c`: habilitar duas culturas no motor comum.
- `port/graphics/estate_art.c`: sprites e resolução da faixa madura.
- `port/CMakeLists.txt`: registrar regras de flores e dois casos no percurso de Won.
- `port/tests/flower_test.c` (novo): vetores originais, inventário, remessas, estações e save.
- `port/tests/won_seed_test.c`: seleção/descrições, preço, cancelamento, fundos, baú e lotação.
- `port/tests/won_playthrough.c`: ampliar o percurso existente para as duas flores,
  incluindo mão/mochila, presente real e virada de estação.
- `port/tests/crop_daily_test.c`: corrigir expectativa antiga que tratava morango
  como não suportado; manter Moondrop como exemplo negativo.

Documentação: este relatório, `ROADMAP.md`, `ARCHITECTURE.md`,
`PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `ROADMAP_STATUS.md` e
`ROADMAP_PENDENTE_CODEX.md`. Alterações locais anteriores, inclusive README e
as correções de câmera/sementes, não foram removidas.

## Validação e limites

`cmake --preset native-debug` e `cmake --build --preset native-debug` passaram
no Linux Debug com SDL3 3.4.18 e dados instalados. Foram validados 18 alvos:

- `native_flower_rules`, `native_won_seed_rules`, `native_won_0_playthrough`
  até `native_won_4_playthrough`;
- `native_crop_daily`, `native_summer_crop_rules`, `native_fall_crop_rules`,
  `native_seed_shop`, `native_seed_unlock_rules`;
- `native_save`, `native_inventory_rules`, `native_farm_carry`,
  `native_farm_shipping`, `native_village_gifts_playthrough` e
  `native_weather_doors_playthrough`.

A seleção final de 18 teve 17 aprovações e uma falha de expectativa antiga no
`native_crop_daily` (morango já suportado). Após a correção desse teste, sua
repetição isolada passou; não foi repetida toda a seleção. Os dois novos casos
com recursos também passaram em execuções diretas com capturas, incluindo uma
noite seca, após a última alteração desses percursos.

As primeiras verificações revelaram uma suposição incorreta de slot vazio na
fixture de compra e a necessidade da resolução madura das ANI, ambas corrigidas
antes das aprovações acima. Ao todo, foram 44 processos de testes nesta entrega,
incluindo repetições, wrappers e processos filhos, respeitando o limite 50.
A suíte completa de 118 testes registrados **não foi executada**. Não foram
verificados Windows, Release, sanitizers, janela/áudio físicos ou instalação
limpa. Não há alegação de validação visual lado a lado com o jogo PS1.

Logs locais: `/tmp/hmbtn-flowers-configure.log`, `/tmp/hmbtn-flowers-build.log`,
`/tmp/hmbtn-flowers-recheck.log`, `/tmp/hmbtn-flowers-final-tests.log`,
`/tmp/hmbtn-flowers-daily-recheck.log`. Capturas em `/tmp/hmbtn-flowers-shots/`.
Esses arquivos são artefatos locais de validação e não recursos adicionados ao Git.

## Como experimentar

Won usa a agenda/conversa instalada, na estalagem, área 204. Exemplos para
abrir a área ao meio-dia na estação adequada:

```sh
# Verão, dia 2: comprar Pink Cat e plantar na fazenda.
./build/native-debug/port/hmbtn_native --data data --area-preview 204 --preview-date 1:2 --preview-time 12:00

# Primavera, dia 3: comprar Toy Flower e plantar na fazenda.
./build/native-debug/port/hmbtn_native --data data --area-preview 204 --preview-date 0:3 --preview-time 12:00
```

Fale com Won, selecione a semente e a quantidade. Não há spawn forçado ou
compra alternativa fora da conversa. Plante em solo preparado na estação
correta, regue e durma; colha após seis/doze manhãs regadas. É possível guardar
na mochila, recuperar na mão e dar de presente. O saldo inicial normal é 500 G.
Nomes vêm das tabelas C compiladas; falas/descrições vêm dos textos instalados,
portanto a combinação de idiomas com BIN traduzido continua como antes.

A próxima tarefa do [roadmap](ROADMAP.md) é Magic Red, com sua escolha original
entre variantes azul/vermelha e persistência do RNG de plantio. Nenhuma parte
dessa implementação foi antecipada aqui.
