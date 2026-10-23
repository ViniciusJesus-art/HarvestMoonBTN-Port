# Relógio, calendário e ciclo de dia/noite

O recorte nativo adapta `world/clock.c`, `world/weather.c`, os estados de
`world/clock_events.c` necessários para o despertar e as regras de sono de
`people/player.c`. As tabelas de iluminação, lugares e faixas sonoras são
extraídas do C original durante a configuração. Não há execução de MIPS.

## Regras de tempo

- `one_minute = 30`; `clock_advance_minute` usa metade: 15 ticks de jogo por
  minuto, cerca de meio segundo. O acumulador avança uma vez por tick.
- O relógio para em lugares marcados com `0x1000` em `place_entries` (incluindo
  a casa), menus, eventos, fades, portas e transições. A fração de minuto é
  preservada durante a pausa.
- Meia-noite muda a hora para 00:00. A **data muda às 06:00**, com 30 dias por
  estação, quatro estações, semana de sete dias e ano limitado a 98 no estado
  interno (99 na interface).
- O estado inicial segue `clock_reset`: ano 0, primavera, dia interno 1,
  segunda-feira, 06:00. A interface mostra dia/ano acrescentando um.
- Hora/minuto, estação e data alimentam o quadro, calendário e programação da
  TV. O quadro exibe dezenas de minutos, como o original, e muda a intensidade
  dos dois-pontos ao cruzar cinco minutos.
- O passeio de Thomas avança 120 minutos usando esse mesmo relógio. A visita,
  os diálogos e o trajeto permanecem com o tempo pausado.

## Luz e áudio

`clock_time_period` usa os intervalos originais: amanhecer 05:00–06:59,
dia 07:00–14:59, entardecer 15:00–16:59 e noite no restante. A luz usa os
três canais 20.12 de `drift_by_hour`, divisão inteira e transição de 256 ticks.
O efeito corresponde ao quad de `screen_fill_quad`, com blend GPU 2:
subtração saturada em canais de cinco bits. A composição ocorre no mundo,
antes das caixas e do relógio. Flags de interior `0x800/0x1000` suspendem o
efeito, conforme `weather_fx_frame`; menus também congelam a interpolação.

O áudio usa as SEQs/VABs reais: faixas sazonais 1..4 e faixa noturna 28.
A proporção casa/exterior continua 80/100; à noite fica pela metade, com
música silenciada no inverno, conforme `farm_set_message_timing`. Os bancos
de efeitos são recarregados ao trocar música para conservar as ferramentas.

Fazenda e casa carregam suas seções sazonais de `farm_section_table.h`.
Uploads de textura preservam os bytes fora de seus retângulos, como a VRAM
original; apagar o atlas entre seções perdia pixels de preenchimento usados
pelos modelos de verão. Os pacotes de estação são aplicados separadamente.
O solo no inverno usa o look 29/página 7 de `field_cell_retexture`.

## Dormir e manhã seguinte

As duas entradas de dormir do menu da cama executam a ANI/coberta e o fade
existentes, avançam o relógio e devolvem o controle na entrada da casa.
Permanecer acordado até 06:00 também inicia a troca de dia e o retorno à casa.

O cálculo de `clock_sleep_until_5am` inclui o intervalo de `60 - minuto`
até a hora seguinte: com fadiga abaixo de 50, o despertar é às 06:00. Fadiga 50..69,
70..79 e 80..99 acrescenta respectivamente 120, 240 e 360 minutos.
A recuperação de stamina/fadiga segue as fórmulas de `player_morning_reset`.
As penalidades horárias de 00:00/02:00/04:00/06:00 também estão ligadas.

Inventário, experiência, terreno e contagem de descartes são conservados.
O solo regado seca segundo `field_tile_dry`; no inverno o mato é removido.
O histórico de contas é deslocado às 05h59 do último dia da estação.
O pagamento da caixa às 17h58 e suas exceções de data também são processados
durante o sono; veja [SHIPPING.md](SHIPPING.md). O retorno limpa animações,
voos temporários e bloqueios de entrada; a visita inicial não se repete.

O porte continua parcial: clima, crescimento de cultivos, animais, festivais,
rotinas de moradores, colapso e demais callbacks diários ainda precisam ser
adaptados. Essas rotinas não são substituídas por callbacks que aparentem
realizá-las. O pedido de dormir e salvar avança a sessão e informa no terminal
que nenhum save foi escrito; persistência ainda está pendente.

## Validação

Os 26 CTests incluem relógio/calendário, pausas, sono, rollover, luz de cinco
bits, altura de atores em um plano inclinado e secagem de solo/mato de inverno.
Os drivers opcionais usam somente os recursos locais do usuário:

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_first_morning_playthrough data
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_world_clock_playthrough data
```

O primeiro verifica a aproximação caminhando e alturas intermediárias na
escada para jogador e Thomas. O segundo verifica pausas reais, iluminação,
faixa noturna, renderização das quatro estações, sono com virada de ano e
manhã automática às 06:00, preservando itens/terreno. O terceiro argumento
opcional é uma pasta existente para capturas BMP. `HMBTN_TEST_GAME_DATA`
registra ambos no CTest quando configurado com a pasta extraída.

Evidências locais: `build/native-debug/clock-*.log` e `tour-captures/`.
Os testes host não substituem a validação SHA-1 do build PS1. O bootstrap
`--diagnostics` não executa o jogo. Windows real e comparação reproduzível
de imagem/tempo com PS1 continuam pendentes.
