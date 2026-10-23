# Primeiro controle na fazenda

Atualização: pausa/estado da fazenda, mochila, troca rápida, efeitos de quebra
e liberação do áudio do regador estão integrados. Veja [FARM_MENUS.md](FARM_MENUS.md).

A saída da casa inicial agora carrega a área 0 e libera caminhada/corrida no
exterior. Usa a zona original da casa e sua entrada `(-4464,0,4032)`, mantém
inventário/opções da sessão e conserva o tema de primavera já carregado.
Na primeira saída, o evento 12 assume o controle: passeio opcional de Thomas,
apresentação de Ellen e conversa/tutorial de Zack. O controle retorna no fim
do evento; detalhes em [FIRST_MORNING.md](FIRST_MORNING.md).

O carregamento ocorre durante uma transição escura, com entrada bloqueada.
O fade atual tem 10 ticks em cada direção; ainda não reproduz o blend,
autowalk e eventos de porta completos de `farm_event_step`. A apresentação
não deve ser considerada idêntica ao PS1.

Setas caminham, X/Circle corre. O chão e os bloqueios estáticos usam os
polígonos do terreno e as consultas de `FarmChoice`. Os 1125 terrenos agora
mantêm estado próprio, associado aos quads originais de flag 8. Pedras, galhos,
mato, madeira e as quatro partes de pedras grandes/tocos usam as regras de
`farm_probe.c` e `farm_tables.c`, incluindo bloqueio e salto ao correr quando
o obstáculo permite e o terreno seguinte está livre. Não há retângulos de
colisão desenhados à mão para esses objetos. Os menus da casa não são disparados nas zonas externas.
O relógio avança no exterior e para em casa, menus e eventos; aceitar o passeio
acrescenta duas horas. Há calendário, iluminação de dia/noite e manhã seguinte
após dormir; detalhes e limites dos sistemas diários em [CLOCK.md](CLOCK.md).

A porta externa usa flags `0x9000` e o evento 0, dependendo de interação e
roteiro próprios. Z/Cross, dentro da zona e olhando para a esquerda, abre a
porta e retorna à casa. Durante os 16 ticks de abertura, posição e animação
permanecem paradas, inclusive com a direção pressionada. A caminhada automática
só começa no tick seguinte ao giro completo. O giro usa aceleração 8 e limite
-0x420; o som 0x16 usa o banco da fazenda, programa 1, tom 12, nota 60, volume 127.
A caminhada automática está adaptada para 10 ticks, e o fechamento/hold de
11 ticks e o blend completo ainda precisam de paridade. As passagens externas
agora conectam as regiões do mapa; veja [WORLD_MAP.md](WORLD_MAP.md).
A caixa da fazenda registra vendas conforme [SHIPPING.md](SHIPPING.md).
Interiores das instalações, rotinas de moradores e demais eventos continuam pendentes.

Validação Linux: configure/build e 21 CTests passaram. Probe com recursos
locais confirmou a troca para área 0, posição original, liberação da entrada,
caminhada externa, ida/volta/ida com conservação de uma alteração no baú e leitura da placa 0x20B. Evidências em
`build/native-debug/outdoor-probe.log` e `outdoor-walking.bmp`.

As cinco placas externas de interação 16 e a estante da casa leem o banco 2,
`A_FILE/04_messege2.bin`. O banco 3 é usado pela TV e pelos outros menus da casa;
usar o mesmo índice nesse banco era a causa das mensagens erradas nas placas.
Direção e alcance seguem as zonas do cenário, incluindo direção livre (5).
O nome do cachorro vem do cadastro. Os inserts `0x4012..0x4015` leem os quatro
estoques da sessão: forragem, madeira, ração de galinha e comida de peixe.
Começam zerados; cortar galhos com o machado já acrescenta madeira.
Obtenção e consumo dos demais suprimentos continuam pendentes.
A expansão preserva os bytes de imagem e limita o texto a 1024 palavras LE16.

