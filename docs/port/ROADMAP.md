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
resumo. A transição para a fazenda, saves e demo continuam pendentes. Detalhes em [TITLE_FLOW.md](TITLE_FLOW.md).

- [x] Adaptar logo Natsume e entrada gradual de título/copyright/texto antes das opções.
- [x] Adaptar as duas camadas móveis de grama, personagens e flores.
- [x] Reproduzir música SEQ e efeitos VAB/ADPCM do título em SDL3.
- [x] Remover barras/textos de diagnóstico da área do jogo.
- [x] Portar apresentação do prefeito, pergunta pelo nome, fonte e retrato originais.
- [x] Portar grade e confirmação do nome do jogador, com limite de oito glifos.
- [x] Validar janela 1280×720 mantendo conteúdo 4:3 e capturas com margens.
- [x] Portar aniversário, nome da fazenda, nome do cachorro e resumo do cadastro.
- [ ] Portar a transição do cadastro para a primeira cena/fazenda.
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

- [ ] Adaptar operações de geometria/GTE e ponto fixo usadas pelo cenário escolhido.
- [ ] Implementar as primitivas e ordenação gráfica exigidas nesse recorte.
- [ ] Carregar um cenário, posicionar câmera e desenhar chão/objetos/personagem.
- [ ] Integrar movimento original, animação, colisões e uma transição de porta.
- [ ] Comparar posição, câmera e colisões em uma sequência reproduzível de entradas.

Entrega: personagem controlável em um cenário real. Não ampliar para todo o jogo
antes de verificar esse caminho completo.

## M5 — Sistemas do jogo

- [ ] Relógio, calendário, clima e transições de dia.
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
