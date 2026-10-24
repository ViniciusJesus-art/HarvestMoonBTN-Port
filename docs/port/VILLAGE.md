# Interiores e rotinas iniciais do vilarejo

O recorte nativo carrega os 22 interiores de base 128..212, de quatro em quatro,
nas quatro estações. As portas usam a posição, direção e entrada serializadas;
as escadas e passagens entre cômodos também funcionam. O relógio permanece
pausado nos interiores conforme a tabela original. Inventário, terreno e
calendário pertencem à mesma sessão durante as trocas de área.

## Regras de entrada

`port/game/village.c` avalia as 31 tabelas de `event/event_rules.c`, extraídas
no configure CMake sem ligar código PS1. As condições incluem janela de horário,
presença do morador, amizade/afeição, dia da semana, datas especiais e flags de
eventos. A recusa mostra a mensagem original do banco 2 e conserva a posição.
Portas de fazenda sem regras continuam disponíveis durante o dia inteiro.

A presença vem das agendas originais: `event/cue_history.c` seleciona uma lista
no bloco `00_gf.bin:0x74800..0x7F000`. Cada morador tem 30 passos de oito bytes,
lidos em little-endian; os endereços continuam offsets de 32 bits. Não há
conversão de endereços PS1 em ponteiros host. A seleção considera o calendário
e as flags atualmente implementadas. Por exemplo, Barley ausente na segunda
impede a entrada no rancho, mesmo durante o horário habitual.

Os moradores no cenário recebem as rotas de `section+0x1000`: pontos, esperas,
saltos e avanço de agenda. A entrada em uma área retoma a caminhada pelo tempo
decorrido, em vez de reiniciá-la na porta. A altura consulta o chão real.
Sprites e texturas vêm dos packs originais: recursos do interior e os três
períodos de cada dia da semana, em atlas host separados. Ellen usa sua animação
sentada; Gotz usa a animação de trabalho na rota correspondente da fazenda.

## Atalhos e testes

```bash
# Rancho; sessão nova, sem abertura e sem visita de Thomas
./build/native-debug/port/hmbtn_native --data data --area-preview 136
# Ferraria
./build/native-debug/port/hmbtn_native --data data --area-preview 156
```

O atalho aceita todos os múltiplos de quatro entre 128 e 212. Começa na chegada
original de uma passagem. O atalho permite inspecionar um interior diretamente;
o acesso normal continua sujeito às regras. Não carrega saves nem altera
estoques ou afinidade.

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_village_playthrough data
```

O percurso opcional valida os 35 packs de moradores, uma porta fechada às 06h,
a mensagem de recusa e entrada com a agenda real de terça-feira, os atalhos,
os 88 pares interior/estação, renderização, pausa do relógio e saídas.
`native_village` usa fixtures sintéticas para verificar limites de afinidade,
horários, presença, datas especiais, retomada e avanço das rotas.

## Limites

A compra de forragem/ração está em [FEED_SHOPS.md](FEED_SHOPS.md).
Ainda faltam conversas livres, presentes, progressão de afinidade, demais lojas,
festivais, eventos familiares e o conjunto completo de scripts dos moradores.
As células de porta das rotas retomam no ponto seguinte; suas animações de
porta e cues de evento ainda não são executados. Bloqueios dinâmicos entre
jogador e moradores e a paridade de temporização também estão pendentes.
O clima continua ensolarado no recorte atual. Flags de sistemas ainda ausentes
conservam seu estado inicial; ter uma condição no avaliador não implementa o
evento que a modifica.

O jogo permanece um porte parcial. `--diagnostics` executa somente o bootstrap.
A validação local Linux não substitui o build PS1/SHA-1 nem comprova execução
em Windows real. Recursos extraídos e capturas continuam fora do Git.
