# Roteiro do porte nativo — Linux e Windows

Este arquivo acompanha a implementação. Atualizar os itens e a evidência ao concluir
cada marco. O objetivo é seguir o comportamento original, adaptar o código C do jogo e compilá-lo para PC; não executar
o binário MIPS em um emulador. Android, mods, troca facilitada de sprites/textos/interface, maior resolução interna
e câmera 16:9 ficam para depois da paridade do jogo básico com o PS1. A janela
pode aceitar resoluções físicas maiores desde já, preservando a imagem original.

## Situação inicial

- [x] Build PS1 preservado: executável e 13 overlays conferidos com SHA-1.
- [x] Build C11 nativo separado em `build/native-*` e testes de little-endian.
- [x] Importação única do disco para `data/`, com integridade e manifesto.
- [x] Reconstrução PS1 usando somente a pasta extraída.
- [x] CI configurada para Linux/Windows em Debug e Release.

Os dados locais vêm de uma tradução PT-BR com alterações também em código. Isso
não confirma compatibilidade com o C americano compilado. O build PS1 continua
exigindo os hashes originais. A prévia PC decodifica recursos; ainda não executa o jogo.

## M1 — Aplicação nativa com janela (primeira implementação)

- [x] Integrar SDL3 ao build host, com versão de download fixada e hash.
- [x] Criar janela redimensionável com apresentação lógica de 320 × 240.
- [x] Implementar apresentação de frames, eventos e temporização sem espera ocupada.
- [x] Fechar por Escape/botão da janela e alternar tela cheia com F11.
- [x] Mostrar diagnóstico explícito: esta etapa ainda não executa o jogo.
- [x] Manter diagnóstico de console e execução finita para testes automáticos.
- [x] Validar build, testes sem display e uma janela real no Linux.
- [x] Preparar a CI Windows com a mesma dependência.
- [ ] Validar execução gráfica no Windows (pendente; não testada neste host).

Entrega: executável de PC abre sua própria janela e responde aos eventos. Não
equivale à tela de título nem ao loop original portado. SDL3 cuida inicialmente
de janela, eventos e apresentação; a tradução gráfica PS1 permanece trabalho próprio.

## M2 — Recursos reais na janela

- [x] Implementar leitura nativa de intervalos de bytes dos membros em `data/A_FILE/`.
- [x] Validar limites/offsets e identificar ANI/bancos por seus cabeçalhos e layout.
- [x] Adaptar a decodificação dos bancos `0x0D` de `texture_pack.c`, sem chamadas GPU PS1.
- [x] Decodificar paletas/cores e composição de partes/frames de `anim.c`.
- [x] Exibir imagem e animação reais da tela de título na janela PC.
- [ ] Comparar sistematicamente a imagem e o ritmo com execução PS1 de referência.
- [x] Testar dados truncados e compressão com amostras sintéticas, sem assets no Git.

Entrega: visualização de recursos originais pelo renderizador PC. Ainda não é a
lógica da tela de título. Manter os recursos no formato original inicialmente.

## M3 — Tela de título executando código C adaptado

**Em andamento:** logo Natsume, introdução, fundo animado, áudio nativo e estados
0..9 e 11..28 implementados em C host. O cadastro inclui nomes, aniversário e
resumo. A transição agora inicia a chegada à fazenda/evento `0x74`; a sequência da infância (`0x75..0x79`) e a entrada na casa também executam;
gameplay, saves e demo continuam pendentes. Detalhes em
[TITLE_FLOW.md](TITLE_FLOW.md) e [FARM_INTRO.md](FARM_INTRO.md).

- [x] Adaptar logo Natsume e entrada gradual de título/copyright/texto antes das opções.
- [x] Adaptar as duas camadas móveis de grama, personagens e flores.
- [x] Reproduzir música SEQ e efeitos VAB/ADPCM do título em SDL3.
- [x] Remover barras/textos de diagnóstico da área do jogo.
- [x] Portar apresentação do prefeito, pergunta pelo nome, fonte e retrato originais.
- [x] Portar grade e confirmação do nome do jogador, com limite de oito glifos.
- [x] Validar janela 1280×720 mantendo conteúdo 4:3 e capturas com margens.
- [x] Portar aniversário, nome da fazenda, nome do cachorro e resumo do cadastro.
- [x] Transferir o cadastro e iniciar a chegada à fazenda/evento `0x74`.
- [x] Adaptar espera por Start, seleção/confirmar, cursor e fades do menu.
- [ ] Conferir imagem, semente aleatória, tempos e áudio contra execução PS1 de referência.
- [ ] Completar interpolação, reverb e comparação de volumes/afinação com a SPU.
- [x] Mapear teclado e gamepad para os bits PS1, com hotplug e teste virtual.
- [x] Registrar no terminal os limites antes de cadastro, saves, demo e fazenda.
- [x] Gerar `.exe` Windows x64 e verificar via Wine, sem confundir com Windows real.

