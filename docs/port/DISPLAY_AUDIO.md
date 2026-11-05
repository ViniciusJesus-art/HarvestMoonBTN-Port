# Degradê, janela grande e alimentação do áudio

Correção de 2026-11-05 para Linux/Windows. Não altera o roteiro, os saves ou
as animações de portas do usuário. O bootstrap `--diagnostics` continua sem
executar o jogo; o runtime jogável permanece parcial.

## Causas e alterações

O menu desenhava o fundo em retângulos de uma unidade na altura lógica 720.
No SDL instalado, `SDL_render.c:SDL_RenderFillRects` aplica a escala e
`software/SDL_render_sw.c:SW_QueueFillRects` converte largura/posição para
inteiros. Em escalas fracionárias, as faixas deixam intervalos ou ganham
largura zero. Assim, a opacidade depende do tamanho da janela. O teste antigo
reproduziu ausência completa de cobertura a 640×480; a escala 1,5 de uma
janela 1080p também permite intervalos entre faixas.

`pause_menu` agora prepara uma textura RGBA contínua de 1024×1, com o mesmo
perfil de transparência e cores. Uma única cópia cobre todo o menu, inclusive
submenus e margens. A textura é reutilizada ao abrir/fechar o menu e liberada
no fechamento definitivo. O filtro nearest evita uma passagem bilinear cara
na alternativa de software; as 1024 amostras já excedem os 256 níveis do alfa.
Texto e demais elementos conservam sua apresentação normal. O tamanho lógico
do jogo e o modo de mistura são restaurados ao terminar a sobreposição.

O host também impunha `SDL_HINT_RENDER_DRIVER=software`. Isso fazia a CPU
escalar o quadro e compor texto/transparências na resolução da janela. Agora
SDL escolhe seu renderizador acelerado disponível e mantém sua alternativa de
software; uma escolha explícita por `SDL_RENDER_DRIVER` continua respeitada.
A composição original do jogo permanece em 320×240, sem alterar suas regras.

O aplicativo aguardava o `RenderPresent` com VSync e ainda aplicava o limitador
de 60 Hz. Em monitores com frequência diferente, essa combinação pode produzir
cadência irregular. A espera manual agora funciona somente quando VSync está
desligado/indisponível. A simulação conserva seu relógio independente. F11
reinicia o acumulador após a troca de modo, evitando recuperar como ticks de
jogo o tempo gasto pelo sistema ao mudar a janela.

O áudio antes era reabastecido apenas nos ticks de aproximadamente 30 Hz,
com alvo de 2048 amostras estéreo, cerca de 46 ms. Um quadro lento podia
consumir quase toda essa margem antes do próximo tick. O aplicativo agora
reabastece antes do desenho e após a apresentação, mesmo sem um tick novo.
Título e cenário expõem apenas a operação do host; ela não avança atores,
relógio, eventos ou input. O alvo é 3072 frames PCM, cerca de 70 ms, com
blocos de 512 e limite de seis blocos por chamada; a fila fica abaixo de
aproximadamente 81 ms. Pausa e volume continuam no stream existente.
Não foi criado um segundo mixer ou uma thread concorrente com as vozes.

## Validação

- `native_pause_menu`: amostras de cor no início/meio/fim do degradê sobre
  fundo conhecido, menu principal e controles, em 640×480, 1280×720,
  1920×1080, 2560×1440 e 3440×1440 no backend dummy/software. Após a correção,
  os valores de vermelho permanecem aproximadamente **17/112/198**, com
  variação de até dois níveis. Antes, o primeiro caso media **200/200/200**,
  iguais ao fundo sem sobreposição.
- `native_display_playthrough`: recursos reais de fazenda/título, fila de
  áudio esvaziada/reposta sem ticks, limite da fila, pausa sem gerar PCM,
  redimensionamento, tela cheia e retorno. Jogador/relógio/cachorro não mudam
  ao alimentar áudio; apresentar pausado também não avança a sessão.
- O percurso passou em X11/OpenGL real, incluindo tela cheia 1920×1200.
  A apresentação mediu aproximadamente 6,8–7,1 ms por quadro nessa sessão;
  inclui a espera do monitor e não representa um benchmark universal.
  O teste de cores também passou em OpenGL. O monitor/gerenciador de janelas
  limita tamanhos reais; as resoluções maiores exatas são verificadas no dummy.
- Debug: seleção de oito testes aprovada (menus, loop real de pausa, relógio,
  áudio/XA, display, potro e águas), seguida de reexecução de menus/display
  após o ajuste do filtro. Release: cinco testes aprovados.
- Windows: build MinGW e testes de menu, display/tela cheia, loop real de
  pausa, mixer e XA aprovados no Wine. Aplicativo exercitado em janela grande.
  Windows real/MSVC e outras combinações de GPU/monitor não foram medidos.

As falhas adicionais do teste gráfico real identificaram expectativas de
redimensionamento síncrono no próprio teste. Ele agora aguarda a janela,
apresenta o primeiro backbuffer e respeita o tamanho utilizável do monitor.
O dummy conserva a verificação das cinco resoluções exatas. Não há mudança
de preferência de tamanho nem espera extra de redimensionamento no produto.

A rodada usou **47 processos de teste**, dentro do teto de 50, incluindo tentativas,
CTest, wrappers/filhos e lançadores Wine. Nenhum recurso proprietário foi
adicionado; `src/`, `sdk/`, `disc/`, `tools/` e o build PS1 foram preservados.
