# Build Windows x64

O mesmo C nativo e as mesmas interfaces SDL3 são usados em Linux e Windows.
O `.exe` atual implementa o recorte de menu descrito em [TITLE_FLOW.md](TITLE_FLOW.md),
sem gameplay. Recursos devem vir da cópia do usuário e não estão no pacote.

## MSVC no Windows

Abra Developer PowerShell do Visual Studio com CMake, Ninja e as ferramentas C++:

```powershell
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
cmake --install build/native-debug --prefix build/package-windows
```

Com o gerador Visual Studio (sem Ninja):

```powershell
cmake -S . -B build/native-vs -DHMBTN_BUILD_NATIVE=ON
cmake --build build/native-vs --config Debug --parallel 1
ctest --test-dir build/native-vs -C Debug --output-on-failure
cmake --install build/native-vs --config Debug --prefix build/package-windows
```

Coloque a pasta `data/` importada ao lado de `hmbtn_native.exe`, ou indique:

```powershell
.\build\package-windows\hmbtn_native.exe --data "D:\Jogos\Harvest Moon\data"
```

Python 3.10+ é necessário para o importador e os testes Python, não para executar
o `.exe`. O build inicial baixa SDL3 quando não há pacote de desenvolvimento
instalado; detalhes/offline em [BUILDING.md](BUILDING.md).

## Compilação cruzada no Linux

O Dockerfile em `port/platform/` instala MinGW em uma imagem separada, sem SDK PS1.
Este exemplo reutiliza os fontes SDL3 já extraídos em `build/dependencies/sdl3-source`:

```bash
docker build -t hmbtn-windows-build -f port/platform/Dockerfile.windows-build .
mkdir -p build/native-windows-x64
docker run --rm --user "$(id -u):$(id -g)" --network none --memory 1536m --memory-swap 1536m --cpus 1 \
  -v "$PWD:/src:ro" -v "$PWD/build/native-windows-x64:/build:rw" hmbtn-windows-build \
  cmake -S /src -B /build -G Ninja -DHMBTN_BUILD_NATIVE=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=/src/port/platform/mingw64-toolchain.cmake \
  -DFETCHCONTENT_SOURCE_DIR_SDL3=/src/build/dependencies/sdl3-source
docker run --rm --user "$(id -u):$(id -g)" --network none --memory 1536m --memory-swap 1536m --cpus 1 \
  -v "$PWD:/src:ro" -v "$PWD/build/native-windows-x64:/build:rw" hmbtn-windows-build \
  cmake --build /build --parallel 1
docker run --rm --user "$(id -u):$(id -g)" --network none --memory 1536m --memory-swap 1536m --cpus 1 \
  -v "$PWD:/src:ro" -v "$PWD/build/native-windows-x64:/build:rw" hmbtn-windows-build \
  cmake --install /build --prefix /build/package
```

Saídas: `build/native-windows-x64/port/hmbtn_native.exe` e a pasta
`build/native-windows-x64/package/`. O toolchain do PS1 não participa desse build.
Se usar o SDL baixado pelo FetchContent de outro build, ajuste o caminho da origem
para seu diretório `_deps/sdl3-src`; o exemplo não baixa dependências sem rede.

O cross-build não executa automaticamente os testes Windows. Wine pode ajudar a
verificar o PE e seus caminhos, mas a CI MSVC e uma sessão Windows real continuam
sendo validações distintas.
