# PF-06 — Clima diário e chuva no campo

Recorte concluído em 2026-10-26 para o runtime nativo Linux/Windows. PF-06
continua aberta. Próxima entrega: sementes de Won, conforme [ROADMAP.md](ROADMAP.md).
O port reaproveita algoritmos C; não exige PS1, SDK, MIPS ou SHA-1 do executável.

## Origem e comportamento

Referência da decompilação: `d3c0f38`. Caminhos abaixo relativos a `src/`:

| Fonte | Adaptação e decisão |
|---|---|
| `overlay/11_GF_FARM/world/weather.c`: `weather_roll_tomorrow`, `weather_step_today`, `date_next_day` | `port/game/weather.c`: mesmas regras sazonais, datas, aniversários e redução de tempestade/nevasca antes de eventos. Datas nativas, sem copiar layout PS1. |
| `main/graphics/screen_fade.c`: `rand_range` | Reusa `port/core/psx_random.c`, com wrap explícito em 32 bits; stream meteorológico pertencente à sessão e persistido. |
| `overlay/11_GF_FARM/event/event_commands.c`: `item_places` | Gerador extrai a tabela de aniversários; `hmbtn_bride_birthday` é compartilhada com conversas, inclusive o aniversário alternativo quando coincide com o do jogador. |
| `overlay/11_GF_FARM/world/clock_events.c`: `scene_overnight_step` | Observer existente em `daily.c`: campo cresce com a água anterior; produção/limpeza; clima atual recebe previsão; próxima previsão; irrigação da chuva. |
| `overlay/11_GF_FARM/field/field.c`: `field_gate_open`; `world/weather.c`: `field_reset_cells` | `hmbtn_farm_field_water/rain`: terra arada/semeada elegível; mesma operação para regador e chuva. Grama estabelecida, células inativas e inverno excluídos. |
| `overlay/11_GF_FARM/field/field.c`: `field_request_dispatch` | Enxada em dia chuvoso deixa solo regado imediatamente, pelo mesmo gate; não depende de outra manhã. |
| `overlay/11_GF_FARM/world/weather.c`: partículas e drift; `farm_data.c`: `drift_by_weather` | Tick cosmético separado, sprites originais 25/26 e tonalidade; draw somente compõe a visão preparada antes de fades/UI. |

Regras sazonais preservadas: primavera usa `(rand(10)+1)%3==0` para chuva;
verão usa `rand(20)==14` para tempestade e `==3` para chuva; outono usa
`rand(5)==3`, exceto dia 13; inverno usa `rand(10)==7` para nevasca e demais
ímpares para neve. As quatorze datas de `clock_date_index` e os aniversários das
cinco candidatas forçam sol. Tempestade/nevasca antes de um evento vira
chuva/neve. A previsão usa a estação do próximo dia, inclusive ao virar o ano.

Valores originais: 0 sol, 1 chuva, 2 neve, 3 tempestade, 4 nevasca. Só chuva
executa irrigação; tempestade não é tratada como chuva para simplificar.
Água aplicada pela chuva de hoje participa do crescimento na próxima manhã.

## Estado, consumidores e persistência

`HmbtnGameState.weather` contém `today`, `tomorrow` e `random` em uma única
instância por sessão. Os dois inteiros privados da casa foram removidos.
Casa/TV e vila emprestam referências; fadiga de ferramentas, pesca e manejo,
horários de galinhas, gatilhos e variável de script 19 usam o mesmo clima.
`hmbtn_village_refresh_weather` gera as cinco flags meteorológicas para os
roteiros, sem manter outro clima mutável ou forçar sol nas agendas.

A chave de data existente garante uma atualização diária, incluindo sono,
passagem normal de 06:00 e fast-forward. Repetir a notificação da mesma data
não troca clima, consome RNG ou reaplica crescimento. Carga de área e draw não
inicializam o domínio. Preview de uma data nova inicializa a previsão uma vez.

