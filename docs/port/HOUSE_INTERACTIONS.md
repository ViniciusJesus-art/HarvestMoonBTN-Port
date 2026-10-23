# Som e interações na casa inicial

A casa da primavera/área 80 agora executa música, passos, TV, calendário,
baú de ferramentas, livro/estante, opções e a sequência de deitar na cama.
O fluxo normal chega aqui
após `0x79`; o atalho continua disponível:

```sh
./build/native-debug/port/hmbtn_native --house-preview
```

Setas caminham, X/Circle corre e **Z/Cross interage**. É necessário estar
perto do objeto e olhar na direção prevista pela zona original. O alcance
é 224 unidades; os testes X/Z usam limites estritos e Y tem a folga original.
O personagem e a câmera param enquanto um desses menus está aberto.

## Comportamentos implementados

| Objeto | Comportamento |
|---|---|
| TV | Z abre a seleção. Cima: previsão; direita: programa diário; esquerda: entretenimento do dia; baixo: educação. Direções trocam o canal enquanto assiste. X fecha. |
| Calendário | Abre no dia atual. Setas movem com as regras originais de retorno; Z ou V/Triangle mostra o evento, ou a mensagem padrão. X/Start fecha. |
| Baú | A tampa gira antes de abrir a interface original. Setas escolhem uma célula; Z marca origem e depois destino para mover/trocar ferramentas. X cancela a marcação ou fecha; Start fecha. Q/E muda a página; Tab organiza; V mostra a descrição. |
| Livro/estante | Mostra a mensagem original `0x246`, com rolagem contínua; Z/X avança e fecha. |
| Opções (livro azul ao lado da cama) | Z abre o menu original. Setas e Z navegam/confirma; X retorna; Enter/Start fecha. Manual: Z exibe a descrição do botão. Configuração: V/Triangle exibe a explicação. |
| Cama | Z, olhando para a esquerda, abre as quatro entradas originais. Setas escolhem; X/Start cancela. As duas primeiras executam a animação de deitar até o limite do pernoite; diário/carregamento e troca de animais ainda não executam os overlays correspondentes. |

O baú começa com foice, machado, enxada, martelo e regador, conforme
`title_new_game_toolbox`. São 64 posições, equipamento e as duas posições
iniciais de ferramentas desbloqueadas. Os valores de id, modificador e
contagem são movidos/trocados, sem duplicar o item. Alterações permanecem
entre aberturas do baú **na mesma execução**. Não há save em disco nesta etapa.
O uso das ferramentas depende do gameplay externo ainda pendente.

As descrições das cinco ferramentas foram conferidas nos recursos locais.
O insert `0x4010` da descrição do regador mostra zero, correspondente ao
regador inicial vazio; enchimento/uso e contagens posteriores não estão portados.

O calendário usa o relógio inicial original: ano interno 0, primavera,
dia interno 1 (dia 2 exibido), segunda-feira às 6:00. Isso é separado do
aniversário cadastrado. Contém os 18 eventos das quatro estações da tabela
original; casamento e mudanças de data por pernoite ainda não estão integrados.
O cabeçalho e rótulos estáticos usam as tabelas C US da decompilação; diálogos
e instruções vêm da cópia importada (PT-BR no teste local).

A TV escolhe as mensagens a partir de data, estação e clima, incluindo os
programas especiais dos primeiros dias. A imagem do aparelho usa três packs
originais, alternados a cada 16 ticks. Ao fechar, restaura o grupo 18.
O clima atual/seguinte é o estado inicial ensolarado deste recorte; não há
avanço diário nem gerador de clima completo. A rolagem `FF1` mantém as quatro
linhas visíveis, deslocando-as por 14 pixels, com recorte na janela de texto.
As instruções `FF3` aparecem completas, sem digitação ou som de letras.

## Opções e relógio