- [ ] Inventariar chamadas e estado necessários em `main.c` e `27_GF_TITLE/title.c`.
- [ ] Substituir endereços fixos por estado e buffers alocados no host.
- [ ] Separar dados serializados de estruturas de runtime e ponteiros host.
- [ ] Compilar o recorte necessário do C do jogo para x86-64.
- [ ] Substituir overlays desse recorte por seleção de módulos/funções nativos.
- [ ] Integrar estados, animações, transições e comandos do menu original.
- [x] Mapear teclado/gamepad aos botões exigidos pelo recorte do título.
- [ ] Conferir escala, transparência e ritmo com o jogo PS1 de referência.

Entrega: primeiro marco do **jogo portado** — tela de título funcional, executando
lógica C nativa e lendo arquivos extraídos, sem executar instruções MIPS.

## M4 — Primeiro trecho jogável

- [x] Decodificar o TMD prelit da área 0 com offsets/índices validados.
- [x] Implementar oito primitivas, texturas afins, cores e ordenação desse modelo.
- [x] Exibir o modelo base da fazenda em `--scene-preview`, com câmera de diagnóstico.
- [x] Projetar sprites do jogador/Thomas com ordenação junto aos polígonos.
- [x] Executar a chegada: caminhada, poses, diálogos, fades e música do evento `0x74`.
- [x] Renderizar modelos adicionais: portas, caixa, objetos com rotação e visibilidade inicial.
- [x] Desenhar o campo inicial com mato, pedras, tocos e galhos dos layouts originais.
- [x] Carregar a área 230 e portar a lembrança da infância/evento `0x75`.
- [x] Exibir as dez imagens do evento `0x76`, com tempos e fades do script.
- [x] Carregar áreas 231/232 e portar encontro e canção/evento `0x77`.
- [x] Ler/decodificar XA em fluxo para as faixas 4/5, sem disco e sem processo externo.
- [x] Portar despedida `0x78` e retorno a Thomas `0x79`, com música 34 e falas.
- [x] Usar o banco de ação correto para a confirmação com a cabeça e restaurar o sprite básico.
- [x] Carregar o interior da casa/área 80 e encerrar o roteiro da abertura.
- [x] Integrar o término da abertura ao controle na casa, com chão e colisões estáticas originais.
- [x] Integrar a transição inicial casa↔fazenda e interação com a porta externa.
- [ ] Completar o ciclo original de portas e os eventos da fazenda.
- [ ] Adaptar operações de geometria/GTE e ponto fixo usadas pelo cenário escolhido.
- [ ] Implementar as primitivas e ordenação gráfica exigidas nesse recorte.
- [x] Carregar um cenário, posicionar câmera e desenhar chão/objetos/personagem.
- [x] Integrar movimento, animação, colisões estáticas e uma transição de porta.
- [ ] Comparar posição, câmera e colisões em uma sequência reproduzível de entradas.

Entrega: personagem controlável em um cenário real. Não ampliar para todo o jogo
antes de verificar esse caminho completo.

A prévia já adapta projeção/matrizes inteiras, mas a paridade GTE/GPU permanece
pendente. New Game inicia a chegada com os atores e as mensagens do primeiro
evento, encontro, canção, despedida e retorno ao presente. A sequência entra
na casa/área 80, posiciona o jogador na porta e encerra o roteiro original.
Já há caminhada/corrida na casa inicial e na fazenda, saída/retorno pela porta
e leitura das placas. Colisões dos ocupantes do campo, ferramentas, eventos
externos e gameplay completo permanecem pendentes. Detalhes em
[SCENE_RENDERER.md](SCENE_RENDERER.md) e [FARM_INTRO.md](FARM_INTRO.md).

## M5 — Sistemas do jogo

- [x] Relógio/calendário, ciclo visual dia/noite e transições de dia no recorte casa/fazenda.
- [ ] Clima e conjunto completo de atualizações/eventos diários.
- [ ] Ferramentas, inventário, plantação e economia.
- [ ] Animais, moradores, família e rotinas.
- [ ] Scripts, diálogos, festivais e minijogos.
- [ ] Bancos sonoros, sequências, efeitos e áudio XA.
- [ ] Saves em arquivos host; definir formato/versionamento e importação PS1.
- [ ] Validar progressão e ida/volta de saves em Linux e Windows.

Entrega: jogo básico funcional e verificável. Subsistemas ausentes devem continuar
identificados como ausentes; não usar stubs para aparentar gameplay implementado.

## M6 — Instalação e distribuição

- [ ] Empacotar executáveis e bibliotecas Linux/Windows, sem dados proprietários.
- [ ] Transformar o importador existente em etapa de instalação da cópia do usuário.
- [x] Localizar a pasta de dados instalada sem depender do diretório atual.
- [ ] Validar instalação limpa, atualização e erros de arquivos ausentes.
- [ ] Avaliar suporte a traduções cujas mudanças também afetam código.

