# Medicamento animal e tratamento das galinhas

O recorte permite comprar o medicamento original `ITEM_ANIMAL_MEDICINE`
(`0x1E`), equipá-lo e curar uma galinha adulta doente na área atual. Carteira,
despesas, ferramentas, saúde e produção pertencem ao mesmo estado da sessão.
Não há remédio ou dinheiro concedido pelos atalhos de região.

## Compra e inventário

`game/feed_shop.c` adapta o ramo de medicamento de `shop_poultry_step` e
`shop_barn_step`, em `src/overlay/11_GF_FARM/menu/shop_dialogues.c`. Lillia e
Barley vendem uma dose por 1.000G, preço lido da tabela original. Mantêm-se
presença, alcance, mãos vazias, dias e horários do atendimento já documentados
em [FEED_SHOPS.md](FEED_SHOPS.md).

A opção aparece somente quando não existe medicamento na ferramenta equipada,
nas vagas acessíveis da mochila de ferramentas ou nas 64 vagas do baú. Isso
adapta `milestone_probe_animal_medicine` e sua consulta aos estoques de ferramentas.
O medicamento vai para a ferramenta equipada se ela estiver vazia; caso
contrário, para a primeira vaga livre da mochila de ferramentas, respeitando
as capacidades 2/4/8. Não é uma ração nem ocupa a mão de produtos.

Triângulo abre a descrição original (`0x48` em Lillia, `0x64` em Barley).
Cross compra diretamente uma dose, conforme o original. A loja recusa saldo
insuficiente com `0x4F` e mochila cheia com `0x54`, conservando os estados.
A confirmação bem-sucedida usa `0x51` ou `0x84`; debita a carteira, registra a
despesa e oculta a opção. O fechamento bloqueia compras repetidas. Guardar o
remédio no baú não permite comprar outro; consumir a última dose libera a opção.

## Uso e cura

R1 percorre as ferramentas e equipa o medicamento. C/Square inicia o modo
original `PLAYER_MODE_USE_MEDICINE` (`0x51`), usando o banco 22 de
`player_anim_banks`, carregado de `00_gf.bin` com leitura limitada e offsets
validados. As quatro direções usam as animações originais desse banco.

O alvo é fixado na área atual antes da animação: sonda a 128 unidades à frente, contra os
limites exclusivos de 200 unidades em X/Z da galinha. Usa a ordem dos registros
do galinheiro; incubação, mortos e nomes ainda pendentes não recebem o tratamento.
Trocar de posição ou não ter alvo não procura outra galinha ao terminar.

Ao concluir, `game/poultry.c` adapta `farm_chicken_clear_sick`: zera o contador
de doença somente da adulta escolhida. Não aumenta afeição nem revive mortos.
Com afeição de pelo menos 10, mostra a cue original 37 durante 45 ticks. O
renderizador usa os frames reversos de `cue_draw_frame`, com avanço a cada
dez unidades do contador; menus pausam a cue e a virada do dia a limpa.
O sprite volta ao ciclo normal de caminhada/repouso.

Uma dose é consumida ao fim, inclusive quando não há alvo ou o animal está
saudável, como `stored_item_release`. Galinhas transportadas para outra área
também podem ser tratadas; registros de outra área não são selecionados.
O som original é o efeito 17;
não há custo de stamina, mas se aplica a fadiga por clima, horário e stamina
zerada. Mãos ocupadas e fadiga 100 impedem iniciar o uso.

Durante o tratamento, movimento, troca de ferramentas, coleta e abertura de
menus ficam bloqueados. Os demais animais mantêm seu ciclo; o relógio obedece
às regras do cenário. Ao concluir, o banco de ação é desativado e o controle
normal retorna. Alimentação e produção continuam usando o estado da galinha curada.

## Validação e reprodução

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_poultry_medicine_playthrough data
```

São 33 CTests sem recursos proprietários e treze percursos opcionais com dados
locais. Os testes de loja cobrem as duas lojas, descrição, saldo, lotação,
baú/mochila/equipamento, crédito da ferramenta e débito único. Os testes de
galinhas verificam quatro direções, alvo fixado, consumo de dose, uso sem alvo,
bloqueios e fadiga.

O novo percurso começa com 500G e uma **fixture declarada de galinha**. Coleta
e vende 12 brotos em quatro dias, chegando a 1.100G. Move uma ferramenta inicial
do baú para o equipamento, compra o remédio para a mochila, compra duas rações
e volta ao galinheiro. Uma **fixture de doença** evita esperar o sorteio de
adoecimento. Equipa pelo atalho R1, trata com a animação real, verifica os
bloqueios e acompanha duas noites de alimentação e produção de ovo. Termina
com 80G, 600G de receita e 1.020G de despesas; o teste não concede dinheiro,
medicamento ou ração. Posições/horários são abreviados pelo driver de teste.

Um terceiro argumento aceita pasta de capturas BMP fora do Git. Evidências
locais: `build/native-debug/poultry-medicine/` e logs em `build/native-debug/`.
As capturas de loja, tratamento e cura foram inspecionadas usando SDL
dummy/software. Não houve nova execução gráfica real, comparação PS1 ou
build Windows nesta etapa; o código continua C11/SDL3 compartilhado.

## Limites

O transporte das galinhas já está integrado; veja
[POULTRY_TRANSPORT.md](POULTRY_TRANSPORT.md). Vacas/ovelhas e seus tratamentos,
atendimento veterinário, clima completo, concursos, melhorias e saves
continuam pendentes. A aleatoriedade dos animais continua host. O bootstrap
`--diagnostics` não executa o jogo; os testes host não substituem o build PS1
nem a validação SHA-1 original.
