<div align="center">

# Harvest Moon: Back to Nature — PC Port

**Um projeto pessoal para portar Harvest Moon: Back to Nature (PlayStation) nativamente para Linux e Windows.**

*Baseado na decompilação [hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp), de coalslav.*

> **Status: título, cadastro, chegada e sequência da infância (M3/M4 parcial).** Já é possível caminhar e correr na casa inicial; Sistema da Fazendo Plantação ja implementada. 

---


<img src="./docs/prints/animacao_estavel.gif" width="300">

**Port ainda em Progresso...**

## Instaladores para PC

Os pacotes AppImage, DEB, RPM e EXE distribuem somente o porte e seu extrator nativo. No primeiro uso, selecione seu `.bin`; apenas os recursos são extraídos para a pasta do usuário. Python, SDK e emulador não são necessários. Veja [instalação e GitHub Actions](docs/port/INSTALLERS.md). O jogo completo ainda não está portado.

## Sobre o projeto





A proposta é estudar e adaptar o código C reconstruído de **Harvest Moon: Back to Nature** para executar em plataformas modernas sem depender de um emulador de PlayStation durante a execução. Pretendo preservar a experiência do jogo original e, futuramente, explorar melhorias e suporte a mods.

Este é um projeto pessoal, desenvolvido por interesse em programação, engenharia reversa e no próprio jogo. O port não é oficial e não possui afiliação com os titulares de Harvest Moon nem com os autores da decompilação.

</div>

## Sumário