Android, melhorias e mods terão roteiro próprio após esses marcos.

## Organização e validação

- Adaptações em `port/`; documentação em `docs/port/`. Preservar `src/`, `sdk/`,
  `disc/`, `tools/` e o fluxo PS1. Adaptar fontes originais apenas no recorte necessário,
  com mudanças pequenas e auditáveis que permitam manter o build original.
- `port/core/`: aplicação e estado host; `platform/`: janela/serviços SDL;
  `graphics/`: apresentação e posteriormente renderização do jogo; `input/`:
  eventos e controles; `filesystem/`: leitura de intervalos; `audio/`: VAB/ADPCM/SEQ do título.
- SDL3 é infraestrutura de PC, não substitui automaticamente GPU, GTE ou SPU do PS1.
- Base: `cmake --preset native-debug`, `cmake --build --preset native-debug`,
  `ctest --preset native-debug`. A janela deve ter teste finito para não travar a CI.
- Testes sem display não comprovam apresentação real. Registrar separadamente
  execução gráfica Linux, execução Windows e comparações com o jogo.
- O teste host não substitui a verificação SHA-1 dos binários PS1.

## Registro de execução

### M1 — 2026-10-22

- Build Debug em Linux x86-64 concluído com SDL3 3.4.18 estático.
- Cinco testes CTest passaram: formatos, diagnóstico sem vídeo, três frames em
  dummy/software, atalhos da janela e importação de dados sintéticos.
- Janela real Wayland/OpenGL: 120 frames apresentados; captura em
  `build/native-debug/window.png` (artefato local, ignorado pelo Git).
- Janela real X11/OpenGL: redimensionamento a 1280 × 720 conferido; com a janela em
  foco, F11 ativou tela cheia e Escape encerrou normalmente. O envio de teclas sem
  foco não serviu como validação dos atalhos; foi substituído pelo teste com foco.
- Windows: CI preparada; execução e validação visual permanecem pendentes.
- M1 não executa `src/`. A implementação de recursos foi adicionada em M2 abaixo.


### M2 — 2026-10-22

- Leitura por intervalos e decodificadores C de bancos/ANI implementados em `port/`.
- Prévia nativa mostra fundo, logotipo e três sequências animadas da tela de título.
- O padrão do executável agora lê `data/`; `--data DIR` seleciona outra pasta.
  `--window-test` mantém o diagnóstico de M1 sem exigir recursos.
- Oito testes CTest passaram, incluindo pipeline completo com pacote sintético.
- Janela real Wayland/OpenGL: 180 frames com recursos locais; captura em
  `build/native-debug/title-desktop.png`. Sem consulta à imagem original.
- A comparação com o PS1 e a execução Windows permanecem pendentes.
- Formatos e limites documentados em [ASSET_FORMATS.md](ASSET_FORMATS.md).
- Próxima implementação: M3, máquina de estados e entrada da tela de título.


### M3 parcial — 2026-10-22

- `port/game/title_flow.c` inicialmente adaptou os estados 4..9, máscaras e prioridade
  de entrada do original. A abertura foi acrescentada na revisão seguinte, abaixo.
- Menu desenhado com sprites originais e cursor; confirmar destinos ainda não
  integrados apresentava um aviso de indisponibilidade com retorno ao menu; o aviso
  foi posteriormente retirado da imagem e mantido no terminal.
- Onze testes CTest Linux passaram, incluindo fluxo, teclado e gamepad virtual.
- Janela real X11/OpenGL: Start, cima/baixo, confirmar New Game/Continue e voltar
  conferidos com eventos sincronizados aos estados. Captura: `build/native-debug/title-menu.png`.
- MinGW x64 em Docker: `.exe` gerado com imports somente de DLLs Windows do sistema.
  Seis testes C, teste adicional de gamepad virtual e frames Win32/software passaram
  em Wine. Caminho com espaço/acento e outro diretório de trabalho conferidos.
- Pacote local: `build/native-windows-x64/package/`, sem dados do jogo. CI MSVC
  Debug/Release prepara o mesmo tipo de pacote após testes; execução remota pendente.
- Wine não substitui validação em Windows real, inclusive gamepad físico.
- Próximo recorte: texto/diálogo e cadastro do caminho New Game. M3 permanece parcial.

### Abertura fiel ao original e controle de memória

- Logo Natsume antes da grama/personagens; entrada gradual de título, copyright
  e Press Start, sem exibir diretamente as opções. Barras/painéis host removidos
  da imagem do jogo. Scroll, sprites e flores seguem as rotinas do título.
- Áudio VAB/ADPCM/SEQ real no Linux/Windows: música e efeitos de cursor/confirmar.
  Interpolação gaussiana/reverb e comparação com a SPU continuam pendentes.
- Doze testes Linux aprovados, janela X11/software + PipeWire validada e música
  renderizada por 55 s com repetição. Build MinGW x64 desta alteração aprovado.
