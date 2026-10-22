# Compilação da base de PC

## Requisitos

- CMake 3.20 ou superior.
- Compilador C11: GCC no Linux ou MSVC com suporte C11 no Windows.
- Ninja para os presets; opcional quando usar o gerador padrão.
- SDL3 3.2+ de desenvolvimento, ou acesso à internet para baixar a versão fixada.

O CMake procura SDL3 instalado. Se não encontrar, `HMBTN_FETCH_SDL3=ON` (padrão)
baixa SDL3 3.4.18 do site oficial, confere SHA-256 e compila uma biblioteca estática
em `build/native-*/_deps/`. Nenhum SDK PSY-Q, disco ou dado original é necessário
para compilar ou abrir `--window-test`. O menu padrão precisa de `data/` importada
e executa o recorte de estados 0..9 do título; ainda não há cadastro/saves/gameplay.

O primeiro build também compila SDL3 e leva mais tempo. No Linux, o build de SDL
precisa dos arquivos de desenvolvimento de um backend de janela (Wayland ou X11)
e suas dependências. Erros de detecção são apresentados na configuração.
Para bordas e botões da janela em Wayland/GNOME ou Weston, inclua também os
arquivos de desenvolvimento de libdecor. Se SDL for compilado sem eles, pode-se
usar X11/XWayland com `SDL_VIDEO_DRIVER=x11 ./build/native-debug/port/hmbtn_native`.

Para usar somente uma instalação local, desative o download e, se necessário,
informe a pasta que contém `SDL3Config.cmake`:

```bash
cmake --preset native-debug -DHMBTN_FETCH_SDL3=OFF -DSDL3_DIR=/caminho/lib/cmake/SDL3
```

Para compilar offline a partir de fontes SDL3 já extraídos:

```bash
cmake --preset native-debug -DFETCHCONTENT_SOURCE_DIR_SDL3=/caminho/SDL3-3.4.18
```

Referência: [integração CMake oficial do SDL3](https://wiki.libsdl.org/SDL3/README-cmake).

## Linux

Com compilador, CMake e Ninja disponíveis:

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
./build/native-debug/port/hmbtn_native
```

Para Release, substitua `native-debug` por `native-release` nos comandos e no caminho.

Sem argumentos, o programa abre uma janela de 960 × 720 e apresenta uma área lógica
de 320 × 240. Escape fecha e F11 alterna tela cheia. A prévia lê os recursos da tela
de título em `data/`, desenha fundo/logotipo e reproduz três sequências animadas.
Enter/Start abre o menu, setas/D-pad alternam e Enter/Z confirma. Os destinos de
cadastro/saves/fazenda ainda não estão portados e mostram um aviso com retorno.
A pasta é localizada junto ao executável ou no projeto; `--data DIR` é prioritário.

Opções de desenvolvimento:

```bash
./build/native-debug/port/hmbtn_native --diagnostics
./build/native-debug/port/hmbtn_native --window-test --frames 3
./build/native-debug/port/hmbtn_native --frames 120 --screenshot build/native-debug/window.bmp
```

O primeiro comando termina sem abrir janela. O segundo testa a janela sem dados do
jogo. O terceiro abre o título parcial, salva o último frame em BMP e encerra após
120 apresentações. A opção `--frames` aceita 1 a 1000000.
Sem display, pode-se usar `SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software`;
isso testa o ciclo e o desenho, mas não comprova apresentação no desktop.

## Windows

Instale as ferramentas de desenvolvimento C++ do Visual Studio, CMake e Ninja. Abra o Developer PowerShell do Visual Studio para que MSVC esteja no PATH:

```powershell
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
.\build\native-debug\port\hmbtn_native.exe
```

Com o gerador padrão do Visual Studio, sem exigir Ninja:

```powershell
cmake -S . -B build/native-vs -DHMBTN_BUILD_NATIVE=ON
cmake --build build/native-vs --config Debug
ctest --test-dir build/native-vs -C Debug --output-on-failure
.\build\native-vs\port\Debug\hmbtn_native.exe
```

Os caminhos acima correspondem a Ninja e Visual Studio, respectivamente. A CI usa o gerador padrão de cada runner e testa Debug e Release. Os testes de janela usam o backend dummy e têm timeout. O build MinGW x64 foi testado via Wine; Windows real e MSVC permanecem pendentes. Veja [WINDOWS.md](WINDOWS.md). Python 3.10+ é opcional para o executável, mas necessário para o teste do importador de dados.

## Separação do PS1

Sem `HMBTN_BUILD_NATIVE=ON`, o CMake raiz mantém o fluxo PS1 existente. Use `build/psx` no ambiente Docker documentado no README e `build/native-*` para host. Não reutilize um diretório de build entre toolchains; o CMake guarda o compilador no cache.

`src/`, `disc/`, `sdk/`, os scripts originais e o workflow PS1 foram preservados. Os hashes originais verificam apenas os binários PS1, não o executável nativo. O build PS1 foi posteriormente executado no Docker do projeto: o executável e os 13 overlays passaram nos hashes originais.

Também é possível importar o disco uma vez e reconstruir usando apenas a pasta
extraída, com `HMBTN_EXTRACTED_DATA_DIR`. Veja [instalação dos dados](GAME_DATA.md).
O caminho nativo de leitura e composição do bloco da tela de título está descrito
em [ASSET_FORMATS.md](ASSET_FORMATS.md).

## Próximo trabalho

Continuar M3 do [roteiro](ROADMAP.md): portar o diálogo/cadastro de New Game e
demais estados/serviços. O recorte atual está em [TITLE_FLOW.md](TITLE_FLOW.md);
o marco completo e a comparação sistemática com PS1 ainda estão pendentes.

## Uso de memória

Os presets nativos usam `jobs: 1` e testes sequenciais. A recompilação incremental
é pequena; o primeiro build da SDL3 recompila a dependência. Evite builds
Linux/Windows simultâneos. Nos contêineres de compilação cruzada use também
`--memory 1536m --memory-swap 1536m --cpus 1` e `--parallel 1`, conforme
[WINDOWS.md](WINDOWS.md). Os recursos da abertura são lidos em intervalos pequenos.