Novas sessões usam seed 1. O stream usa o algoritmo original, separado dos
streams nativos já existentes de pesca/galinhas/moradores. Partículas têm outro
stream estritamente cosmético, sem interferir na previsão. Quarenta partículas
preservam a densidade visual inicial; isso não limita entidades, mapas ou
jogadores no PC. Reiniciar essas partículas na carga não afeta gameplay.

O codec escreve **v2**. As palavras `today/tomorrow` mantêm sua posição antiga em
AUXI e o RNG é acrescentado ao final da seção. A leitura de **v1** conserva os
valores gravados, instala seed 1 para o estado aleatório antes inexistente e
atualiza as flags derivadas. Não sorteia na carga nem altera inventário/campo.
O primeiro clima após a carga continua sendo a previsão que já estava salva.
Valores meteorológicos fora dos cinco tipos implementados são rejeitados.
Versões desconhecidas continuam rejeitadas. Não houve importação de memory cards.

## Validação

Linux x86-64 Debug, SDL3 local 3.4.18, dados reais instalados pelo BIN do usuário:

- `cmake --preset native-debug`: sucesso.
- `cmake --build --preset native-debug`: sucesso, produção com `-Werror`.
- `ctest --preset native-debug`: **105/105**, zero falhas, **134,14 s**;
  63 verificações sem depender dos dados opcionais e 42 com recursos instalados.
- `native_weather`: vetores sazonais com seeds conhecidas; quatorze datas e cinco
  aniversários/alternativos; vésperas, exceção de outono, estação/ano, contexto
  de casamento; ordem de irrigação/crescimento, grama/inverno/células inativas;
  uma passagem por data; 95 dias em avanço único versus diário; cinco tipos,
  flags/TV/scripts, pausa e isolamento do RNG cosmético.
- `native_weather_playthrough`: previsão real da TV; sono e terreno molhado;
  enxada na chuva; save/destruição/carga comparada; viagens; crescimento após
  a chuva e continuação da sequência; cinco apresentações com recursos reais;
  draw repetido sem alterar clima/partículas. Capturas de chuva/neve/TV verificadas.
- `native_save`: RNG não padrão em roundtrip; migração de bytes v1 válidos,
  preservação da previsão e regravação em v2, corrupção e versão desconhecida.
  Save v1 real de berinjela da entrega anterior também abriu no aplicativo.
- Regressões de cultivo, loja, galinhas/transporte, conversas, calendário,
  saves e atualização independentes do desenho incluídas na suíte completa.

Os vetores antigos de noites secas/rega manual e pasto ensolarado agora declaram
esse clima na fixture. A produção continua gerando clima real; não foi desligada
a chuva para preservar expectativas antigas. Conversas com vila de teste
emprestam o clima da sessão, assim como a vila real.

Logs locais: `/tmp/hmbtn-pf06-weather-final-{configure,build,tests}.log`,
`/tmp/hmbtn-pf06-weather-targeted3.log`, `preview.log`, `v1-load.log` com o mesmo
prefixo; imagens em `/tmp/hmbtn-pf06-weather-shots/`. Snapshot anterior em
`/tmp/hmbtn-pf06-weather-before/`. Esses arquivos não entram no Git.

Não executados: Windows/cross-build, Release, sanitizers, Android, instalação
limpa ou sessão manual de janela/áudio. Capturas usam SDL dummy/software; não
comprovam desempenho nem fidelidade audiovisual integral. Os avisos const da
fixture de mapa já existiam; nenhum novo aviso de produção foi introduzido.

## Reprodução

Preview novo na fazenda, Primavera 5, chuva hoje e sol amanhã:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --area-preview 0 --preview-date 0:5 --preview-time 10:00 \
  --preview-weather 1:0