O livro azul abre “How to operate / Configuration”. Os rótulos estáticos
continuam vindo das tabelas C US; as descrições e instruções usam o arquivo
importado. A página do manual contém as duas colunas originais (5/6 entradas),
com descrições `0x367..0x371`. A configuração apresenta quatro linhas:

- Controle: tipos 1/2/3, conforme `player_update_controller_flags`. O tipo 2
  permuta cima→esquerda, baixo→direita, esquerda→baixo e direita→cima.
  O tipo 3 usa essa permutação conforme a direção após 17 ticks parado.
  Menus continuam recebendo o D-pad sem essa transformação.
- Vibração: ativa/desativa e solicita uma vibração curta ao passar de desligada
  para ligada, encaminhada por SDL ao controle conectado. Sem controle há apenas
  a opção; não se simula vibração. Teste físico permanece pendente.
- Estéreo/mono: altera a mistura real das vozes, preservando o pan ao voltar
  para estéreo. A saída mono combina os canais antes do limite da amostra.
- Relógio: mostra/oculta o quadro original com estação, dia, semana e hora.
  Dentro da casa, este recorte permanece no horário inicial, 6:00.

O valor selecionado aparece opaco e as alternativas usam semitransparência.
As opções e o inventário permanecem apenas na sessão, sem persistência em disco.

## Cama e fronteira do pernoite

O menu conserva as quatro entradas de `sleep_menu`, a ajuda `0x1B7`, suas
posições e sons. “Sleep without making diary entry” e “Make diary entry and
then sleep” carregam a ANI real do modo `0x4A` (linha 15), executam suas quatro
faixas, o deslocamento de 17 passos da câmera/jogador e o fade de 30 ticks.
O pedido de diário fica registrado, sem gravar um arquivo incompleto.

A coberta usa `scene_link_handback`: 8 passos para levantar e 12 para acomodar,
alternando as entradas LINK 0/1. O renderizador interpola os vértices do primeiro
objeto entre as chaves originais e desenha a topologia de saída, sem alterar os
bytes importados ou converter endereços PS1 em ponteiros host.

Ao terminar o fade, a tela permanece preta e o terminal informa **Overnight
boundary**. Antes de trocar a data ou salvar, faltam `field_daily_tick`,
`farm_end_of_day`, clima, animais, eventos e a preparação da manhã.
Não há avanço fictício da data nem arquivo de save nesta etapa. Escape fecha.
“Read diary” e “Exchange animals” emitem o som de recusa e identificam o overlay
pendente no terminal, mantendo o menu utilizável. A interface de memory card e
o carregamento não estão portados.

## Arquitetura e recursos

- `game/house_interaction.*`: zonas ativas, alcance/direção, pausa, seleção
  dos canais, calendário, ferramentas, opções e menu da cama.
- `game/house_sleep.*`: sequência do jogador/câmera e transição da coberta,
  até a fronteira das atualizações diárias.
- `graphics/house_ui.*`: interface com fontes, molduras, cursores, ícones,
  fundos e imagens das ferramentas originais. Lê cada imagem em um setor.
- `graphics/new_game_ui.*` e `game/new_game.*`: texto compartilhado, rolagem
  contínua e clipping. A abertura/cadastro continuam usando o mesmo caminho.
- `audio/psx_audio.*`: três bancos separados — efeitos comuns, música,
  efeitos da fazenda — com programa/tom/nota/volume explícitos nos passos.
- `graphics/scene_preview.c`: carga, áudio, atualização da textura da TV,
  tampa do baú e composição dos menus sobre o mundo.

Origem: `people/player.c`, `event/messages.c`, `menu/calendar.c`,
`menu/item_storage.c`, `world/scene.c`, `farm_main.c`, `farm_load.c`,
`main/sound/sound.c` e `overlay/27_GF_TITLE/title.c`.

