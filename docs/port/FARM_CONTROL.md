# Primeiro controle na fazenda

A saída da casa inicial agora carrega a área 0 e libera caminhada/corrida no
exterior. Usa a zona original da casa e sua entrada `(-4464,0,4032)`, mantém
inventário/opções da sessão e conserva o tema de primavera já carregado.

O carregamento ocorre durante uma transição escura, com entrada bloqueada.
O fade atual tem 10 ticks em cada direção; ainda não reproduz o blend,
autowalk e eventos de porta completos de `farm_event_step`. A apresentação
não deve ser considerada idêntica ao PS1.

Setas caminham, X/Circle corre. O chão e os bloqueios estáticos usam os
polígonos do terreno. O campo usa o layout inicial já integrado; mato, pedras
e troncos são desenhados, mas suas colisões e uso de ferramentas ainda não
participam deste controle. Os menus da casa não são disparados nas zonas externas.
O relógio permanece no estado inicial: avanço de tempo e eventos diários faltam.

A porta externa usa flags `0x9000` e o evento 0, dependendo de interação e
roteiro próprios. Z/Cross, dentro da zona e olhando para a esquerda, agora abre a porta e retorna à casa. O giro usa aceleração 8 e limite -0x420; o som 0x16 usa o banco da fazenda, programa 1, tom 12, nota 60, volume 127. A caminhada automática está adaptada para 10 ticks, e o fechamento/hold de 11 ticks e o blend completo ainda precisam de paridade. As saídas para
outras áreas param num limite explícito; essas áreas e seus NPCs/gameplay
não são simulados. Próximo recorte: controle e colisões do campo, uso de ferramentas e eventos externos.

Validação Linux: configure/build e 21 CTests passaram. Probe com recursos
locais confirmou a troca para área 0, posição original, liberação da entrada,
caminhada externa, ida/volta/ida com conservação de uma alteração no baú e leitura da placa 0x20B. Evidências em
`build/native-debug/outdoor-probe.log` e `outdoor-walking.bmp`.

As placas externas com id de interação 16 usam as mensagens originais e a mesma
janela de leitura. Direção e alcance seguem as zonas do cenário; outras
interações externas continuam pendentes.
