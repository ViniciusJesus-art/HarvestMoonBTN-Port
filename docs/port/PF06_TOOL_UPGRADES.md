# PF-06 — Melhorias das ferramentas de Saibara

Recorte de 2026-10-29 da fila do [ROADMAP.md](ROADMAP.md). Adapta o serviço
original e os efeitos das cinco ferramentas: foice, enxada, machado, martelo e
regador. Nenhum código de `src/`, SDK ou recurso proprietário foi modificado.

## Regras e evidência original

`shop_stock_step` mantém **Improve Tool como instrução** (texto 2:0x87).
A encomenda começa ao conversar com Saibara **segurando minério e com a
ferramenta equipada**, pelo script instalado **354**, comando **81/8/4**.
Não foi criado um catálogo substituto que venda níveis sem esse percurso.

`shop_tool_upgrade_state` e `shop_greenhouse_step`, em
`menu/shop_dialogues.c`, validam ferramenta, nível máximo, minério inferior,
nível repetido e experiência, nessa ordem. São permitidos saltos de nível.
A experiência é por espécie de ferramenta e permanece no dono `farm_action`.

| Nível | Minério na mão | XP mínimo | Preço |
|---|---|---|---|
| Cobre | 0x7c | 6.000 | 1.000 G |
| Prata | 0x7b | 18.000 | 2.000 G |
| Ouro | 0x7a | 36.000 | 3.000 G |
| Mystrile | 0x60 | 65.535 | 5.000 G |

As recusas usam **2:0x96/0x8d/0x97/0x98/0x99**. A primeira confirmação usa
**2:0x9a..0x9d**, conforme o minério; a segunda usa **2:0x9e**. O saldo é
conferido ao aceitar; saldo insuficiente usa **2:0x4f**, No/Circle usa
**2:0x93** e a recusa termina com a instrução **2:0x87**.
A aceitação final debita uma vez, recolhe os registros da ferramenta equipada e
do minério na mão, e usa **2:0x9f**. Inventário e experiência restantes não mudam.
O retorno usa a transição comum para área **32**, **(1000,0,-2800)**, direção 3,
e refaz a agenda de Saibara. As animações locais de portas foram preservadas.

`milestone_set_timer` começa ferramentas em **2**, e `milestone_tick` só limpa
a flag **0x37** na chamada seguinte à que alcança zero: **três manhãs**.
Ferramentas e máquinas usam a mesma encomenda pendente; não há duas oficinas
paralelas. A retirada ocorre ao conversar com Saibara depois do prazo.
O script 354 consulta/lê o pedido, verifica vaga, armazena a ferramenta e então
aplica/limpa o pedido. Se equipamento e mochila estiverem cheios, o pedido
permanece aguardando retirada. Uma máquina já instalada conserva sua posse.

A ferramenta entregue usa o ID original **1 + 5×espécie + nível**,
`modifier=nível` e **count=1**, como `event_item_store/stored_item_set` no C/BIN.
Isso inclui o regador: a entrega não preserva nem enche seu reservatório antigo.
O reabastecimento real continua dando **100** unidades (`WATERING_CAN_FULL`).

Evidência binária local, além da leitura do C:

- `greenhouse_level_value` **0x8011debc**, `greenhouse_kind_index`
  **0x8011df04**; tabelas de XP **0x801282e4** e preço **0x801282f0** no
  overlay instalado `A_FILE/11_GF_FARM.bin`.
- `milestone_set_timer` **0x80111e68**, `milestone_tick` **0x80111f88**;
  o mesmo protocolo de pedido/flag usado pelas máquinas.
- `tool_charge_steps_a/b/c` **0x8012319c/0x801231a8/0x801231b4** e
  `tool_stamina_costs` **0x801231c0**. Valores conferidos nos bytes little-endian.
- As 25 grades foram confrontadas com `disc/SLUS_011.15`, a partir do offset
  **0x3a31c**, registros de 176 bytes. `picture_grid_find_cell` e
  `grid13_copy_oriented` determinam a ordem e orientação dos alvos.
- `event_item_store` e `shop_greenhouse_step` confirmam a substituição por um
  registro novo, sem copiar quantidade/modificador antigos para a nova versão.

O overlay instalado tem SHA-1 `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`;
a referência PS1 é `664eee540550138d6e3717bb7e1593aeb8fe9d38`. Não se afirma
igualdade com a referência. O build host não substitui a validação PS1.

## Ações reais

`generate_farm_tables.py` extrai grades, XP, preços, contagens e custos do C.
`tool_upgrade.c` compartilha essas regras com a loja e com `farm_action`.
Não há tabelas de alcance desenhadas novamente como retângulos aproximados.

Soltar Square usa a ANI correspondente à **carga efetivamente alcançada**,
mesmo que a ferramenta seja de nível maior. Carga máxima leva 20 ticks por
nível; o excesso de 61 ticks mantém a reação existente sem aplicar o golpe.
Som, custo de energia e XP são efetivados uma vez por ação.

| Ferramenta | Alvos por carga 0/1/2/3/4 |
|---|---|
| Foice | 1 / 3 / 6 / 9 / 25, conforme a grade original |
| Enxada | 1 / 2 / 3 / 4 / 6 em linha; um bloqueio válido interrompe os seguintes |
| Regador | 1 / 3 / 6 / 9 / 15; consumo conforme carga, limitado pela água disponível |
| Machado/martelo | Uma posição; força acumulada de 1 / 2 / 4 / 8 / 16 por golpe |