- Presets limitam compilação e testes a uma tarefa. Build Windows em contêiner
  limitado a 1536 MiB; esta revisão não foi executada no Wine.
- Detalhes, limitações e medições de RAM em [TITLE_FLOW.md](TITLE_FLOW.md).

### New Game — apresentação e nome do jogador

- Estados 11..16 adaptados em C: mensagens reais de 8 KiB, fonte, retrato, caixas,
  grade alfabética/símbolos, edição de até oito glifos e confirmação Yes/No.
- Os recursos de UI são carregados sob demanda, sem ler arquivos grandes inteiros.
- `--window-size 1280x720` e separação entre janela e coordenadas 320×240; a
  imagem original continua 4:3. Widescreen real e mods continuam etapas futuras.
- Próximo recorte: estado 17 em diante, aniversário e demais nomes do cadastro.
- Limites e validação em [NEW_GAME.md](NEW_GAME.md).

### Correções de UI e conclusão do cadastro

- Molduras corrigidas de BGR para RGB, conforme `prim_make_*`: tons bege originais.
- Ajuda e confirmações `0x0FF3` aparecem completas, sem digitação/blips.
- Estados 16..28 acrescentam aniversário, nomes de fazenda/cachorro, resumo,
  recomeço e fade. Numeração dos estados alinhada ao enum original.
- Quatorze testes Linux aprovados e percurso com dados reais até aceitar o cadastro.
- Janela X11/software + PipeWire 1280×720: 2210 frames, saída zero, pico RSS ~74 MiB.
- Build MinGW x64 aprovado e pacote Windows atualizado; execução em Windows real pendente.
- Próximo recorte: carregar a primeira cena após o cadastro e integrar o estado
  original da fazenda; ainda não há gameplay nativo.
- Evidências e limites atuais em [NEW_GAME.md](NEW_GAME.md).

### M4 parcial — geometria da fazenda

- Leitura limitada da área 0 em `00_gf.bin`: TMD principal com 294 objetos e 4.246
  primitivas, banco de texturas/paletas, sem carregar o arquivo inteiro.
- Parser por bytes little-endian e renderer C de matrizes/projeção inteiras,
  cores, UVs afins, dithering, semitransparência e ordenação por profundidade.
- `--scene-preview` mostra geometria real e permite mover a câmera; personagens,
  objetos dinâmicos e gameplay ainda não estão implementados.
- Janela Linux X11/software em 1280×720: 900 frames e quatro direções/reset
  conferidos, saída zero e pico RSS de cerca de 73 MiB.
- Configure/build e 16 testes CTest aprovados no Linux; executável/pacote MinGW
  Windows x64 atualizados em contêiner limitado a 1536 MiB, com uma tarefa.
  Sem execução no Wine nesta revisão; Windows real/MSVC continuam pendentes.
- A transição de New Game permanece na fronteira após o resumo. O original
  solicita o evento `0x74`; seus 78 registros e dependências foram identificados
  para o próximo recorte. Não substituir esse evento pela prévia estática.
- Fontes, formatos, comando de execução e limites em [SCENE_RENDERER.md](SCENE_RENDERER.md).

### M4 parcial — chegada com personagens e roteiro nativo

- Interpretador limitado dos registros originais de evento, com validação de
  offsets/saltos e limite de instruções por tick; handlers C do primeiro roteiro.
- Jogador e Thomas com sprites/poses, câmera acompanhando, ordem de desenho com
  o cenário, mensagens reais `0x76B..0x76F`, música 31 e fades.
- O cadastro entrega nomes/aniversário à chegada; o título e seu áudio são
  liberados antes de carregar a cena. `--intro-preview` permite testar esse recorte.
- A execução alcança o passo 74, solicitação da área 230; infância/evento `0x75`
  ainda não portados. Esse limite não é tratado como uma troca de cena concluída.
- Janela X11/software + PipeWire 1280×720: 2.984 frames, cinco mensagens e saída
  zero; pico RSS cerca de 79 MiB. Caminho completo do cadastro à primeira conversa
  também conferido: 3.413 frames, saída zero e cerca de 79 MiB; probe dummy adicional.
- Build/testes Linux aprovados; executável/pacote Windows recompilados com MinGW
  x64 em contêiner de 1536 MiB e uma tarefa. Windows real/MSVC continuam pendentes.
- Detalhes, evidências e próximos passos em [FARM_INTRO.md](FARM_INTRO.md).

### M4 parcial — cenário adicional e primeiros flashbacks

- Tabela original de 15 modelos: portas, caixa, objetos giratórios, posições,
  rotações e visibilidade inicial, com uma ordenação comum a terreno e atores.
- Campo inicial com mato, pedras, rochas, tocos e galhos. Layout 0 reproduzível;
  seleção aleatória dos três layouts e seu estado global ainda pendentes.
- Área 230, evento `0x75`: menino, cachorro, avô, caminhada, câmera, falas
  `0x770/0x771`, sépia original e música 32. Evento `0x76`: dez imagens e fades.
