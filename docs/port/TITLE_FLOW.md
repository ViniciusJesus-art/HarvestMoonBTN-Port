# Abertura e menu nativos (M3 parcial)

O executável PC adapta o C de `src/overlay/27_GF_TITLE/title.c` em
`port/game/title_flow.c` e `port/graphics/title_preview.c`. O bootstrap não executa
o jogo completo: saves, demo e gameplay completo ainda não estão portados.
O cadastro está em [NEW_GAME.md](NEW_GAME.md); sua confirmação agora inicia a
chegada com personagens e roteiro, descrita em [FARM_INTRO.md](FARM_INTRO.md). Não há
execução de instruções MIPS ou ligação ao SDK PS1.

## Sequência implementada

1. Logo **Natsume** (sequência ANI 30) sobre branco, com fades de 30 ticks.
2. Grama com duas camadas móveis, personagens e flores; inicialmente sem título.
3. Espera de 30 ticks; título entra de y=-160 até zero, um pixel por tick.
4. Copyright entra de y=48 até zero; “Press Start Button” entra de x=320 até zero,
   dois pixels por tick, pisca por 90 ticks e sai. O ciclo do texto se repete.
5. Start ou Cross encerra a introdução com fade de 10 ticks e mostra o título fixo.
6. **Outro Start** abre New Game / Continue. Continue começa selecionado, como no
   original. Cross não abre opções a partir da espera por Start.
7. Cima, baixo ou Select alterna a opção e toca o efeito de cursor. Start/Cross
   confirma e toca o efeito de confirmação. Continue apaga imagem/música em 10 ticks.

O título usa os estados originais 0..9; a apresentação e o cadastro
prosseguem pelos estados 11..28, descritos em [NEW_GAME.md](NEW_GAME.md). Os estados negativos representam a logo durante a
carga no host. O tempo extra que o PS1 permanece na logo depende das leituras de
CD e da preparação da fazenda; o porte não fabrica essa espera. Os dados necessários
à abertura são lidos em intervalos pequenos antes de começar sua apresentação.

Não há barra inferior, instruções de teclado ou painel de diagnóstico sobre os
pixels do jogo. O modo separado `--window-test` continua sendo um diagnóstico.
Após confirmar New Game, a apresentação e os editores do cadastro abrem.
Continue/demo registram o limite no terminal, sem simular gameplay; Start, Cross
ou Circle retorna ao menu nessa fronteira. New Game segue pela chegada, infância, canção e retorno a Thomas até
entrar na casa/área 80 e encerrar a abertura e liberar movimento com colisões estáticas na casa inicial. Escape fecha a janela.

## Animação e composição

`title_draw_bg_scroll()` usa o fundo colorido ANI 29 e duas malhas 3×3 da ANI 37,
semitransparentes, com deslocamentos senoidais e velocidades diferentes. A ordem
B → A sobre o fundo reproduz a inserção invertida da ordering table do PS1. Tiles
fora da tela são recortados antes de amostrar as texturas.

Os personagens começam nas animações 0, 10 e 19. Os três grupos de flores usam
22..27. Posições, períodos de seleção e avanço por instância vêm do original;
as instâncias de um grupo compartilham o player e avançam 3/5/3 vezes por tick.
A composição fica em um único buffer 320×240 e é reutilizada entre ticks. A
apresentação da janela não altera a velocidade das animações. O relógio usa cerca
de 29,97 ticks/s para o caminho americano com `VSync(2)`.

A função aleatória usa a recorrência original e o avanço por frame de `main.c`.
Sua semente ainda não inclui todas as chamadas anteriores feitas pela inicialização
da fazenda, que não está portada. As escolhas visuais aleatórias, portanto, ainda
não são sincronizadas com uma execução PS1 específica. Seno/cosseno usam ângulos
inteiros de 12 bits com arredondamento host; conferir os limites com a tabela SDK
faz parte da comparação de pixels pendente.

## Áudio

