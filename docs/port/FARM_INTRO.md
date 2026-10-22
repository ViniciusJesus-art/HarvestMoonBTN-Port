# Chegada, infância e retorno ao presente — porte parcial

New Game entrega nomes/aniversário ao roteiro nativo após aceitar o resumo.
A sequência executa a chegada (`0x74`), carrega a fazenda da infância/área 230,
mostra menino, cachorro e avô (`0x75`), apresenta as dez imagens das lembranças
(`0x76`), executa o encontro/canção (`0x77`), a despedida (`0x78`) e o retorno
a Thomas e a entrada na casa/área 80 (`0x79`). Os scripts chamam funções C host; nenhum código MIPS é executado.

```sh
./build/native-debug/port/hmbtn_native
./build/native-debug/port/hmbtn_native --intro-preview
./build/native-debug/port/hmbtn_native --childhood-preview
```

Z/Cross ou X/Circle avança mensagens. Escape fecha; F11 alterna tela cheia.
`--scene-preview` continua sendo uma entrada de desenvolvimento separada:
cenário com câmera livre, sem atores ou áudio. A imagem lógica permanece 320×240.

Após a conversa com Thomas, o evento `0x79` carrega o **interior da casa/área 80**,
posiciona o personagem junto à porta em `(-90,0,541)`, clareia a tela por dez
ticks e emite o pedido original `FARM_REQUEST_FINISH`. O roteiro encerra,
a casa permanece visível e o terminal registra a conclusão. A partir daí há
caminhada/corrida com colisões estáticas, descrita em [HOUSE_CONTROL.md](HOUSE_CONTROL.md).
Saída para a fazenda, colisões com atores, progressão e saves permanecem pendentes. O bootstrap e este recorte nativo ainda
não executam o jogo completo nem comprovam paridade 1:1 com PS1.
`--childhood-preview` começa diretamente na área 231/evento `0x77`, como atalho
de desenvolvimento; o fluxo normal continua passando pela chegada e imagens.

## Entrada na casa e término da abertura

A área 80 é a casa inicial na primavera, sem ampliação. O carregador lê
seção e texturas dessa área antes de confirmar `EVENT_TARGET_HOUSE = -2`;
o cursor do evento aguarda a carga e só então executa o posicionamento.
O modo 2 de `event_cmd_set_player_position` usa `house_door_positions[0]`
do original. O fade de dez ticks é concluído antes de `event_cmd_enter_mode(0,0,0,0)`.
Esse último comando registra o pedido de término e fecha a apresentação;
o terminador do script encerra a VM, preservando a cena clara na janela.
Esse pedido agora libera o controle na casa: setas/D-pad caminham; X/Circle
corre para a frente. O chão e os bloqueios vêm da geometria original. Faltam
saída para a fazenda, relógio, inventário e interação com os móveis.

Os modelos dos móveis têm identificador `0x41`; o modelo principal usa `0x42`.
O original trata ambos pelo mesmo layout de tabela relativa e pacotes prelit.
O leitor nativo valida integralmente offsets, índices e primitivas nos dois casos;
não reinterpreta os endereços como ponteiros host. O cenário contém 23 objetos
no modelo principal e 655 primitivas potenciais visíveis, incluindo os modelos
adicionais. A cama inicia na primeira chave do morph, ainda sem interação.

## Cenário e ordenação

`graphics/scene_decode.*` lê os 15 registros `FarmZoneModel` do cabeçalho da área:
flags, offsets relativos, posição, ângulos e velocidade de rotação. Ausentes e
ocultos não são desenhados; o slot inicial 0 fica visível e os slots 1/2 ocultos.
Os morphs começam na primeira chave. As portas e a caixa aparecem fechadas;
interagir com elas e animar os morphs permanece trabalho de gameplay.