- Carregamento confirmado pelo host antes de avançar o comando. O próximo
  limite é área 231/evento `0x77`, encontro da infância; depois concluir o
  retorno da sequência e o primeiro trecho com controle e colisões.
- Configure/build e 17 CTest aprovados. Janela real X11/software + PipeWire:
  5.871 frames, saída zero e pico RSS de aproximadamente 80 MiB, sem swap.
  Probe dummy adicional com dados locais: 6.500 ticks e cerca de 22 MiB.
- Executável/pacote Windows x64 recompilados em contêiner com 1536 MiB e uma
  tarefa por vez. Windows real/MSVC e paridade PS1 ainda pendentes.
- Recursos, evidências e limitações em [FARM_INTRO.md](FARM_INTRO.md).

### M4 parcial — encontro, canção, despedida e retorno ao presente

- Áreas 231/232, atores 27/33, encontro `0x77` e canção com duas faixas XA
  extraídas. Áudio em fluxo com histórico por canal, misturado com SEQ/efeitos.
- Câmera com altura/distância/centro vertical e os 177 passos do recuo original.
- Despedida `0x78` na área 230, quatro atores e falas; retorno `0x79` na área 0,
  campo recarregado, música 34 e conversa completa com Thomas.
- Banco de ação real do jogador para o aceno da cabeça; restauração ao terminar.
  Cursores/sombras usam a ANI 24 e respeitam os flags dos atores. Corrigida a
  contagem do separador FFC/FFB nas páginas de texto.
- `--childhood-preview` testa diretamente o encontro. O fluxo normal percorre
  todos os eventos. O próximo limite é o interior da casa/área 80, no passo 31
  de `0x79`; depois integrar o controle livre e colisões reais.
- 18 CTest aprovados no Linux; probe da chegada ao limite com 6.792 ticks e
  saída zero. XA 4/5 comparados integralmente ao FFmpeg: PCM sem diferenças.
- Implementação e recursos em [FARM_INTRO.md](FARM_INTRO.md) e
  [XA_AUDIO.md](XA_AUDIO.md). O host continua sem executar o jogo completo.
- Janela real Linux: 8.330 frames em 139,80 s, percurso completo do encontro ao
  limite da casa, saída zero; imagens também conferidas por leitura direta SDL.
  Executável/pacote MinGW Windows x64 atualizados; execução Windows real/MSVC
  permanece pendente.

### Interior da casa e conclusão do roteiro de abertura

- Área 80 carrega modelos e texturas originais; móveis `0x41` usam o mesmo
  caminho validado de pacotes prelit dos modelos `0x42`.
- Evento `0x79` conclui a carga, posiciona o jogador conforme a tabela original,
  clareia a tela e emite o pedido de término. A casa permanece visível.
- 18 testes Linux aprovados e probe com recursos locais percorreu `0x74..0x79`
  em 6.856 ticks, sem fronteira pendente; captura direta SDL conferida.
- Build e pacote Windows x64 atualizados; execução Windows real pendente.
- Próximo recorte: integrar o pedido de término ao primeiro ciclo de controle,
  portar chão/colisões e validar caminhada e interação/saída pela porta.
  O bootstrap ainda não executa o jogo completo.

### Primeiro controle dentro da casa inicial

- `game/ground.*` consulta os triângulos/quads, flags e centros do cenário original.
- `game/house_control.*` libera caminhada/corrida ao encerrar `0x79`, com
  animações originais, bloqueios e deslizamento ao correr. Atalho `--house-preview`.
- Zona da porta reconhecida a partir dos dados: destino área 0, entrada
  `(-4464,0,4032)`. A porta pausa no limite explícito; transição ainda pendente.
- 19 testes Linux aprovados; nove percursos com recursos reais e teste de teclado
  em janela X11/software de 480 frames, saída zero. Windows x64 recompilado.
- Próximo recorte: transição completa casa → fazenda, eventos externos e retorno
  pela porta. Interações, relógio, inventário e sons dos passos ainda faltam.
- Escopo e evidências: [HOUSE_CONTROL.md](HOUSE_CONTROL.md). O porte continua parcial.

### Música e interações da casa inicial

- [x] Tema de primavera e passos com os bancos/parâmetros originais.
- [x] TV: quatro canais, programas por data e animação original no aparelho.
- [x] Calendário: navegação e mensagens dos eventos.
- [x] Baú: tampa, interface e cinco ferramentas; equipar, mover, trocar,
  organizar e consultar descrições, conservando os itens na sessão.
- [x] Livro/estante com texto e rolagem contínua original.
- [x] Mesa de opções: manual, tipos de controle, vibração SDL, estéreo/mono e relógio.
- [x] Menu da cama e sequência original de deitar, câmera, coberta e fades.
- [ ] Avanço do pernoite/manhã: plantações, animais, clima e eventos diários.
- [ ] Diário/save/load e troca de animais; entradas identificam seus limites.
- [ ] Comparação sistemática da apresentação e áudio com o PS1.
- [x] Transição inicial casa↔fazenda e leitura das placas externas.
- [ ] Paridade completa das portas e eventos externos.