A potência mínima dos obstáculos continua sendo validada pelo nível possuído;
uma ferramenta básica não passa a quebrar pedras grandes por insistência.
Machado/martelo conservam regras de quatro células, madeira e efeitos originais.
Foice usa os mesmos cortes/efeitos e estoque de forragem, inclusive 25 cortes.
Os resultados em área avançam em ordem, um alvo por tick, reutilizando o pool
existente de 25 efeitos e a atualização de texturas.

A água é descontada pela quantidade pedida pela carga, mesmo em alvos que não
aceitam água; estoque insuficiente limita os primeiros alvos. Um golpe gera
**150 XP** se houve resultado ou **40 XP** caso contrário, limitado a 65.535;
não multiplica XP por célula. Energia: 1..5 conforme a carga.
A enxada continua escavando **uma célula nas minas**, inclusive melhorada.
A fazenda bloqueia aração no inverno; a estufa usa sua própria grade e permite
aração no inverno. Limites/rotação e as demais regras continuam compartilhados.

## Estado, validação e limites

A fila existente `HmbtnMakers` também admite IDs de ferramentas; `village`
deriva a flag de trabalho da mesma fila. A loja toma emprestada a experiência
viva. Novos campos de resultado são transitórios na ação. O codec estável
continua **v15**: apenas o intervalo válido da encomenda foi ampliado.
Snapshots durante golpe/resultado, mensagem ou transição continuam bloqueados.
Não foi criado trabalho separado de compatibilidade de saves.

Linux Debug: seleções CTest **6/6**, **10/10** e **1/1** aprovadas,
**15 casos distintos** no conjunto. A última inclui o percurso original e duas
retomadas em aplicações novas. Após o ajuste de uso fora de campo, unidade e
percurso Debug foram novamente aprovados. Linux Release: build, unidade e
percurso aprovados, com unidade final repetida para esse ajuste. Windows MinGW:
build aprovado; Wine: unidade/percurso e retomadas pendente/concluída aprovados,
com a unidade final repetida. Mesa/EDID gerou avisos, mas os checks finais
encerraram com exit 0. Windows físico não foi validado.

A contagem conservadora é de **48 processos de testes**, incluindo CTest,
wrappers, filhos e tentativas (Wine contado como lançador + programa). A suíte
inteira não foi executada; limite de 50 respeitado. Contagem: 6/6 inicial = 7
processos; quatro execuções unitárias e dois percursos iniciais = 6; 10/10 = 15;
Release inicial = 2; wrapper próprio via CTest = 5; quatro checks Wine = 8;
unidade Debug/Release e percurso Debug finais = 3; unidade Wine final = 2.

Reprodução, selecionando apenas os checks pertinentes ao orçamento da rodada:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_tool_upgrade(_playthrough)?$'
```

A seleção consome **6 processos**: CTest, unidade, wrapper, percurso e duas
aplicações de retomada. Capturas opcionais, diretório previamente criado:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  build/native-debug/port/hmbtn_tool_upgrade_playthrough data /tmp/tools.hms /tmp/tool-shots
```

Logs em `/tmp/hmbtn-upgrades-{regression,final-debug,process-final}.log`,
`/tmp/hmbtn-upgrades-final-playthrough.log`,
`/tmp/hmbtn-upgrades-release-playthrough.log` e `/tmp/hmbtn-upgrades-wine-*.log`.
Capturas em `/tmp/hmbtn-upgrades-shots/` foram inspecionadas; não entram no Git.

`tool_upgrade_test.c` cobre os 20 pares espécie/nível, limiares, saltos,
cancelamentos, saldo, materiais/posse, metadados de outros registros, fila única,
três manhãs e retomada. Exercita as cinco ferramentas nos cinco níveis de carga
e quatro direções, XP/custo/estoque, água insuficiente, golpe sem carga com
ferramenta máxima, linha bloqueada, inverno/estufa, 25 cortes de pasto,
sobrecarga e uma célula de mina com enxada máxima.

`tool_upgrade_playthrough.c` escava cobre, prata, ouro e duas Mystrile, obtém XP
real da enxada na mina, executa cinco encomendas no script 354 e usa as cinco
ferramentas entregues com ANI/solo/estoque reais. Cancela na segunda confirmação,
verifica recusa de retirada com mochila cheia, aguarda três noites pelo fluxo
comum, verifica custos/consumo/entrega e retoma pedidos e ferramentas entregues.
Saldo, energia e XP das quatro outras ferramentas são fixtures; minério não é
concedido. Loads diretos encurtam deslocamentos não relacionados. Os terrenos
usados para os vetores de alcance são fixtures, vinculados ao piso original.

Melhorias de acessórios/presentes da forja, paletas de brilho da carga e todas
as vozes/cues permanecem fora deste recorte. Festivais e cozinha completa seguem
abertos. O bootstrap `--diagnostics` não executa o jogo; o runtime continua
parcial. Próximo recorte recomendado: aquisição original dos utensílios da
cozinha (programa de compras/pedido/entrega), usando a cozinha já existente.