Os modelos adicionais e o terreno compartilham a mesma ordenação de polígonos
com os sprites. Rotação Z/X/Y e translação são aplicadas antes da câmera. Isso
permite posicionar portas espelhadas e o objeto giratório sem desenhá-los sempre
por cima dos personagens. A área 0 soma 4.362 primitivas visíveis potenciais;
a fazenda da infância, 4.323. Texturas e paletas vêm dos respectivos bancos.

O campo usa a grade original 25×45 e o layout inicial 0 de
`title_fill_field_layout`: mato, pedras, rochas, tocos, galhos e madeira. Posições,
seleção de sprites e deslocamentos verticais seguem `field_draw_grid` e
`field_cell_look_spring`. O CMake gera um include no build com os três layouts
já existentes em `src/overlay/27_GF_TITLE/title.c`; não duplica mapas ou assets
em `port/` nem exige disco para compilar. A seleção aleatória dos três layouts
e o estado global do gerador original ainda não foram integrados; a prévia
permanece reproduzível no layout 0. Na área 230 o campo não é desenhado,
conforme o `place.location` original do flashback.

## Organização

| Arquivo em `port/` | Responsabilidade |
|---|---|
| `game/event_script.*` | Blocos/offsets, saltos e registros de 16 bytes |
| `game/farm_intro.*` | Comandos C dos eventos `0x74..0x79`, atores, câmera, mensagens e fades |
| `graphics/scene_decode.*` | Tabela de modelos adicionais, posição, rotação e visibilidade |
| `graphics/scene_preview.*` | Leitura das áreas/eventos, campo, sprites, imagens e apresentação SDL |
| `graphics/model_render.*` | Instâncias e sprites na mesma ordenação de profundidade |
| `game/new_game.*`, `graphics/new_game_ui.*` | Cadastro retido e texto/retratos compartilhados |
| `audio/title_audio.*`, `audio/psx_audio.*` | VAB/SEQ das músicas 26/31/32/33/34 e efeitos |
| `audio/xa_decode.*`, `audio/xa_stream.*` | ADPCM XA, filtragem por canal, reprodução em fluxo e mistura |
| `core/app.c`, `graphics/title_preview.*` | Transferência do cadastro e liberação do título |

A origem dos comportamentos é `event_script.c`, `event_commands.c`, `scene.c`,
`scene_view.c`, `field.c`, `anim.c`, `screen_fade.c`, `sound.c` e `title.c`.
Os fontes PS1 e seu fluxo de compilação continuam preservados.

## Recursos e memória

Todos os caminhos abaixo são relativos a `data/A_FILE/`:

| Arquivo | Intervalo / registros |
|---|---|
| `00_gf.bin` | Área 80: seção `[0x1C52800,0x1C5E000)`, texturas até `0x1C66000` |
| `00_gf.bin` | Área 0: seção `[0x83800,0xBA800)`, texturas `[0xBA800,0xDD800)` |
| `00_gf.bin` | Área 230: seção `[0x3483800,0x34BA000)`, texturas `[0x34BA000,0x34D9000)` |
| `00_gf.bin` | Área 231: seção `[0x34EC800,0x3509800)`, texturas até `0x351D800` |
| `00_gf.bin` | Área 232: seção `[0x3530800,0x3544000)`, texturas até `0x3550800` |
| `00_gf.bin` | Jogador `[0x2A800,0x73800)`, ANI até `+0xBE08` |
| `00_gf.bin` | Confirmação com a cabeça: `[0x3CE2800,0x3CE6000)`, ANI de 352 bytes |
| `00_gf.bin` | Arte do campo primavera `[0x385F800,0x386A800)`, banco a partir de `+5588` |
| `12_event_s.bin` | Evento `0x74`: `[0x2CC800,0x2CE800)` |
| `12_event_s.bin` | Evento `0x75`: `[0x2CE800,0x2DA000)` |
| `12_event_s.bin` | Evento `0x76`: `[0x2DA000,0x309800)` |
| `12_event_s.bin` | Evento `0x77`: `[0x309800,0x315000)` |
| `12_event_s.bin` | Evento `0x78`: `[0x315000,0x321000)` |
| `12_event_s.bin` | Evento `0x79`: `[0x321000,0x323000)` |
| `03_messege1.bin` | Mensagens `0x76B..0x77F`, 8 KiB por leitura |
| `09_sound.stm` | Música 31: VAB `[5058560,5224448)`, SEQ `[5224448,5230592)` |
| `09_sound.stm` | Música 32: VAB `[5230592,5376000)`, SEQ `[5376000,5380096)` |
| `09_sound.stm` | Música 33: VAB `[5380096,5525504)`, SEQ até `5527552` |
| `09_sound.stm` | Música 34: VAB `[5527552,5689344)`, SEQ até `5701632` |
| `09_sound.stm` | Efeitos: banco 0 `[0,24576)` |
| `disc/STR/01.XA` | Faixas 4/5 filtradas; um setor decodificado por vez |