Detalhes e controles em [HOUSE_INTERACTIONS.md](HOUSE_INTERACTIONS.md).
A meta do porte 1:1 permanece aberta; o jogo completo ainda não executa no host.

### Continuação da casa — opções e cama

- Configurações alteram o comportamento real: mapeamento do personagem,
  canais de áudio, relógio e pedido de vibração ao controle conectado.
- Morphs usam a topologia de saída e chaves de vértices validadas, sem modificar
  os recursos; isso permite animar a coberta com as duas entradas LINK da cama.
- Dormir para após a animação e o fade, antes das atualizações diárias ainda
  ausentes. A data não muda e o pedido de diário não produz um save parcial.
- Próximo recorte: transição casa↔fazenda e seus eventos, conforme a prioridade
  atual. Avanço diário/pernoite e salvamento completo dependem também do estado
  externo ainda não portado. A saída atual permanece no limite.
- Validação e diferenças conhecidas em [HOUSE_INTERACTIONS.md](HOUSE_INTERACTIONS.md).

### Distribuição do porte — instaladores

- [x] Extrator nativo em fluxo e preparação gráfica com seleção/arraste do `.bin`.
- [x] Importar somente recursos, sem executável de PS1, overlays MIPS ou disco agregado.
- [x] Dados por usuário, manifesto SHA-256, transação e cancelamento sem pasta parcial.
- [x] Pacotes AppImage, DEB, RPM e EXE; ícone e atalhos; sem Python/SDK/emulador no usuário final.
- [x] Workflow de artifacts Linux/Windows sem dados proprietários, com builds e CTest.
- [ ] Validar instalação e interface em Windows real; executar o workflow no GitHub.

Implementação, formatos e validação local em [INSTALLERS.md](INSTALLERS.md).
Isso organiza a distribuição do recorte atual, sem alterar o escopo parcial do jogo.

### Primeiro controle fora da casa

- [x] Zona de saída da casa carrega a fazenda/área 0 na entrada original.
- [x] Controle, chão estático, música e estado do inventário preservados.
- [x] Retorno inicial pela porta externa, com giro e som originais; temporização completa ainda pendente.
- [ ] Autowalk/blend original da transição, colisões do campo e eventos externos.

O exterior ainda é um recorte parcial. Limites e evidências em
[FARM_CONTROL.md](FARM_CONTROL.md).

### Retorno da fazenda e leitura externa

- Porta da casa: zona/direção/Cross, giro com aceleração original e som 0x16.
- Volta à área 80 na entrada serializada, sem perder inventário/opções; saída novamente validada.
- Placas externas de tipo 16 usam seus textos importados.
- Próximo recorte: colisões dos ocupantes do campo e uso de ferramentas.
- O ciclo completo de portas (hold, fechamento e autowalk) ainda requer paridade.

### Correções das placas, espera da porta e volume

- [x] Banco 2 para placas/estante; TV e menus continuam usando o banco 3.
- [x] Nome do cachorro cadastrado e estoques da sessão nos textos das placas.
- [x] Respeitar direção livre nas placas cujas zonas usam `FACING_ANY`.
- [x] Jogador parado durante a abertura da porta; caminhar somente após abrir.
- [x] Música com volume 80 dentro e 100 fora, inclusive no tick da troca de área.
- [x] 21 CTests Linux, probe com as cinco placas/estante/porta e build Windows x64.
- [ ] Colisões dos ocupantes do campo, uso de ferramentas e atualizações diárias.

Recursos importados permanecem fora do Git. O porte continua parcial e não
substitui o build PS1 nem sua validação SHA-1. Evidências e diferenças de
apresentação restantes em [FARM_CONTROL.md](FARM_CONTROL.md).

### Fazenda — conclusão funcional solicitada

- [x] Fechamento de diálogos em um único ciclo, sem reexibir a moldura.
- [x] Validar a proporção real de amplitude da música: casa 80% do exterior.
- [x] Consultas originais de chão, retângulos FarmChoice e regras dos ocupantes do campo.
- [x] Bloqueio/deslizamento e salto conforme o tipo do obstáculo.
- [x] Estado mutável dos 1125 terrenos, conservado nas entradas/saídas.
- [x] Cinco ferramentas iniciais: ANI/sons, efeito após a animação, arar, regar e limpar.
- [x] Abastecer o regador com 100 unidades e mostrar a contagem real na descrição.
- [x] Stamina/fadiga/experiência na sessão e animação original por sobrecarga.
- [x] Efeitos de quebra originais no campo (fila limitada e pausa).
- [ ] Ferramentas melhoradas e áreas ampliadas.
- [x] Pegar/carregar/lançar objetos soltos, guardar itens e caixa da fazenda.
- [ ] Portas e interiores das instalações da fazenda.
- [ ] Relógio, animais, eventos e atualizações diárias ligados ao estado da fazenda.
- [ ] Comparação reproduzível com PS1 e execução em Windows real.

