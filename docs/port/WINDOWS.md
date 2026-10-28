# Build Windows x64

O mesmo C nativo e as mesmas interfaces SDL3 são usados em Linux e Windows.
O runtime tem título, abertura e os recortes jogáveis de fazenda/vila descritos
na [arquitetura atual](ARCHITECTURE.md); o jogo completo continua em andamento.
A CI usa MSVC; o cross-build local usa MinGW. Compilar com MinGW ou executar
via Wine não substitui validar com MSVC e Windows real.
Recursos devem vir da cópia do usuário e não estão no pacote.

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

## Correção de portabilidade de 2026-10-28

O [job Windows 122794267669](https://github.com/ViniciusJesus-art/HarvestMoonBTN-Port/actions/runs/40681838415/job/122794267669)
falhou na compilação do código nativo com MSVC 19.51, em Release. A configuração
e a biblioteca SDL3 foram compiladas; três avisos do projeto viraram erros por
`/W4 /WX`:

| Diagnóstico | Arquivo | Correção |
|---|---|---|
| C4456: variável local oculta outra variável | `port/game/exploration.c` | Nome próprio `menu_opened` para a abertura do menu; preserva a variável `opened` usada pelas interações da casa. |
| C4310: conversão trunca uma constante | `port/graphics/scene_decode.c` | Aplica a máscara ao valor de flags antes da conversão para 16 bits, em vez de converter a constante invertida de 32 bits. |
| C4018: comparação signed/unsigned | `port/game/farm_action.c` | Multiplica o modificador por `20u`, mantendo a comparação com `charge_frames` no domínio sem sinal. |

Os três ajustes conservam os algoritmos e os valores existentes. Não foram
desativados `/WX`, `/W4` ou os avisos GCC, nem alterados os fontes PS1.

`Native PC tests` agora roda em push e pull request quando mudam os fontes ou
a configuração do porte, além do acionamento manual. Compila todos os alvos em
Linux/GCC e Windows/MSVC, Debug e Release. Nas execuções automáticas, seleciona
oito testes sem BIN por configuração (32 processos na matriz): renderizador,
exploração, campo, inventário, saves, estufa, Orangecup e carry. O acionamento
manual conserva a suíte existente e os artefatos Windows. O empacotamento segue
no workflow próprio, sem publicação automática introduzida por esta correção.

Para verificar MSVC com a correção, é necessário executar o workflow **sobre o
commit que contém estes ajustes**. Reexecutar o job antigo recompila o commit
antigo, que ainda tem os três avisos.

### Acesso ao cenário anterior após o sono

O percurso completo Windows em Release/Wine revelou também uma leitura de
memória liberada em `hmbtn_world_gate_at`. O callback de sono chama `morning`,
que recarrega a casa e invalida o empréstimo `HmbtnExplorationFrame.section`.
Ao retornar, o modo já é FREE e o mesmo tick tentava consultar portões/saídas
na seção antiga. O alocador Linux não expôs isso no percurso anterior; no Wine
o acesso encerrou o processo após a primeira venda de Orangecup.

`exploration.c` registra que o tick entrou no callback de sono e adia essas duas
consultas até o próximo frame. Preserva mensagens, efeitos de apresentação e
câmera; o coordenador já obtém os empréstimos atualizados antes da fase de mundo.
Não há cópia de cenário, atraso de dias ou alteração nas regras de inventário.
`port/tests/exploration_test.c` reproduz a substituição de recursos com uma saída
obsoleta sob a posição nova do jogador, cobrindo tanto portão quanto saída
simples, sem depender de uma leitura inválida para detectar a regressão.

### Validação local desta correção

- Configuração e build completos Linux Debug/Release aprovados, usando a SDL3
  local já disponível. Aviso opcional de libdecor ausente não impede o build.
- Configuração e build completos Windows x64 MinGW Debug/Release aprovados,
  incluindo os executáveis de teste. Após a correção adicional do despertar,
  recompilados Linux Debug/Release e Windows Release.
- Linux Debug: **12/12 CTests** na seleção inicial (16 processos, com wrappers
  e filhos), mais a regressão de despertar **1/1** após a correção. Inclui
  exploração, modelos, campo, inventário, saves, estufa, Orangecup, carry e
  percursos reais de exploração, clima/portas, segunda casa e Orangecup.
- Linux Release: **8/8** testes sem BIN; depois da correção do despertar,
  **1/1** percurso completo de Orangecup com recursos e retomada em processo novo.
- Windows/Wine: modelos, exploração/ferramentas e Orangecup aprovados em
  Debug e Release. A regressão adicional de despertar passou no executável
  Windows Release; o percurso completo passou após a correção, incluindo
  noites, colheita, pagamento, perda/reconstrução e saves. O aplicativo Windows
  retomou o save em processo novo e apresentou três frames com retorno zero.
- Pacote local atualizado em `build/native-windows-x64/package/`, com
  `hmbtn_native.exe` em Release. Auditoria de payload aprovada, sem recursos
  proprietários, fontes ou SDK no pacote. A instalação final troca somente
  o executável atualizado e os mesmos arquivos de apresentação/licenças.
- YAML do workflow validado sintaticamente e matriz/gatilhos conferidos.
  `/W4 /WX` e os avisos GCC continuam ativos. `git diff --check` aprovado.
- Orçamento conservador de **50 processos de teste/verificação**, contando
  wrappers, filhos, execuções Wine e suas cargas Windows, a tentativa de
  percurso que expôs a falha, auditoria do payload e leitura do YAML. Não foi
  executada a suíte inteira.

**MSVC com a correção e Windows real ainda precisam do próximo job da CI e
de validação na plataforma.** Cross-build/Wine não são evidência de aprovação
do MSVC; o job antigo permanece um registro da falha do commit anterior.
Não foram publicados artefatos ou alterado o remoto nesta entrega.

Logs locais: `/tmp/hmbtn-windows-fix-linux-tests.log`,
`/tmp/hmbtn-windows-fix-linux-release-tests.log`,
`/tmp/hmbtn-windows-fix-wake-test.log`,
`/tmp/hmbtn-windows-fix-linux-release-playthrough.log`,
`/tmp/hmbtn-windows-fix-cross-build.log`,
`/tmp/hmbtn-windows-fix-cross-release-build.log`,
`/tmp/hmbtn-windows-fix-wine-release-playthrough.log` e
`/tmp/hmbtn-windows-fix-wine-release-resume.log`.
