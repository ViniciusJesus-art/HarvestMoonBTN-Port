# Contadores de dias e atraso dos moradores

Os scripts de conversa agora consultam contadores da sessão em vez de respostas
fixas de início de jogo. A implementação adapta `scene_item_queue_reset`,
`scene_message_timers_tick` e as variáveis 40..42 de `event_commands.c`, mantendo
os tipos PS1 de oito bits. O bytecode e as mensagens continuam nos BINs originais.

## Estado e avanço diário

`port/game/event_days.*` mantém três grupos:

| Variável do script | Estado nativo | Comportamento |
|---|---|---|
| 40 | `count0[128]` | Contadores que avançam em cada mudança de dia |
| 41 | `count1[8]` | Contadores com condições de pausa |
| 42 | `values[128]` | Valores escritos pelos scripts, sem incremento diário |

Os valores iniciais de `count0` são extraídos da função original durante o
configure CMake. Entradas não iniciadas e todo `count1` começam em 255; `values`
começa zerado. O valor 255 significa parado. Os contadores ativos avançam uma
unidade por dia até 120; valores ativos maiores que 120 convergem para 120 na
atualização, como no laço original.

O grupo `count0` avança mesmo quando `count1` está pausado. O segundo grupo não
avança nas primeiras 14 datas de `clock_date_table`, quando há uma proposta de
casamento pendente ou quando o clima é 3/4. A consulta das datas mantém o índice
original de base 1; as outras quatro datas de remapeamento de cenas não entram
nessa condição. A tabela vem da fonte C original e é compartilhada com o relógio
nativo por uma interface de calendário, sem offsets de recursos novos.

`world_clock_events` recebe as notificações ordenadas do relógio. Uma mudança
de dia às 06h avança o estado uma única vez, inclusive durante sono ou avanço de
tempo de scripts. Abrir menus ou conversar conserva o relógio e os contadores.
Atalhos de prévia começam uma sessão nova com os valores iniciais; selecionar
uma data posterior não simula dias anteriores nem concede eventos ao jogador.

O interpretador de conversas lê e escreve esse estado com índices validados e
valores limitados a 0..255. Operações aritméticas seguem o despachante existente.
Se a conversa chega a um comando ainda ausente, os contadores são restaurados
junto com flags e afinidade. Nenhum evento é registrado como concluído por
ter apenas um contador disponível.

## Atraso em uma rota bloqueada

Um morador em movimento que para diante do jogador acumula atraso a cada minuto
do relógio do jogo, até 30. Frames com relógio parado não contam como minutos.
Ao atingir 30, a rota pode atravessar o jogador e a caixa do morador deixa de
bloquear caminhada; o NPC continua selecionável para conversa. A corrida
continua permitindo atravessar desde o início.

A agenda inclui o atraso nas próximas partidas. Recarregar a área desconta o
atraso do tempo de caminhada retomado, conforme `farm_item_history_resume`.
Quando uma célula de rota muda de local, o horário registrado desconta o atraso;
a entrada em local cujo relógio é pausado limpa o atraso, conforme a consulta
original de `place_entries`.

Os atrasos são zerados na manhã seguinte, ao meio-dia e às 18h, como nas rotinas
`villager_clear_delays`/`farm_hourly_refresh`. Moradores ausentes e a exceção de
corrida não acumulam atraso por bloqueio. O recorte cobre o bloqueio de uma
rota pelo jogador; o cooldown de cumprimento, a ação pós-conversa de 150 ticks,
bloqueios entre moradores e cues de eventos continuam pendentes.

## Verificação e limites

- `native_event_days`: defaults, sentinela 255, limite 120, pausas por data/clima/
  casamento e avanço ordenado por ano/dias, sem dados proprietários.
- `native_village_talk`: escrita/leitura por scripts, limite de byte, índice
  inválido e restauração ao chegar a comando ausente.
- `native_village`: 30 minutos em vez de frames, corrida, liberação da caixa,
  seleção do NPC, retomada de rota, agenda adiada e resets de horário/dia.
- `native_world_clock_playthrough`: sono real na virada do ano, pausa no primeiro
  dia da primavera e avanço ao permanecer acordado até 06h, conservando valores.
- O percurso de conversas continua comparando os glifos/controles originais em
  suas 93 solicitações, sem inventar falas.

Clima e casamento completos ainda não existem no porte. O chamador usa o clima
atual da sessão e informa ausência de proposta pendente; a condição de proposta
é testada isoladamente, sem implementar casamentos. Entrega da vara de Greg,
presentes, demais lojas e eventos especiais continuam pendentes. Não há saves
para persistir os contadores entre execuções.

Validação em Linux, SDL dummy/software. O bootstrap diagnóstico não executa o
jogo, e testes host não substituem execução Windows real ou o build PS1/SHA-1.
Nenhum leitor de recursos gráficos foi adicionado nesta etapa; decodificadores,
identidades de recursos e leitura dos BINs permanecem em uso.