- [Estado atual](#estado-atual)
- [Objetivos](#objetivos)
- [Arquitetura planejada](#arquitetura-planejada)
- [Plataformas](#plataformas)
- [Documentação](#documentação)
- [Compilação da decompilação original](#compilação-da-decompilação-original)
- [Origem e créditos](#origem-e-créditos)
- [Licenciamento e dados do jogo](#licenciamento-e-dados-do-jogo)

## Estado atual

O repositório contém a **decompilação correspondente à versão americana de PlayStation, `SLUS_011.15`**. O projeto de origem reconstrói o executável e seus 13 overlays com correspondência byte a byte.

**Implementado:** janela SDL3 de área lógica 320 × 240, logo Natsume, introdução gradual do título, grama e sprites animados, e estados 0..9 do menu original adaptados em C. New Game já mostra diálogos/retrato e permite completar nomes, aniversário e resumo do cadastro. Música e sons dos botões usam VAB/SEQ dos dados extraídos, com saída SDL3. A área do jogo não contém barras ou textos de diagnóstico. Enter/Start encerra a introdução; outro Enter abre o menu. Setas/D-pad alternam e Z/face inferior confirma. Escape fecha e F11 alterna tela cheia. Linux validado; há build Windows x64 e CI MSVC preparada. Os presets compilam e testam uma tarefa por vez.

**Dados do jogo:** o executável lê o bloco da tela de título em `data/A_FILE/00_gf.bin`, sem consultar o disco original. Há importação/verificação da pasta e reconstrução PS1 a partir dela. Veja o [guia dos dados](docs/port/GAME_DATA.md) e os [formatos implementados](docs/port/ASSET_FORMATS.md).

**Cenário 3D — M4 parcial:** `./build/native-debug/port/hmbtn_native --scene-preview` mostra o modelo base da fazenda com texturas originais, projeção e rasterização em C nativo. Setas movem a câmera de diagnóstico e Enter restaura a vista. É uma prévia sem personagens/gameplay, separada do fluxo de New Game. Veja [SCENE_RENDERER.md](docs/port/SCENE_RENDERER.md).

**Chegada à fazenda:** aceitar o resumo do cadastro agora inicia o roteiro original, com jogador, Thomas, diálogos, câmera e música. `./build/native-debug/port/hmbtn_native --intro-preview` abre essa sequência diretamente. A cena inclui portas, caixa de venda, mato, pedras e galhos. A sequência continua pelo encontro com a menina, canção, despedida e retorno a Thomas (eventos `0x75..0x79`), além das dez imagens das lembranças. A sequência entra na casa/área 80, posiciona o jogador junto à porta e encerra o roteiro da abertura. `--childhood-preview` abre diretamente o encontro. Veja [FARM_INTRO.md](docs/port/FARM_INTRO.md).

**Controle na casa:** `./build/native-debug/port/hmbtn_native --house-preview` abre a casa inicial diretamente. Setas/D-pad caminham; X/Circle corre para a frente. O chão e os bloqueios usam polígonos e flags do cenário original. A porta pausa em um limite explícito; a transição para a fazenda continua pendente. Z/Cross interage com TV, calendário, baú e livro/estante; música e passos estão ativos. As opções alteram controle, vibração, estéreo/mono e relógio. A cama executa o menu, animação/coberta e fade, parando antes das atualizações diárias; avanço do dia e diário/save/load permanecem pendentes. Veja [HOUSE_CONTROL.md](docs/port/HOUSE_CONTROL.md) e [HOUSE_INTERACTIONS.md](docs/port/HOUSE_INTERACTIONS.md).

**Ainda não foram implementados:** gameplay completo, saves, colisões gerais, objetos dinâmicos da fazenda, áudio completo da SPU/reverb, mods. O bootstrap não executa o jogo completo. Continue/demo param na fronteira registrada no terminal; Start/Z/X retorna ao menu. A abertura e o áudio ainda exigem comparação com o PS1 antes de afirmar fidelidade total. O recorte foi adaptado em `port/`; os módulos completos de `src/` ainda não são compilados no host. O build original de **PlayStation** continua disponível por padrão. O `.exe` Windows local foi recompilado com MinGW x64 incluindo sequência da infância e áudio XA em fluxo; Windows real/MSVC continuam pendentes.

## Começar no Linux e Windows

Requisitos: CMake 3.20+, compilador C11, Ninja e SDL3 de desenvolvimento. Se SDL3 não estiver instalado, o CMake baixa a versão fixada e confere seu hash; o primeiro build precisa de internet. No Windows, execute em um Developer PowerShell do Visual Studio com as ferramentas C++ e Ninja disponíveis. Veja também o fluxo offline e sem Ninja no [guia de compilação](docs/port/BUILDING.md).

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

Execute `./build/native-debug/port/hmbtn_native` no Linux ou `.\build\native-debug\port\hmbtn_native.exe` no Windows. O programa procura `data/` junto ao executável e no layout do projeto; `--data DIR` é prioritário. Enter abre o menu, setas selecionam e Enter/Z confirma. X/Backspace retorna do aviso de destino ainda não portado. `--window-test` abre o diagnóstico sem dados e `--diagnostics` verifica somente o console, sem display. Veja também [Windows e compilação cruzada](docs/port/WINDOWS.md).

## Objetivos

- Portar a lógica do jogo para uma aplicação nativa em Linux e Windows.
- Criar uma camada de compatibilidade para gráficos, áudio, entrada, temporização, memória e arquivos do PS1.
- Manter, na medida do possível, o código decompilado isolado das adaptações multiplataforma.
- Preservar uma forma de acompanhar as atualizações da decompilação original.
- Avaliar suporte a mods e melhorias apenas depois que o jogo básico estiver funcionando.

## Arquitetura planejada

| Camada | Responsabilidade | Estado |
|---|---|---|
| `src/` | Lógica decompilada do jogo | Base existente |
| `port/core/` | Aplicação host e formatos de dados PS1 | Base compilável |
| `port/game/` | Fluxo do título e comandos de evento adaptados | Menu, cadastro e primeiro evento da fazenda |
| `port/tests/` | Formatos, importação, janela e eventos | Implementado |
| `port/graphics/` | Bancos, animações, modelos TMD e sprites projetados | Título e chegada implementados parcialmente |
| `port/audio/` | Sampler VAB/ADPCM e sequenciador SEQ do título | Implementação parcial; reverb pendente |
| `port/input/` | Atalhos e tradução para botões PS1 | Teclado/gamepad implementados |
| `port/filesystem/` | Leitura de intervalos de recursos; futuros saves | Leitura implementada |
| `port/platform/` | Janela/renderizador SDL3 | Implementado |

**SDL3 foi integrado à infraestrutura de janela, eventos e apresentação.** A tradução dos gráficos PS1 e a adaptação da lógica ainda dependem do estudo de PSY-Q, endereços fixos, overlays e estruturas específicas do console. Veja os marcos no [roteiro](docs/port/ROADMAP.md).

O build original continua organizado pelos `CMakeLists.txt` e pelo toolchain PS1 existentes. O build nativo usa `-DHMBTN_BUILD_NATIVE=ON` e diretório separado, selecionado antes do toolchain PS1. Sem essa opção, o fluxo PS1 permanece selecionado.

## Plataformas

| Plataforma | Objetivo | Estado |
|---|---|---|
| Fedora / Linux | Primeira plataforma de desenvolvimento | Janela validada em Wayland e X11 |
| Windows | Build nativo de PC | MinGW x64 e Wine verificados; Windows real/MSVC pendentes |


## Documentação

- [Compilação Linux e Windows](docs/port/BUILDING.md)
- [Build Windows e pacote portátil](docs/port/WINDOWS.md)
- [Recorte do fluxo do título](docs/port/TITLE_FLOW.md)
- [Auditoria inicial do código PS1](docs/port/INITIAL_AUDIT.md)
- [Plano técnico de desenvolvimento](docs/port/ROADMAP.md)
- [Arquitetura inicial](docs/port/ARCHITECTURE.md)
- [Guia de contribuição e sincronização com upstream](CONTRIBUTING.md)
- [Decompilação original](https://github.com/coalslav/hmbtn-decomp)

## Compilação da decompilação original

O comando abaixo **compila para PlayStation 1**, não para Linux ou Windows. Requer Docker e as ferramentas instaladas pela imagem do projeto de origem:

```bash
docker build -t hmbtn-build .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src:ro -v /tmp/hmbtn-build:/build \
  hmbtn-build bash -c 'cmake -S /src -B /build -G Ninja && cmake --build /build --target check'
```

A verificação `check` confere os hashes dos 14 binários originais. **Quando o port ou alterações de gameplay começarem, os hashes originais não serão mais uma verificação válida para os binários modificados.** Para reconstruir a imagem de disco no fluxo original é necessária uma cópia própria do disco; consulte a documentação upstream.


![HMBTN — captura do projeto 1](./docs/prints/Screenshot_20261024_233235.png)


![HMBTN — captura do projeto 1](./docs/prints/Screenshot_20261024_233706.png)


## Origem e créditos

Este repositório é um **fork** de [coalslav/hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp). A decompilação e sua estrutura inicial foram produzidas pelo projeto original. O desenvolvimento do port será realizado separadamente e o histórico de autoria será preservado.

Agradecimentos aos autores da decompilação, aos criadores das ferramentas utilizadas e aos desenvolvedores do jogo original.

## Licenciamento e dados do jogo

A licença [CC0 1.0](LICENSE) do upstream cobre **somente o trabalho próprio do repositório original**. Componentes como `tools/maspsx` podem ter licença própria. O jogo, seus gráficos, músicas, recursos e o SDK PSY-Q pertencem a seus respectivos titulares e não são liberados pela CC0.

O port não deve publicar cópias do jogo ou dados proprietários sem autorização. Pretende-se estudar um processo no qual cada usuário fornece os recursos da sua própria cópia do jogo. A distribuição pública de um port com tais recursos exige análise dos direitos aplicáveis.

Música, passos, TV, calendário, baú de ferramentas e livro/estante estão descritos em [HOUSE_INTERACTIONS.md](docs/port/HOUSE_INTERACTIONS.md).