```

`--preview-weather ATUAL:AMANHA` é uma fixture de desenvolvimento, aceita os
valores 0..4, exige preview de área e preserva o RNG. Não avança a data; para
verificar irrigação matinal, durma tendo previsão de chuva (`0:1`). Em chuva,
terra nova arada pela enxada já deve ficar molhada. `--preview-date` usa estação
0..3 e dia 1..30. Neve pode ser vista com `--preview-date 3:5 --preview-weather 2:0`.
Omitir a opção meteorológica usa a previsão sazonal real da sessão.

Percurso automatizado com capturas opcionais:

```sh
mkdir -p /tmp/hmbtn-weather-shots
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_weather_playthrough data \
  /tmp/hmbtn-weather.hms /tmp/hmbtn-weather-shots
ctest --preset native-debug -R 'native_(weather|save$)' --output-on-failure
```

## Arquivos desta entrega

Caminhos relativos à raiz; lista referente apenas ao recorte meteorológico,
comparada ao snapshot anterior, preservando todo o trabalho que já existia:

| Área | Arquivos criados/alterados |
|---|---|
| Domínio novo | `port/game/weather.c`, `weather.h`, `weather_effects.c`, `weather_effects.h` |
| Proprietário/ciclo | `port/game/game_state.c/.h`, `session.c/.h`, `daily.c` |
| Casa e consumidores | `port/game/house_interaction.c/.h`, `farm_action.c`, `fishing.c`, `poultry.c`, `village.c/.h`, `village_talk.c`, `event_variables.c`, `runtime.c`, `exploration.c` |
| Campo/luz | `port/game/farm_field.c/.h`, `farm_clock.c/.h` |
| Save | `port/game/save.c/.h` |
| Recursos/build | `port/cmake/generate_farm_tables.py`, `port/CMakeLists.txt` |
| Apresentação/preview | `port/graphics/scene_preview.c/.h`, `port/core/app.c/.h`, `main.c` |
| Testes novos | `port/tests/weather_test.c`, `weather_playthrough.c` |
| Regressões/fixtures | `port/tests/save_test.c`, `crop_daily_test.c`, `summer_crop_test.c`, `crop_playthrough.c`, `house_interaction_test.c`, `poultry_test.c`, `poultry_transport_playthrough.c`, `village_talk_playthrough.c` |
| Documentação | `docs/port/PF06_WEATHER.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`, `ROADMAP.md`, `ROADMAP_STATUS.md`, `ROADMAP_PENDENTE_CODEX.md` |

## Limites e continuidade

Casamento ainda não é um domínio jogável. A adaptação aceita data/flag de
casamento como contexto, testado isoladamente; o observer não inventa casamentos.
Faltam danos de tempestade/nevasca (`field_storm_damage`), consequências da
estufa, tremor de tempestades, trovões e cobertura completa de falas
meteorológicas. A seleção de áudio por clima foi integrada na correção
[PF06_RAIN_DOORS.md](PF06_RAIN_DOORS.md). A apresentação básica dos cinco tipos não conclui esses sistemas.

Não houve novo motor de calendário/cultivo/inventário/loja, modificação de `src/`,
`sdk/`, `disc/`, `tools/` ou do CMake PS1. SDL3, áudio, instalação e BIN do usuário
foram preservados. PNG, conteúdo novo e coop continuam fora deste recorte.
A fila principal continua no roadmap; PF-06 permanece aberta.


## Correção posterior — cor e deslocamento da chuva

Na entrega de Won de 2026-10-26, `main.c: main_draw_ots` e
`screen_fade.c: screen_fill_quad` confirmaram que a luz global vem depois das
partículas da world OT e antes da UI. O renderer nativo foi corrigido nessa
ordem: a paleta azul original agora também recebe a iluminação subtrativa.
`player.c: player_update_position` / `weather.c: particle_step` confirmam o
scroll projetado; o tick host o aplica às partículas visíveis usando a mesma
câmera do renderer, sem deslocar partículas por draw ou por teleporte entre áreas.
Testes de pixel real e movimento foram acrescentados ao percurso existente.
Atualização e validação em [PF06_WON_SEEDS.md](PF06_WON_SEEDS.md).