Esta lista descreve a conclusão solicitada; itens pendentes não são considerados
implementados apenas por terem representação visual no cenário.

Validação: 22 CTests Linux, comparação do PCM 80/100, probe com dados locais
para 1125 quads/ferramentas/refil/sobrecarga/ida e volta, e build MinGW x64.
Detalhes e limites da fazenda em [FARM_CONTROL.md](FARM_CONTROL.md).

### Menus da fazenda, mochila e áudio de ferramentas

- [x] Regador libera sua própria voz no fim da ação e ao abrir menus.
- [x] Identificadores de áudio impedem que um release antigo interrompa outra voz.
- [x] Animações de quebra para pedras, galhos, madeira e mato; fila de 25 efeitos.
- [x] Start: resumo, envios, ajudantes, contas e experiência, com o pack original.
- [x] Pausa congela o mundo e efeitos; música continua.
- [x] Triângulo: mochila com os slots desbloqueados, ajuda e descrições originais.
- [x] Mover/trocar/equipar e empilhar sementes, preservando o inventário do baú.
- [x] R1 troca ferramentas; R2 guarda/retira itens; teclado com R1/R2 separados.
- [x] Imagens de itens com cache limitado e centralização pelos recortes originais.
- [x] Coleta de objetos soltos, carregamento, lançamento e caixa da fazenda.
- [ ] Colocação de instalações e colheita.
- [ ] Dados vivos de animais, ajudantes contratados e economia diária.
- [ ] Paridade visual completa de todos os menus e testes em Windows real.

Controles, fontes e validação em [FARM_MENUS.md](FARM_MENUS.md).
Objetos soltos já usam a mão/mochila compartilhadas e o fluxo de lançamento.
Entrega e contabilização descritas em [SHIPPING.md](SHIPPING.md).

### Coleta, mochila e descarte

- [x] Recolher pedra pequena, galho e mato do campo, removendo imagem/colisão.
- [x] Carregar com as ANIs originais de coleta, repouso, caminhada e corrida.
- [x] Guardar/retirar pela mochila compartilhada, respeitando capacidade.
- [x] Lançar e descartar objetos soltos, com animação, som e três slots de voo.
- [x] Bloquear comandos durante a ação e congelar o voo nos menus.
- [x] 24 CTests e probe de coleta → mochila → retirada → lançamento.
- [x] Consulta real do chão no voo e entrega à caixa da fazenda.
- [ ] Colocação de instalações, colheitas e demais reações de aterrissagem.

Limites e controles em [FARM_CARRY.md](FARM_CARRY.md).
Entrega e economia em [SHIPPING.md](SHIPPING.md).

### Ajustes de carregamento e primeiro encontro matinal

- [x] Deslocamentos originais de itens carregados por direção e corrida.
- [x] Pedra/galho/madeira com classe de campo retornam a célula livre no plantio.
- [x] Preservar solo, ocupantes e colisão; mato segue o descarte.
- [x] Primeiro trecho do evento 12: Thomas, textos, nome e escolha Sim/Não.
- [x] Gatilho uma vez por sessão.
- [x] Continuar o passeio aceito a partir do cursor 15 e Zack a partir do 147.

### Passeio de Thomas e apresentação de Zack — 2026-10-23

- [x] Passeio aceito completo: estrada 32, áreas 36/40/48/52 e retorno à fazenda.
- [x] Geometria/texturas reais, caminhadas, poses, Ellen e mensagens 64..77.
- [x] Recusa com falas 78/79 e continuação comum com Zack.
- [x] Subrotinas do roteiro sem modificar os bytes importados; retorno validado.
- [x] Trocas de retrato na mesma mensagem e esperas pelos bancos 105/159.
- [x] Apresentação de Zack, câmera da caixa, marcador e saída original.
- [x] Controle liberado no término do evento; mochila/terreno conservados.
- [x] Acréscimo das duas horas do passeio ao horário da sessão.
- [x] 24 CTests Linux, percurso Sim/Não com dados locais, janela X11/software e cross-build MinGW x64.
- [x] Exploração livre das regiões externas da cidade.
- [ ] Rotinas de moradores e interiores.
- [x] Entrega à caixa da fazenda e pagamento; produtores e demais caixas ainda pendentes.
- [ ] Persistência em saves, comparação com PS1 e execução em Windows real.

Fontes, reprodução e limites em [FIRST_MORNING.md](FIRST_MORNING.md).
Entrega à caixa e integração da economia descritas em [SHIPPING.md](SHIPPING.md).
Próximo recorte: cultivos e demais atualizações diárias.
O bootstrap nativo de diagnóstico não executa o jogo; o porte continua parcial.

### Aproximação de Thomas, escadas e relógio — 2026-10-23