Nenhum arquivo grande é lido por inteiro. O título e seu áudio são liberados
antes da chegada; os nomes e a data são copiados por valor. Ao trocar a área,
o cenário anterior é liberado; o atlas do campo é descartado na infância.
O evento anterior é substituído e seus offsets não viram ponteiros PS1 no host.
Há limites para modelos, primitivas, sprites, instruções e PCM decodificado.
Não há pré-carregamento de todo o arquivo de gráficos ou da trilha sonora.

## Eventos e limites

As solicitações de recursos usam `HMBTN_EVENT_PENDING`: o cursor fica no comando
até o host realmente carregar a área ou substituir o evento. Não se reconhece
uma troca inexistente como concluída. Áreas sem implementação continuam em uma
fronteira explícita. Comandos desconhecidos param; argumentos inválidos falham.

O recorte inclui posições, movimentos de 24/40 unidades dos atores e caminhada
do jogador, animações/esperas de término, visibilidade, slots/aritmética/testes,
câmera seguindo um ator ou deslizando até outro, diálogo, música e fades.
A área da infância usa as próprias texturas em sépia. As dez imagens são ANI
originais desenhadas em `(80,60)`, com fundo preto e tempos/fades do script.
Enquanto uma imagem é exibida não se rasteriza o cenário que ficaria oculto.

O encontro usa os atores 27/33, a música 33 e as faixas XA 4/5. O canto na área
232 recua a câmera com as mesmas 177 interpolações escalares do fonte original,
incluindo altura, distância e centro vertical. O retorno à área 230 troca o
elenco para quatro atores. Depois a área 0 recarrega o campo e a música 34;
Thomas conclui as falas `0x77A..0x77F`.

A confirmação com a cabeça agora carrega a linha de setores 12 do jogador.
O comando enfileirado contém modo/animação/flag de banco, não animação/frame;
ao terminar os 12 ticks dessa ação, o jogador restaura o sprite básico.
Os cursores/sombras usam a ANI 24 da UI compartilhada e respeitam ocultação.
A quebra inicial após FFC/FFB não conta como quinta linha: ela é consumida
antes de mostrar a página seguinte, inclusive nas falas maiores de Thomas.

Continuam pendentes: saída para a fazenda, água/animações
de textura, interações, morphs em movimento, chão/colisões gerais, inserts e
controles de texto além deste recorte, reverb/interpolação SPU e comparação
sistemática de câmera, imagem, tempos e áudio com o PS1. Detalhes das faixas e
sua validação independente em [XA_AUDIO.md](XA_AUDIO.md).

## Validação da etapa anterior (área 230)

- Configure/build e os 17 testes CTest aprovados no Linux. Os testes ampliados
  cobrem instâncias transformadas, profundidade entre modelos, tabela da cena,
  ocultação, offsets inválidos, vários atores, câmera e espera pelo carregador.
  As fixtures são sintéticas, sem recursos proprietários.
