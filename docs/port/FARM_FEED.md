# Depósitos e cochos

`port/game/farm_feed.c` adapta os ramos de `people/player.c` para retirar,
devolver e colocar forragem/ração. O celeiro usa o estoque compartilhado de
forragem; o galinheiro usa o de ração. A mão é a mesma da mochila, sem cópia
paralela do inventário. Estoques começam em zero, conforme o estado inicial.

Z/Cross consulta a zona à frente do personagem. O depósito de forragem não
exige direção; o de ração e os cochos respeitam as direções originais, inclusive
os grupos horizontal/vertical. Um depósito vazio ou um item incorreto abre a
mensagem original do banco 2 (`0x19C`, `0x19F`, `0x1E4`, `0x1E5`).

Retirar debita uma unidade e inicia a animação original 11+direção. Devolver ou
colocar usa 23+direção e som 7. A mão só é limpa ao terminar a animação.
O estoque devolvido é limitado a 999. Como no original, colocar novamente em
um cocho já cheio também consome a unidade carregada.

Os 22 slots do celeiro e dez do galinheiro são limitados explicitamente. Cada
edifício inicial expõe somente suas zonas originais. Os sprites dos cochos
usam o pack `farm_item_toc_7`, índices 4/7, nas posições dos registros de
`farm_tables.c`, extraídas durante o configure. Participam da mesma ordenação
gráfica do cenário. Os cochos permanecem cheios nas trocas de área e são
esvaziados na atualização de dia, inclusive durante o sono.

## Validação

`native_farm_feed` testa estoques, direções, consumo ao fim da ação, capacidade,
limites dos índices e limpeza diária com dados sintéticos. O percurso opcional
usa as zonas, mensagens, animações e texturas extraídas:

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_farm_feed_playthrough data
```

O percurso coloca estoque apenas na fixture do teste; o executável normal e os
atalhos de região não concedem recursos.

## Limites

A compra de forragem e ração já abastece os estoques pelo fluxo normal das
duas lojas, conforme [FEED_SHOPS.md](FEED_SHOPS.md). O corte de grama cultivada
e outras fontes ainda estão pendentes. Galinhas adultas já consomem uma ração
por dia e produzem ovos conforme a alimentação anterior, antes da limpeza dos
cochos: [POULTRY.md](POULTRY.md). Outros animais, reprodução, comércio completo
e eventos de saúde continuam pendentes. Esvaziar os cochos do celeiro não simula
vacas ou ovelhas. O bootstrap não executa o jogo e os testes host não substituem
a validação SHA-1 do build PS1.