- [x] Aproximação de Thomas pela entrada norte antes da primeira conversa.
- [x] Jogador/atores assentados no chão durante os eventos, incluindo as escadas da estrada 32.
- [x] Relógio de 15 ticks por minuto; pausa em interiores, menus, eventos e fades.
- [x] Virada da data às 06:00, semana, 30 dias/estação e limite de ano original.
- [x] Amanhecer/dia/entardecer/noite com tabelas originais, blend subtrativo e interpolação de 256 ticks.
- [x] Faixas sazonais e noturna, proporção de volume e silêncio no inverno à noite.
- [x] Hora/minutos, calendário e TV atualizados pelo mesmo estado de tempo.
- [x] Sono até a manhã, recuperação de stamina/fadiga e retorno ao controle.
- [x] Seções sazonais da casa/fazenda, solo de inverno e uploads de textura persistentes.
- [x] Secagem do terreno, mato de inverno e conservação de mochila/experiência/descartes.
- [x] 25 CTests, dois percursos com dados locais, janela X11/software e compilação MinGW x64.
- [ ] Clima, cultivos, animais, festivais e demais callbacks diários.
- [ ] Saves, comparação de paridade com PS1 e execução em Windows real.

Regras, reprodução e limites em [CLOCK.md](CLOCK.md). A aproximação antes
do bloco do evento 12 é uma preparação nativa solicitada para a visita;
o script importado continua intacto. O bootstrap de diagnóstico não executa o jogo.

### Direção na conversa e caixa de venda — 2026-10-23

- [x] Jogador vira para Thomas ao terminar o deslocamento da porta e permanece assim na primeira fala.
- [x] Altura dos objetos lançados consultada no terreno real.
- [x] Entrega reconhecida pelo bit 0x80 da superfície original, nas quatro estações.
- [x] Preços originais, 25 grupos de produtos, limite de 999 e total de peixes enviados separado.
- [x] Valor pendente, pagamento único às 17h58, datas sem coleta e entregas após o horário.
- [x] Saldo/receitas no estado compartilhado do menu; limite de carteira original.
- [x] Eventos de economia em ordem durante sono; histórico de contas às 05h59 do último dia da estação.
- [x] 26 CTests Linux, três percursos com dados locais, passeio X11 e cross-build MinGW x64.
- [ ] Cultivos, crescimento diário e geração dos produtos; cesta, cavalo e caixas de interiores.
- [ ] Rotina de Zack, demais moradores, clima e saves.

Reprodução e limites em [SHIPPING.md](SHIPPING.md). O campo inicial continua
com os objetos soltos já suportados; os produtos do teste não são concedidos
em jogo. O pedido seguinte priorizou as demais regiões do mapa.

### Regiões externas do mapa — 2026-10-23

- [x] Fazenda e 14 outras regiões externas conectadas por zonas originais.
- [x] 34 passagens direcionais, fade e retorno ao controle na chegada.
- [x] Resolução das quatro seções sazonais de cada região.
- [x] Caminhada/corrida no terreno real; altura de chegada, escadas e encostas.
- [x] Limites originais da câmera em exploração livre.
- [x] Leitura das placas do vilarejo/trilhas, incluindo nome cadastrado da fazenda.
- [x] Música regional, exceções de volume e pitch wheel real das SEQs da praia.
- [x] Inventário, terreno e remessas conservados; retorno sem repetir Thomas.
- [x] 27 CTests e quatro percursos de integração com dados locais no Linux.
- [ ] Interiores, horários, moradores e eventos dos estabelecimentos.
- [ ] Minas, coleta silvestre, pesca, banhos e blits de textura animada.

Detalhes e reprodução em [WORLD_MAP.md](WORLD_MAP.md). Nesta etapa, por pedido
do usuário, validar somente no Linux local; manter o código compartilhado
compatível com Windows, sem cross-build a cada entrega. Próximo recorte do
mapa: interiores e suas regras de entrada, antes dos sistemas próprios de cada local.

### Interiores iniciais da fazenda — 2026-10-23

- [x] Estábulo 92, celeiro 96 e galinheiro 112, nas quatro estações.
- [x] Entrada por Z/Cross, direção e altura da zona; saída interna ao caminhar.
- [x] Giro por direção, rotação inicial e suporte a modelos de portas duplas.
- [x] Relógio pausado nos interiores e estado conservado no retorno.
- [x] Atalhos de teste `--area-preview 92/96/112`.
- [x] 27 CTests e cinco percursos de integração revalidados no Linux local.
- [ ] Alimentação, animais e demais operações internas.
- [ ] Portas da vila: regras de presença, afinidade, calendário e horário.

Fontes, reprodução e limites em [FARM_INTERIORS.md](FARM_INTERIORS.md).
As portas da fazenda não têm bloqueios de horário na tabela original; as
regras dos estabelecimentos da vila precisam de seus estados reais antes
da liberação. Windows continua como alvo do código, sem recompilação nesta etapa.
