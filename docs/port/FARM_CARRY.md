# Coleta e descarte do campo

`farm_carry.c` liga os objetos soltos da fazenda ao mesmo inventário da mochila.
Z/Cross recolhe o objeto à frente: pedra pequena, galho ou mato. O terreno volta
à sua camada de chão e a colisão desaparece, sem recriar o objeto ao trocar de área.
Objetos grandes e plantas imaturas não são coletados por esse caminho.

2/R2 guarda o objeto da mão se houver espaço; com a mão vazia retira o primeiro
item da mochila. V/Triângulo permite mover o item entre os slots e a mão.
A mochila inicial comporta dois itens. Se estiver cheia, R2 conserva o objeto
na mão. Para descartar: retirar da mochila com R2 ou pelo menu, fechar o menu
e pressionar Z/Cross segurando o objeto na fazenda.

A coleta usa ANI 11+direção, carregar parado usa 27, caminhar usa 15+direção,
correr usa 19+direção e lançar usa 23+direção, de `people/player.c`.
A ação bloqueia novos comandos até terminar a animação. O lançamento só remove
o item da mão ao concluir a animação, com som 9. A coleta usa som 7.
Três posições de lançamento, velocidade horizontal de 44 unidades, altura inicial
-300, gravidade 9 e queda máxima 45 seguem o caminho básico do original.
O objeto lançado fica visível durante o voo. Pedra, galho e madeira retornam a
uma célula livre do campo; mato é descartado ao atingir o chão.
Pausa/mochila congelam o voo; não podem abrir durante a ação de coleta/lançamento.
Ferramentas, sementes e objetos especiais não podem ser descartados por este caminho.

O voo agora consulta a altura real do cenário e entrega produtos à caixa,
conforme [SHIPPING.md](SHIPPING.md). O porte continua parcial: reações a
água, obstáculos durante o voo, puff de aterrissagem, colocação de objetos de
instalação, colheitas e animais ainda precisam dos caminhos originais
correspondentes. Descartar em chão comum não registra venda.
O desenho de objetos carregados ainda usa projeção sobre o quadro do cenário;
a oclusão completa com os modelos precisa de integração adicional.

Validação: 24 CTests, incluindo coleta/limpeza, ida e volta pela mochila,
lançamento temporizado, aposentadoria do voo e proteção de objetos grandes e
regador. Probe com dados importados coleta mato real, guarda, abre a mochila,
retira e lança, renderizando as ANIs e imagens originais. Logs/capturas locais
em `build/native-debug/farm-carry-probe.log` e `farm-carry-*.bmp`.

## Correções posteriores

O objeto carregado agora usa `mark_screen_offsets` original: (-2,-22)/(0,-24)
conforme a direção e (-1,-31) ao correr, em vez do deslocamento de apresentação
de ferramenta. Pedras pequenas, galhos e madeira com bit de classe 0x10 voltam
a ocupar uma célula livre de solo/solo arado/molhado ao aterrissar no campo;
a camada de chão é preservada. Células ocupadas e fora do campo não recebem o
objeto. Mato continua descartado. As colisões consultam o mesmo terreno mutável.
A altura de aterrissagem agora usa o cenário; as reações especiais ainda
têm os limites acima.
