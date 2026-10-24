# Colisão e conversas comuns dos moradores

O controle livre consulta os moradores visíveis da área antes de aceitar o
passo e antes de deslizar junto a um obstáculo durante a caminhada. Na corrida
(modo original `PLAYER_MODE_POSE_4`), jogador e moradores podem se atravessar.
O teste de chão, paredes, móveis e campo continua ativo na corrida. A caixa segue
`farm_villager_find_box`: limites estritos de 200 em X/Z e 20 em Y.
Um morador em rota também para diante da caixa de 218 em X/Z do jogador
quando ele não está correndo, conforme `people/villager.c`.
Posições que já se sobrepõem na chegada podem se afastar; moradores ausentes
não bloqueiam. A retomada da agenda ao carregar uma área continua independente
dessa colisão. Galinhas mantêm a regra própria descrita em `POULTRY_TRANSPORT.md`.

Z/Cross consulta o ponto 128 à frente e inicia a conversa do morador encontrado,
incluindo o ramo de presente quando há um produto na mão. Barley/Lillia têm
prioridade para atendimento com as mãos vazias nos horários das lojas;
fora do expediente usam o diálogo comum. A conversa suspende
movimento, menus, rotas e relógio, vira o morador para o jogador e devolve sua
direção ao terminar. Cima/baixo selecionam respostas quando o texto as oferece.

`port/game/village_talk.c` executa o bytecode dos diálogos comuns em
`12_event_s.bin`. O leitor valida o bloco, copia somente o script marcado,
limita a 3.296 bytes e usa o interpretador nativo já existente, sem executar MIPS.
Os offsets vêm da tabela C original durante o configure. A faixa usual é
350 + id; Won usa 400. Os atores de evento 27/33 e Gourmet não recebem uma
conversa inventada a partir dessa fórmula.

As mensagens solicitadas pelo script são carregadas dos bancos 0..4
(`02_messege0.bin` até `06_messege4.bin`), com os glifos, controles e retratos
extraídos. Não há reescrita da fala de saldo insuficiente nem outra frase
substituta: a edição/tradução instalada fornece o texto. Nomes de jogador/fazenda
são expandidos. As comparações mantêm a
convenção original de retornar 1 quando a regra falha. O avaliador considera
horário, local/destino, calendário, amizade/afeição, flags de encontro e
aniversários originais, incluindo a mudança das cinco candidatas quando seu
aniversário coincide com o do jogador. A flag de apresentação persiste na
sessão; as flags diárias são consumidas pela atualização de amizade/afeição
na manhã seguinte, conforme [VILLAGER_AFFINITY.md](VILLAGER_AFFINITY.md).

## Validação local

`native_village` verifica as bordas da caixa, altura, ausência, afastamento de
uma sobreposição e parada/retomada de uma rota diante do jogador.
`native_house_control` verifica bloqueio ao caminhar, travessia do NPC ao
correr e preservação dos limites do chão/móveis. `native_village` também verifica
que a rota deixa de bloquear ao correr e volta a bloquear ao caminhar.
`native_village_talk` verifica suspensão da mensagem, respostas, primeira
conversa/repetição, restauração ao atingir comando ausente e limites de retorno
indireto, sem exigir recursos do jogo.

O percurso opcional `hmbtn_village_talk_playthrough` lê os scripts reais,
executa 93 solicitações de mensagem em cenários de primeira conversa,
repetição e afinidade, compara os glifos/controles com o `.bin`, carrega os
retratos e confere o retorno. Também verifica
que Won para no comando de loja ainda ausente, sem registrar progresso.
Uma conversa com Lillia na área real valida a pausa de controle/relógio/menus,
fechamento, captura de tela, repetição e ganho único de amizade após dormir.
Os estados de afinidade e introdução da vara de
Greg nesse teste são fixtures, não atalhos disponíveis ao jogador.

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_village_talk_playthrough data
# Lillia depois do expediente: aproxime-se e aperte Z
./build/native-debug/port/hmbtn_native --data data --area-preview 128 \
  --preview-date 0:3 --preview-time 16:00
```

A resolução estável dos recursos e a preparação para futuros PNGs extraídos
estão em [RESOURCE_IDENTITIES.md](RESOURCE_IDENTITIES.md).

## Limites de paridade

O recorte cobre conversas comuns e presentes. A entrega da vara
por Greg e o armazenamento/apresentação de ferramentas estão descritos em
[GREG_ROD.md](GREG_ROD.md). Presentes e aniversários estão em
[VILLAGER_GIFTS.md](VILLAGER_GIFTS.md). Lojas adicionais (incluindo Won), cenas
familiares, casamentos e outros comandos de história continuam pendentes. Ao chegar a um comando não
adaptado, o interpretador informa NPC/comando/passo no terminal, devolve o
controle e restaura flags/afinidade de todos os moradores e os itens da
conversa; não registra a ação como feita.
Os sistemas de família/clima/construção ausentes conservam o estado inicial;
os contadores de história agora têm estado e avanço diário próprios, conforme
[EVENT_DAYS.md](EVENT_DAYS.md). A existência do contador não implementa o evento
que o usa. Não há paridade completa das falas de história.

Rotas bloqueadas pelo jogador acumulam atraso em minutos do jogo e são
liberadas aos 30 minutos; a agenda/retomada leva esse atraso em conta. Os
resets de manhã, ao meio-dia e às 18h também foram adaptados. Veja
[EVENT_DAYS.md](EVENT_DAYS.md). Bloqueios entre dois moradores, cooldown de
cumprimento, ação após conversa, animações especiais e temporização completa
de eventos continuam no roadmap.

Validação em Linux com SDL dummy/software; sem comparação visual em PS1 e sem
execução em Windows real. Recursos e capturas permanecem fora do Git. O porte
continua parcial; `--diagnostics` executa apenas o bootstrap e não o jogo.
