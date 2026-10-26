# PF-06 — Áudio da chuva e passagem pelas portas

Correção solicitada em 2026-10-26, dentro da PF-06. Preserva o porte PC,
SDL3, instalação pelo BIN e as alterações anteriores do usuário. Não modifica
`src/`, SDK, disco, ferramentas ou fluxo PS1. A próxima entrega funcional
continua sendo a definida no [roadmap](ROADMAP.md).

## Origem e comportamento

| Fonte original | Adaptação e resultado |
|---|---|
| `src/overlay/11_GF_FARM/farm_main.c`: `farm_track_id`, `farm_set_message_timing` | `port/game/world_map.c`: seleção compartilhada com clima. Chuva escolhe a faixa 22 (ambiência original VAB/SEQ), substituindo a música normal; tempestade/nevasca escolhem 23, neve mantém a faixa sazonal. Conserva as exceções de cena 14/29 e as regras de volume diurno/noturno/interior. APIs anteriores delegam ao mesmo seletor com sol. |
| Mesma seleção na entrada/retomada do mundo | `port/game/runtime.c` e `port/graphics/scene_preview.c` usam o seletor ao concluir a abertura, acordar, concluir eventos, mudar horário/área e carregar saves. Reutiliza o sampler e o banco de efeitos existentes, sem misturar música sazonal com chuva nem sintetizar outro som. |
| Interação de zona antes da ação de arremesso do jogador | `port/game/exploration.c` consulta a porta antes de presentes/coleta/arremesso. A recusa também consome a interação e carrega sua mensagem. Retira a consulta tardia redundante. O registro em `HmbtnGameState.inventory.held_item` permanece o mesmo durante a viagem. |
| `people/villager.c`: `villager_event_cell_start`, `villager_cell_trigger`, `villager_reached_cell`; `farm_init.c`: `farm_zone_trigger`; `world/doors.c`: `door_step`, `door_entries_update` | `port/game/area_doors.c/.h` adapta abertura, espera aberta e fechamento, aceleração 8, limite `0x420`, espera 11, sentido pela orientação e folha dupla espelhada. `village.c` liga células 3/17/18 aos eventos das portas da área; o morador espera a abertura e cruza usando o movimento existente. Efeitos originais `0x16`/`0x4c` usam o banco da fazenda. |

A entrada do jogador mantém o **recuo 30, aceleração 5, avanço 41 e entrada em
31 ticks** ajustados pelo usuário em `runtime.c`. As poses agora distinguem
mãos vazias, item pequeno e objeto grande durante recuo, avanço e chegada.

## Estado, ciclo e persistência

`HmbtnSession.area_doors` possui o controlador transitório da área. A vila recebe
uma referência ao mesmo controlador. As entradas são indexadas pelos modelos
decodificados do recurso, sem trazer o pool global de dez portas do PS1 para o
produto. Limites de 15 modelos/50 zonas pertencem ao leitor do formato atual.
Requisições de vários moradores para uma porta compartilham sua abertura.
A entrada do jogador assume exclusivamente seu modelo/folha dupla, restaurando
a base antes de aplicar sua animação existente.

O tick atualiza portas e rotas, aplica poses decodificadas e solicita som;
desenhar não avança nenhum desses estados. Menus/conversas/transições pausam o
ciclo junto dos moradores. Mudança de área reinicializa as poses; rebind de
recursos da mesma área preserva o controlador, que não contém ponteiros host.
Catch-up histórico de rotas não reproduz abertura/sons antigos.

O save v2 permanece compatível: conserva inventário, clima, ator, agenda e cursor
da rota. A animação transitória da porta não é serializada. Se a carga encontra
um morador ainda na célula da porta, ele solicita nova abertura e continua dessa
posição/célula, sem pular a passagem ou duplicar mudança de agenda. A espera
visual pode reiniciar após carregar. A política de bloqueio de save durante
viagem do jogador e transporte de animais continua igual.

Não implementa os demais eventos de história das células de rota, recepções,
embarques ou portas controladas por eventos especiais. Células sem uma porta
comum correspondente conservam o caminho anterior. Danos climáticos, trovões e
meteorologia integral continuam pendentes.

## Verificação

- `native_area_doors`: quatro sentidos, aceleração/limite/espera/fechamento,
  pausa, folha dupla, requisições compartilhadas, portas simultâneas, posse do
  jogador, dados inválidos, espera/passagem/mudança de agenda e catch-up.
- `native_weather`: seleção de faixas/volumes por clima, horário e estação,
  exceções originais e regressões meteorológicas existentes.
- `native_weather_doors_playthrough`: PCM real da chuva contínuo até completar
  o loop original de aproximadamente 186 s; som diurno/noturno/interior,
  troca para dia seco, carga, entrada/retorno carregando item sem descarte,
  recusa sem arremesso, arremesso normal longe da porta e efeitos das portas.
  Rotas reais da área 40 (evento 25, linha 0/célula 1 para sair e linha 1/célula 3
  para entrar) verificam abertura, passagem, fechamento, pausa, draw sem avanço
  e save/load durante abertura. Estados iniciais de inventário/rota são fixtures
  declaradas; geometria, células, algoritmos e áudio vêm dos recursos reais.
- `native_weather_playthrough` confirma a faixa 22 após chuva natural na manhã
  seguinte e após carregar o save. `native_poultry_transport_playthrough`
  preserva o transporte real de galinhas/pintinhos pelas entradas.
- `native_farm_interiors_playthrough` agora verifica o recuo do usuário nas
  três construções/quatro estações; o teste anterior ainda exigia ficar parado.

Reprodução:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
./build/native-debug/port/hmbtn_native --data data --area-preview 0 \
  --preview-date 0:5 --preview-time 10:00 --preview-weather 1:0
```

O teste integrado aceita diretório opcional para capturas:

```sh
mkdir -p /tmp/hmbtn-rain-doors-shots
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_weather_doors_playthrough data \
  /tmp/hmbtn-rain-doors-check.hms /tmp/hmbtn-rain-doors-shots
```

Configuração e build nativos passaram em Linux Debug: **111/111 CTests**,
zero falhas, **136,90 s**. Detalhes e limitações de plataforma estão no
[status](ROADMAP_STATUS.md).
PCM/capturas são validados com os drivers dummy/software; isso não equivale a
escuta em dispositivo físico ou teste de janela interativa no Windows.

## Arquivos desta correção

- Novos: `port/game/area_doors.c/.h`, `port/tests/area_doors_test.c`,
  `port/tests/weather_doors_playthrough.c` e este relatório.
- Integração: `port/game/world_map.c/.h`, `runtime.c`, `exploration.c/.h`,
  `session.c/.h`, `village.c/.h`, `port/graphics/scene_preview.c`,
  `port/CMakeLists.txt`.
- Regressões: `port/tests/weather_test.c`, `weather_playthrough.c`,
  `farm_interiors_playthrough.c`.
- Documentação: `docs/port/ARCHITECTURE.md`, `PORTING_MATRIX.md`, `ROADMAP.md`,
  `ROADMAP_STATUS.md`, `PF06_WEATHER.md`.
