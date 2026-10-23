# Cadastro nativo de New Game

O porte adapta os estados 11..28 de `27_GF_TITLE/title.c`, a caixa de
`menu_window.c`, a fonte de `text.c` e os editores de `name_entry.c`.
O cadastro inclui nome do jogador, aniversário, nome da fazenda, nome do cachorro
e resumo. A confirmação agora inicia a chegada à fazenda, com jogador, Thomas,
diálogos e música, seguindo até o interior da casa/área 80. O bootstrap ainda
não executa o jogo completo: saída para a fazenda, demais interações, saves e demo continuam pendentes.
TV, calendário, baú, livro, música e passos já executam; veja [HOUSE_INTERACTIONS.md](HOUSE_INTERACTIONS.md).
Após a abertura já há caminhada/corrida com colisões estáticas na casa inicial. Veja [FARM_INTRO.md](FARM_INTRO.md).

## Caminho disponível

Depois da abertura, Enter encerra a introdução e outro Enter abre as opções.
Selecione New Game com cima/baixo e confirme com Z ou Enter.

1. A janela apresenta o prefeito. Z ou X avança as páginas e fecha cada diálogo;
   segurar Z/X acelera a escrita. Start não substitui esses botões nas mensagens.
2. A grade de nome usa setas para mover, Z para inserir, X para apagar e Enter
   para pedir confirmação. Aceita até oito glifos; espaços sozinhos não podem
   ser confirmados. Mark/Space/OK pertencem à grade original. Q/E enviam as
   combinações de ombros usadas pelo código original para trocar a grade.
3. Yes confirma o nome; No ou X retorna à edição. Nome de jogador vazio não
   pode ser cancelado.
4. A pergunta de aniversário abre o seletor de estação e depois a grade de
   trinta dias. Setas movem, Z escolhe, X volta. Os contornos e saltos do calendário
   seguem `name_sound_move_position()` e `name_sound_move_index()`.
5. Confirmado o aniversário, são pedidos os nomes da fazenda e do cachorro.
   Os editores reutilizam a grade original. X com o nome vazio retorna à etapa
   anterior; a fazenda mostra seu sufixo no cabeçalho, como no C original.
6. O resumo mostra nomes e aniversário. A primeira opção aceita; a segunda
   recomeça as perguntas pelo nome do jogador. A aceitação apaga imagem e música
   em dez ticks. Os nomes e o aniversário passam ao estado nativo da chegada;
   o título e seu áudio são liberados antes de carregar a cena. A chegada segue
   os eventos `0x74..0x79`: chegada, infância, canção, despedida e retorno a Thomas.
   O roteiro termina dentro da casa/área 80, com o jogador na porta e libera caminhada/corrida dentro da casa. Não grava save.

Os identificadores seguem os originais: nome 15, aniversário 16..18, fazenda
19..21, cachorro 22..24, resumo 25..26 e saída 27..28. Foi removido o passo extra
que a primeira adaptação havia inserido antes do editor de nome.

Na cópia PT-BR local, a primeira mensagem inclui créditos da tradução, lidos
sem remoção. As mensagens de arquivo seguem essa cópia. Rótulos fixos, estações
e o texto do resumo vêm das listas C americanas em `text_bank.c`/`text.c`;
a tradução dessas listas embutidas no executável PS1 ainda não foi portada.
A mistura de idiomas não comprova paridade com toda a edição PT-BR.

## Correções de cor e ajuda fixa

As cores numéricas de `prim_make_pixel/line/gouraud_rect()` são **RGB**: vermelho
vem de `color >> 16`. A primeira adaptação leu essas constantes como BGR e inverteu
vermelho/azul, deixando as janelas azuis. `port/graphics/window_frame.c` agora usa
a ordem correta e mantém os valores originais, produzindo molduras bege. Isso
não muda a interpretação BGR555 dos pixels de texturas do PS1.

O comando `0x0FF3` das instruções e confirmações usa o caminho de página fixa
em `text_window_frame()`. Ao abrir a caixa, o porte apresenta o texto inteiro,
sem animação ou sons de digitação. Diálogos do prefeito continuam graduais.
A lista de duas opções (`0x0FFA`) também aparece completa.

## Arquivos e recursos

- `port/game/new_game.c`: estado do cadastro, páginas, grade, nomes, aniversário,
  confirmação e saída. Sem SDL, ponteiros PS1 ou dados de jogo simulados.