| Recurso em `data/A_FILE/` | Intervalo |
|---|---|
| `09_sound.stm` | Música 1: VAB `[468992,608256)`, header 8192 bytes; SEQ `[608256,618496)` |
| `09_sound.stm` | Efeitos da fazenda `[24576,251904)`, header 10240 bytes |
| `00_gf.bin` | TV: grupo `g` em `0x3D5A800 + g * 0x1800`, três setores por grupo |
| `00_gf.bin` | Imagem de item `id`: setor em `0x3C19000 + id * 0x800` |
| `00_gf.bin` | Ação de dormir `[0x3CED000,0x3CF0800)`, ANI alinhada em 312 bytes |
| `05_messege3.bin` | Um registro de 8192 bytes por mensagem |

O passo usa o SoundDef 72: VAB da fazenda, programa 8, tom 0, nota 68,
volume 20; intervalos seguem os limites 12/8 do original para andar/correr.
O tema da casa ensolarada é a música da estação (primavera: música 1),
conforme `farm_track_id`, com subida de volume por 100 ticks.
Não há CD/MIPS ou recursos proprietários no Git; as leituras continuam limitadas.

## Limites e validação

Avanço do pernoite e manhã seguinte, diário/save/load, outros armários de casas
ampliadas, casamento, receitas e saída para a fazenda ainda estão pendentes.
Essas operações não são substituídas por diálogos que aparentem completá-las.
A porta continua pausando na fronteira documentada em [HOUSE_CONTROL.md](HOUSE_CONTROL.md).
A casa ainda não comprova paridade 1:1 com PS1: ritmo de abertura/fechamento,
câmera, reverb/interpolação SPU e entradas completas precisam de comparação.
Em particular, fechar o baú ainda reverte a tampa junto do fechamento do menu.
As páginas de opções também ainda precisam de comparação dos intervalos de
fechamento/reabertura das janelas. A projeção da sombra durante o salto da cama
ainda usa o caminho comum; o original reprojeta sua profundidade no chão.

- Configure/build e **20 testes CTest Linux**: zonas/direção, seleção de TV,
  datas, conservação de ferramentas ao transferir/organizar, cancelamento,
  rolagem contínua, relocação de textura/CLUT e banco de efeitos separado.
- Probe com os dados locais: quatro canais e suas texturas, calendário/detalhe,
  cinco descrições de ferramentas, equipamento, fechamento/reabertura e texto
  completo do livro; saída zero. Log `build/native-debug/house-interaction.log`.
  Capturas `house-interaction-*.bmp` na mesma pasta, obtidas pelo SDL.
- O build original de PS1 e a validação SHA-1 continuam independentes e preservados.

- Executável real Linux X11/software + PipeWire, 1280×720: caminhada até a TV,
  Z para interagir e teclado para trocar os quatro canais; **600 frames, saída
  zero**. Captura direta SDL: `house-interaction-keyboard.bmp`; log
  `house-interaction-window.log`, ambos em `build/native-debug/`.
- Build e pacote Windows x64 recompilados com MinGW. Execução em Windows real
  e compilação MSVC continuam pendentes.

Validação adicional das opções/cama: os 20 CTests incluem navegação, controle
tipos 1/2/3, mistura mono/estéreo, cancelamento, limites de diário e a sequência
de pernoite sem avanço de data. O teste de modelos cobre interpolação e offsets
inválidos. Probe com os dados locais carrega todas as descrições, renderiza
as páginas, altera opções, confirma movimento remapeado e completa a animação
da cama/coberta até a fronteira. Evidências em
`build/native-debug/house-options-probe.log` e `house-options-*.bmp`.

A abertura completa `0x74..0x79` foi revalidada com os dados locais (6.856 ticks,
sem fronteira de roteiro), assim como TV, calendário, baú e livro. Executável
Linux X11/software + PipeWire, 1280×720: percurso por teclado até as opções,
seleção de mono e ocultação do relógio, descrição e retorno à configuração;
600 frames, saída zero. Log `house-options-window.log` e captura direta SDL
`house-options-keyboard.bmp`, em `build/native-debug/`. Executável/pacote
Windows x64 atualizados com MinGW; teste em Windows real/MSVC ainda pendente.