- Probe com dados locais: 6.500 ticks determinísticos, chegada → área 230 →
  conversa → imagens → limite da área 231. Saída zero, pico RSS 22.864 KiB
  (cerca de 22 MiB). `build/native-debug/childhood-probe.log` e `childhood-*.bmp`.
- Janela real Linux X11/software + PipeWire, 1280×720: 5.871 frames, 98,54 s,
  mensagens até `0x771`, dez imagens `0..9`, saída zero; pico RSS 81.972 KiB
  (cerca de 80 MiB), sem swap. Capturas `childhood-live-*.png` e log
  `childhood-window-validation.log` em `build/native-debug/`, ignorados pelo Git.
- O cadastro até a chegada já havia sido validado com nomes distintos e data;
  nesta etapa a janela direta testa o cenário expandido e os flashbacks.
- Build/pacote Windows x64 atualizado com MinGW em contêiner limitado a
  1.536 MiB, sem rede, uma tarefa de compilação. Execução em Windows real/MSVC
  e comparação com uma execução PS1 continuam pendentes.

As medidas de memória são deste ambiente, não um limite universal.


## Validação do encontro e retorno

- Configure/build e **18 CTest** aprovados no Linux, incluindo o novo teste XA.
  Eventos também cobrem espera por áudio/banco de ação, modo musical 34,
  atores 27/33 e os 177 passos da câmera.
- Probe com dados locais: entrada direta no encontro → canção → despedida →
  Thomas → limite da casa. Foram 3.976 ticks, saída zero, ambos os canais XA
  solicitados e eventos `0x77/0x78/0x79` carregados. Log `reunion-events.log` e
  capturas `reunion-*.bmp` em `build/native-debug/`.
- Probe desde a chegada: **6.792 ticks**, saída zero; todos os eventos
  `0x74..0x79` e as trocas 0→230→231→232→230→0 alcançam o mesmo limite.
  `full-opening-probe.log` e `full-opening-events.log`, na mesma pasta.
- Os canais XA 4/5 inteiros produziram PCM idêntico ao FFmpeg antes do
  resampling/mistura, sem dependência dele no executável.
- Janela real Linux X11/software + PipeWire, 1280×720: **8.330 frames em
  139,80 s**, encontro, XA 4/5, despedida, falas `0x773..0x77F` e saída zero no
  limite da casa. Log `reunion-window-validation.log`; captura direta adicional
  do renderizador em `reunion-readback-*.bmp`, incluindo a câmera da canção na
  área 232. As capturas externas do compositor podem repetir um frame antigo;
  a leitura direta SDL é a referência visual desta conferência.
- Executável e pacote Windows MinGW x64 atualizados com estes mesmos fontes,
  incluindo o decodificador/streaming XA. Windows real/MSVC continuam pendentes.

## Validação da entrada na casa

- Configure/build e **18 testes CTest aprovados** no Linux. O teste de evento
  verifica espera pela carga, posição original, fade completo, pedido de término
  e rejeição de posicionamento da casa fora da área suportada. O teste gráfico
  cobre renderização `0x41` e rejeita offsets inválidos e identificadores desconhecidos.
- Probe local com os recursos extraídos: chegada `0x74` até o terminador de
  `0x79`, **6.856 ticks**, ambos os canais XA e nenhuma fronteira não implementada.
  Confirma o carregamento da área 80 e a mensagem `Opening complete` no terminal.
  Captura direta SDL: `build/native-debug/full-opening-80.bmp`; PNG de inspeção:
  `build/native-debug/house-complete.png`. Acelera a simulação para validar
  transições e recursos; não comprova tempos/áudio em tempo real nem paridade PS1.
- O mesmo percurso passou em janela Linux real X11/software com PipeWire,
  captura via `SDL_RenderReadPixels`, 6.856 ticks e saída zero. Esse probe também
  acelera a simulação; a fidelidade do ritmo continua dependente de comparação PS1.
- Executável e pacote Windows x64 atualizados via MinGW; execução em Windows
  real/MSVC permanece pendente.