O volume da música segue o flag `0x1000` do cabeçalho da área: 80 dentro da
casa, 100 na fazenda, conforme `farm_set_message_timing`. A mudança é aplicada
no tick da troca de área, sem reiniciar o tema ou alterar o volume dos efeitos.
A subida inicial continua durando 100 ticks. Regras de minas, clima e período
noturno ainda dependem das próximas áreas e da passagem do tempo.

Validação das correções: 21 CTests Linux e compilação MinGW Windows x64.
Probe local conferiu estante, todas as cinco placas, nome cadastrado e estoques
não nulos, conservação da sessão, posição/animação durante a abertura e volumes
80/100 nas duas direções. Evidências em
`build/native-debug/farm-corrections-probe.log` e `farm-sign-*.bmp`.
O teste de execução em Windows real continua pendente.

## Ferramentas e terreno mutável

Equipe uma das cinco ferramentas iniciais pelo baú da casa. No exterior,
**C/Square** prepara a ferramenta; soltar executa a animação original.
O efeito só é aplicado após terminar a animação, com som do VAB da fazenda:

- Foice: corta o mato do terreno à frente.
- Enxada: ara um terreno permitido pela regra original; respeita obstáculos.
- Machado: corta um galho e acrescenta uma madeira ao estoque da sessão.
- Martelo: quebra pedras pequenas e desfaz o solo arado/regado.
- Regador: detecta água pelo polígono original à frente; carrega o banco de
  reabastecimento e repõe 100 unidades. Fora da água, rega terra arada e
  consome água. Vazio não executa uma rega fictícia. Sua descrição no baú
  mostra a contagem real, inclusive depois de reabastecer e voltar à casa.

Pedras grandes, rochas e tocos recusam ferramentas iniciais conforme o poder
exigido. O núcleo já representa as quatro partes e seus requisitos, mas o
caminho de entrada aceita apenas as cinco ferramentas básicas; upgrades,
alcances ampliados e suas cores ainda não estão integrados.
Segurar por mais de 60 ticks após a carga máxima executa o banco de ação
original de sobrecarga. Stamina, fadiga e experiência das ferramentas são
mantidas na sessão; reações completas à exaustão e recuperação dependem do
restante do sistema de condições/pernoite.

Limpar, arar e regar alteram o estado e os UVs/CLUTs dos quads originais.
As mudanças são conservadas ao sair/voltar; não há gravação em disco ainda.
O gerador `port/cmake/generate_farm_tables.py` cria visões numéricas das
tabelas C existentes, sem SDK, ponteiros PS1 ou recursos importados no Git.

## Diálogos, volume e validação atual

O fim de uma leitura não reinicia mais a animação de fechamento. O controlador
retorna ao movimento quando o texto termina de fechar, eliminando o segundo
ciclo que fazia a caixa piscar. O teste verifica cada tick do fechamento e
a ausência da moldura depois dele.

Os 22 CTests incluem colisões/regras, limites da grade, requisitos das
ferramentas, peças 2x2, estoque, salto e efeito posterior à animação. O teste
de áudio compara PCM da mesma sequência nas mesmas posições: amplitude
interna de 80% da externa, dentro do erro de quantização S16. Valores SEQ
originais são 80/100; o ganho dos efeitos permanece independente.

O probe com recursos locais vinculou os **1125/1125 quads**, executou as cinco
ferramentas, reabastecimento, sobrecarga, placas, porta e retorno à fazenda
com conservação dos terrenos. Capturas `farm-tool-*.bmp` e log
`build/native-debug/farm-corrections-probe.log`. O porte ainda é parcial:
carregar/lançar/vender itens, partículas de quebra, plantio/crescimento,
instalações, animais/NPCs, relógio e eventos diários impedem considerar a
fazenda completa ou afirmar paridade 1:1 nesta etapa. Os efeitos de quebra,
citados nessa validação anterior, já foram integrados na etapa de menus.