- `port/graphics/new_game_ui.c`: fonte 8×12, retrato, ícones, calendário e resumo.
- `port/graphics/window_frame.c`: caixas, cantos, bordas e gradiente em RGB.
- `port/graphics/name_labels.inc`: listas de glifos do C original; personalizações
  continuam reservadas à etapa posterior ao porte.
- `port/graphics/asset_decode.c`: aplica bancos adicionais preservando retângulos
  existentes e desenha sprites indexados com checagens de limites.

O atlas de UI é separado do título e criado sob demanda. Lê 8 KiB de `00_gf.bin`
no offset zero, 22 KiB em `0x25000` e registros individuais de 8 KiB de
`05_messege3.bin`, sem carregar arquivos grandes inteiros:

| Registro | Uso |
|---|---|
| `0x3A2` | Apresentação e páginas da tradução |
| `0x397` | Pergunta pelo nome do jogador |
| `0x3A8` | Ajuda fixa dos editores de nome |
| `0x3A6` | Confirmação do nome |
| `0x3A5` | Pergunta pelo aniversário |
| `0x3A9` | Ajuda fixa do calendário |
| `0x3A7` | Confirmação do aniversário |
| `0x3A4` | Pergunta pelo nome da fazenda |
| `0x3A3` | Pergunta pelo nome do cachorro |
| `0x3AA` | Opções para aceitar/recomeçar o cadastro |

Texto é lido como halfwords little-endian em `0x1800`, até `0xFFFF`. Registros
com retrato trazem seu banco gráfico após o identificador de quatro bytes.
A fonte é 4 bpp em 896/0 com CLUT 896/252; o retrato é 8 bpp em 960/175 com
CLUT 0/490. O calendário usa a animação de ícones `0x2B`, frames 0..3.
O resumo expande somente os três nomes conhecidos, em buffers com tamanho limitado.

O motor de texto ainda não é geral: trocas de retrato para outros bancos externos,
substituições de diálogo por variáveis e rolagem além de quatro linhas por página
continuam sem suporte e produzem erros explícitos. Comparação de pixels/tempos
com PS1 permanece pendente, incluindo dither e interpolação do gradiente.

## Janela e resoluções

```bash
./build/native-debug/port/hmbtn_native --window-size 1280x720
```

A janela aceita redimensionamento, alta densidade e F11. As dimensões físicas
ficam separadas das coordenadas 320×240. Em 1280×720, o conteúdo 4:3 ocupa 960×720,
centralizado com margens laterais, escala inteira e pixels sem filtro.
As capturas incluem a janela inteira. Câmera 16:9 e resolução interna maior ainda
não estão implementadas; fazem parte das melhorias posteriores à paridade PS1.

## Verificação

O caminho atual do cadastro à chegada foi validado com os dados locais, em
dummy e em janela X11/software + PipeWire 1280×720: nomes, aniversário, confirmação,
carregamento e conversa com Thomas; 3.413 frames, saída zero e pico RSS cerca de
79 MiB. A suíte atual tem 17 testes CTest aprovados. Detalhes em
[FARM_INTRO.md](FARM_INTRO.md). O registro abaixo descreve o marco anterior do cadastro.

Configure/build e 14 testes CTest aprovados no Linux. A regressão de molduras
verifica RGB bege e gradiente. Os testes do cadastro cobrem ajuda completa ao abrir,
listas de opções, páginas, digitação, oito glifos, repetição/foco, calendário,
contornos de todos os trinta dias, voltar/corrigir, nomes, resumo e fade de saída.
Fixtures sintéticas não incluem recursos proprietários.

Com os dados locais, um percurso da UI preencheu nomes distintos, escolheu outono,
dia 30, confirmou o resumo e alcançou a fronteira antes da fazenda. As capturas
`build/native-debug/registration-{player,season,day,farm,dog,summary}.png` são
artefatos locais, ignorados pelo Git. A validação em Windows real/MSVC e a
comparação lado a lado com PS1 continuam pendentes.

Janela Linux X11/software + PipeWire em 1280×720: percurso completo com teclado
até aceitar o resumo, 2210 frames e saída zero; pico RSS de cerca de 74 MiB.
O teste isolado da UI com dados locais mediu cerca de 14 MiB; a recompilação
incremental serial, cerca de 46 MiB. Logs e medições ficam em
`build/native-debug/registration-window-validation.log` e
`registration-{ui,window,build}-memory.txt`.

O build Windows x64 desta revisão passou com MinGW e o mesmo C/SDL3, em contêiner limitado a
1536 MiB e uma tarefa. Esta revisão não é executada no Wine; Windows real/MSVC
continuam sendo validações separadas e pendentes.