`port/audio/psx_audio.c` implementa leitura VAB v7, decodificação SPU ADPCM,
loop de samples, altura, panorama, envelope ADSR, 24 vozes e o subconjunto SEQ
necessário ao título. `port/audio/title_audio.c` envia PCM estéreo de 44,1 kHz à
SDL3 no Linux/Windows. Os dados vêm de `data/A_FILE/09_sound.stm`:

| Recurso | Intervalo no membro | Uso |
|---|---|---|
| Banco 0 VH/VB | 0..24576 | Efeitos: tom 0 confirmar, tom 5 cursor; nota 60, volume 60 |
| Banco 53 VH/VB | 4411392..4517888 | Instrumentos da música do título |
| SEQ 54 | 4517888..4528128 | Notas, tempos, instrumentos, volumes, panorama e repetição |

A sequência usa MIDI running status e os comandos Sony de loop CC99=20/30.
O início musical precede a região repetida: o primeiro loop é alcançado em cerca
de 49,92 s e retorna à marca interna, sem reiniciar a introdução da música.
Sem dispositivo de áudio a janela continua e registra explicitamente o motivo
no terminal; os testes usam o driver dummy, sem acessar os alto-falantes.

**Fidelidade sonora ainda incompleta:** o sampler atual interpola linearmente;
não implementa a interpolação gaussiana e o reverb da SPU. Os loops usam PCM
pré-decodificado, sem realimentar o histórico ADPCM entre repetições. As curvas
de volume/afinação/envelope e a cadência do sequenciador ainda precisam de comparação
com libsnd/PS1. Não tratar esta implementação como áudio idêntico ao console.
Não há bipes substitutos ou arquivos musicais recriados.

Formato consultado: [VAB/SEQ](https://psx-spx.consoledev.net/dev/ps1/cdr/cdromfileformats/audio/)
e [SPU](https://psx-spx.consoledev.net/soundprocessingunitspu/), além das tabelas e
chamadas de `src/main/sound/`. O suporte não é um sintetizador MIDI genérico:
eventos fora do recorte implementado geram erro explícito na importação da sequência.

## Validação e memória

- `cmake --preset native-debug`, build e 12 testes CTest aprovados no Linux.
- Testes sintéticos verificam ordem da abertura, posições/tempos do título,
  prioridade de botões, efeitos, ADPCM/VAB/SEQ, ADSR, panorama, loop, dados truncados
  e loop de duração zero, sem recursos proprietários versionados.
- Caminho completo de leitura/desenho testado com fixtures sintéticas, inclusive
  pasta portátil Unicode e ausência da barra inferior.
- Janela Linux X11/software + PipeWire: 1050 frames, duas passagens por Start,
  seleção e confirmação de ambas as opções; terminou normalmente.
- Janela padrão Wayland/software + PipeWire: 180 frames, abertura e saída normal;
  pico RSS de cerca de 108 MiB.
- Renderização offline de 55 s com os recursos locais: pico PCM 23843/32767,
  zero samples saturados, uma passagem pela marca de repetição.
- Capturas locais de Natsume, grama sem título, entrada do logo e menu ficam em
  `build/native-debug/capture-*.png`; não são distribuídas.

Nesta máquina, a recompilação incremental serial mediu pico RSS de cerca de
46 MiB, a janela real com renderizador de software 75 MiB (X11) / 108 MiB
(Wayland) e o teste sem display
16 MiB. São medições desse recorte, não limites gerais para qualquer driver/build.
O áudio mantém no máximo aproximadamente 46 ms na fila e limita a memória de
samples decodificados a 4 MiB por banco. Não lê discos/arquivos de recursos inteiros.
Os presets usam uma tarefa de build e um teste por vez. A apresentação de software
é o padrão deste recorte 2D; `SDL_RENDER_DRIVER` continua podendo sobrescrever isso.

A comparação lado a lado com uma execução PS1 de referência, incluindo gravação
de áudio e entradas repetíveis, continua obrigatória antes de declarar fidelidade
completa. O `.exe` foi recompilado com MinGW x64 pelo mesmo C e SDL3, em contêiner
limitado a 1536 MiB e uma tarefa. Esta alteração não foi executada no Wine;
validação em Windows real/MSVC continua pendente.
