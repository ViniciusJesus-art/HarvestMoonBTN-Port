<div align="center">

# Harvest Moon: Back to Nature — PC Port

**Um projeto pessoal para portar Harvest Moon: Back to Nature (PlayStation) nativamente para Linux, Windows e Android.**

*Baseado na decompilação [hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp), de coalslav.*

> **Status: fase inicial de planejamento e preparação.** Ainda não existe uma versão nativa jogável.

---

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

**Ainda não foram implementados:** runtime nativo para PC, renderização nativa, integração com SDL3, sistemas de mods e builds de Windows ou Android. A compilação existente continua sendo a do **PlayStation**, não uma compilação nativa.

## Objetivos

- Portar a lógica do jogo para uma aplicação nativa em Linux, Windows e Android.
- Criar uma camada de compatibilidade para gráficos, áudio, entrada, temporização, memória e arquivos do PS1.
- Manter, na medida do possível, o código decompilado isolado das adaptações multiplataforma.
- Preservar uma forma de acompanhar as atualizações da decompilação original.
- Avaliar suporte a mods e melhorias apenas depois que o jogo básico estiver funcionando.

## Arquitetura planejada

| Camada | Responsabilidade | Estado |
|---|---|---|
| `src/` | Lógica decompilada do jogo | Base existente |
| `port/core/` | Integração e runtime nativo | Planejado |
| `port/graphics/` | Adaptação gráfica | Planejado |
| `port/audio/` | Adaptação de áudio | Planejado |
| `port/input/` | Controles | Planejado |
| `port/filesystem/` | Recursos, leitura e saves | Planejado |
| `port/platform/` | Integração específica de cada plataforma | Planejado |

**SDL3 é uma candidata de infraestrutura, não uma dependência implementada ou decisão técnica definitiva.** Antes do port, será necessário estudar os usos de PSY-Q, endereços fixos de memória, overlays e estruturas específicas do PS1.

O build original continua organizado pelos `CMakeLists.txt` e pelo toolchain PS1 existentes. A configuração de build nativa deverá ser criada separadamente para evitar regressões.

## Plataformas

| Plataforma | Objetivo | Estado |
|---|---|---|
| Fedora / Linux | Primeira plataforma de desenvolvimento | Não iniciado |
| Windows | Port nativo após estabilizar o Linux | Não iniciado |
| Android | Port com NDK, ciclo de vida Android e controles por toque | Não iniciado |

## Documentação

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

## Origem e créditos

Este repositório é um **fork** de [coalslav/hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp). A decompilação e sua estrutura inicial foram produzidas pelo projeto original. O desenvolvimento do port será realizado separadamente e o histórico de autoria será preservado.

Agradecimentos aos autores da decompilação, aos criadores das ferramentas utilizadas e aos desenvolvedores do jogo original.

## Licenciamento e dados do jogo

A licença [CC0 1.0](LICENSE) do upstream cobre **somente o trabalho próprio do repositório original**. Componentes como `tools/maspsx` podem ter licença própria. O jogo, seus gráficos, músicas, recursos e o SDK PSY-Q pertencem a seus respectivos titulares e não são liberados pela CC0.

O port não deve publicar cópias do jogo ou dados proprietários sem autorização. Pretende-se estudar um processo no qual cada usuário fornece os recursos da sua própria cópia do jogo. A distribuição pública de um port com tais recursos exige análise dos direitos aplicáveis.
