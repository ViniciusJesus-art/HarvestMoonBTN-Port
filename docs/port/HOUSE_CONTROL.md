# Controle na casa inicial — M4 parcial

Atualização: a saída para a fazenda e o retorno inicial pela porta já estão
integrados; o gameplay externo completo e a paridade da transição ainda faltam. Veja [FARM_CONTROL.md](FARM_CONTROL.md).
As referências abaixo ao limite da porta descrevem a validação anterior.

Depois de New Game executar a abertura até o fim do evento `0x79`, o pedido
original de término libera caminhada e corrida dentro da casa/área 80.
O atalho de desenvolvimento abre diretamente esse mesmo cenário:

```sh
./build/native-debug/port/hmbtn_native --house-preview
```

Setas/D-pad caminham. X/Circle corre para a frente, inclusive sem uma seta
pressionada; soltar os botões retorna ao sprite parado na direção atual.
Escape fecha; F11 alterna tela cheia. O tamanho da janela continua separado
da imagem lógica 320×240, com proporção 4:3 preservada.
O atalho não executa o cadastro nem grava os estados da abertura ignorada.

## Origem dos comportamentos

| Arquivo em `port/` | Origem e função |
|---|---|
| `game/ground.*` | `main/geometry/ground.c`: chão por triângulos/quads, flags, tolerância, altura e centros dos objetos |
| `game/house_control.*` | `people/player.c`: escolha da direção, velocidades, animações, teste à frente e deslizamento ao correr |
| `graphics/scene_preview.c` | Liberação após o evento, dados reais, câmera, animação e apresentação SDL |
| `core/main.c`, `core/app.*` | Atalho `--house-preview`, exclusivo dos demais modos |
| `tests/house_control_test.c` | Cenário sintético sem recursos proprietários para verificar bloqueios, alturas e controle |

A direção segue `player_dpad_facing`: pares diagonais escolhem uma das quatro
direções conforme a tabela original; não é criado movimento diagonal novo.
A caminhada usa 41 unidades por tick; a corrida, 73. Como `player_update_mode`,
a velocidade vem do modo anterior, antes da troca de animação, preservando o
tick inicial de partida e o primeiro tick ao trocar caminhada/corrida.
Os intervalos ANI são 1..4 para caminhar e 6..9 para correr; parado usa o
intervalo 0 e o frame da direção. A animação continua sendo decodificada dos
recursos extraídos, sem sprites substitutos.

O teste à frente usa 48 unidades na direção atual, máscara `0xE080` e
tolerância de altura 200. Os primeiros cinco ticks bloqueados aguardam;
ao correr, as duas consultas laterais e os pequenos passos de deslizamento
seguem as tabelas do original. A posição final consulta o chão com `0xC080`.
A posição só é publicada se houver chão válido. O teste retorna o primeiro
polígono aceito, respeitando a ordem dos objetos/primitivas e o recorte pelos
centros do modelo. Não há caixas de colisão desenhadas manualmente para os móveis.

Os offsets do modelo permanecem inteiros relativos, validados na abertura do
recurso; são resolvidos em bytes host sem executar código MIPS. Produtos de
geometria usam intermediários de 64 bits para evitar overflow C; isso não
comprova equivalência dos casos extremos com os limites aritméticos do PS1.
A casa tem localização 2, sem atalhos de chão nas tabelas `FarmChoice`; sua
consulta é feita pelo modelo. Este recorte não implementa ocupantes dinâmicos,
animais, ferramentas, carregamento de itens ou salto.

## Porta e limite da etapa

A leitura das zonas usa os registros originais de 44 bytes, limites estritos
em X/Z e filtragem de zonas inativas/de interação. A porta da casa inicial
é a zona 0: aponta para a área 0, com entrada externa `(-4464,0,4032)`.
Ao entrar nela, o controle pausa e o terminal registra `House exit boundary`.
A casa permanece visível; Escape fecha a janela. Ainda não há transição de
porta, animação de saída nem execução dos eventos externos.

A etapa seguinte acrescentou música, passos, TV, calendário, baú e livro;
veja [HOUSE_INTERACTIONS.md](HOUSE_INTERACTIONS.md). Continuam pendentes
saída/entrada da fazenda, cama/pernoite, diário/save/load, opções, progressão
do relógio, colisões com atores e o restante do gameplay. Não afirmar paridade 1:1: câmera,
velocidades, imagens e áudio ainda precisam de comparação reproduzível com PS1.
O bootstrap e este recorte ainda não executam o jogo completo.

## Validação

- Configure/build e **19 testes CTest aprovados no Linux**. O teste novo cobre
  máscaras de chão, área sem piso sob um móvel, alturas/tolerância, plano
  inclinado, recorte por centros, partida/parada, caminhada/corrida, prioridade
  dos pares diagonais, bloqueio e zona de saída com limites estritos.
- Probe local com recursos reais: nove casos de 100 ticks (quatro direções
  caminhando, quatro correndo e chegada à porta). Cada posição foi consultada
  contra o chão; todos os casos moveram o personagem, respeitaram o chão e a
  porta foi reconhecida. Log: `build/native-debug/house-movement.log`.
  Capturas SDL: `house-walk-0.bmp` até `house-walk-8.bmp` na mesma pasta.
- Executável real em Linux X11/software + PipeWire, 1280×720: teclado enviou
  quatro direções e corrida; **480 frames, saída zero**. Captura direta SDL:
  `build/native-debug/house-keyboard.bmp`; imagem de inspeção
  `house-keyboard.png`; log `house-window-validation.log`.
- O probe acelerado da abertura continua verificando chegada `0x74` até o
  término de `0x79`, com os dois canais XA e liberação do controle na casa.
- Windows x64 é compilado com MinGW; validação de execução em Windows real
  e compilação MSVC permanecem pendentes. Recursos proprietários, probes locais
  e capturas continuam fora do Git.
