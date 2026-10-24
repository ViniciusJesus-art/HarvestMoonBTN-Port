# Instaladores do porte nativo

Os pacotes distribuem **somente o porte para PC**, seu extrator e bibliotecas de
runtime. Não contêm executáveis/overlays MIPS, SDK, fontes, imagem de disco ou
recursos proprietários. O programa continua sendo um porte parcial; empacotar
não implementa gameplay, avanço diário ou saves que ainda faltam.

## Uso

| Formato | Instalação |
|---|---|
| Windows x64 EXE | Assistente por usuário, sem administrador. Escolha a pasta e, opcionalmente, o `.bin`. Cria atalhos com ícone na área de trabalho e menu Iniciar, além do desinstalador. |
| DEB | Abra no gerenciador de software de Debian/Ubuntu. Bibliotecas do sistema são resolvidas automaticamente; o jogo aparece no menu de aplicativos. |
| RPM | Abra no gerenciador de software de Fedora/distribuição compatível. Dependências são resolvidas pelo gerenciador; inclui entrada no menu. |
| AppImage x86_64 | Permita execução e abra. Após preparar os dados, registra atalhos no menu/área de trabalho e mantém uma cópia estável do AppImage na pasta do usuário. |

Python, compilador, SDL instalado separadamente e emulador **não são necessários
para jogar**. O Windows inclui SDL e CRT estáticos. O AppImage inclui SDL,
backends X11/áudio, dependências e seus avisos de licença. O DEB/RPM usa bibliotecas
normais do sistema, instaladas pelo gerenciador de pacotes.
A base Linux dos artefatos de CI é Ubuntu 22.04/x86_64, glibc 2.35; não se promete
compatibilidade com distribuições mais antigas ou com libc musl. Execução em
Windows real/MSVC e outras distribuições ainda precisa de validação.

No primeiro início, escolha sua cópia **MODE2/2352, SLUS_011.15**. Traduções
compatíveis com o mesmo layout, como a cópia PT-BR testada, são aceitas. Pode-se
arrastar o arquivo `.bin` para a janela se o seletor do desktop não estiver
disponível. Escape cancela e remove a extração temporária.
No EXE, escolher o disco no assistente antecipa essa importação; deixar vazio
permite fazê-la no primeiro início. O menu também oferece “Preparar dados”.

O ícone do executável, da janela, dos pacotes e dos atalhos é gerado a partir de
`port/HMBTN_Logo.png`. No Windows, o instalador aplica o `.ico` também aos atalhos
do menu Iniciar e da área de trabalho. No AppImage, os atalhos criados no primeiro
uso apontam para a cópia estável e usam o mesmo logo. Para substituir o ícone,
troque esse PNG quadrado (RGBA, 8 bits por canal) e gere novamente os pacotes.
O PNG original permanece no lugar, inclusive para as janelas. O configure gera
variantes de 16, 24, 32, 48, 64, 128 e 256 pixels: o ICO Windows contém todos
esses tamanhos e os pacotes Linux instalam as variantes em `hicolor`. AppImage
inclui também o PNG de 256 pixels na raiz e em `.DirIcon`. EXE, instalador,
desinstalador e atalhos compartilham o mesmo logo; não é necessário manter
cópias manuais do arquivo fonte em outras pastas.

## Destino dos dados

- Linux: `$XDG_DATA_HOME/hmbtn/HarvestMoonBTN/data`, normalmente
  `~/.local/share/hmbtn/HarvestMoonBTN/data`.
- Windows: `%APPDATA%\hmbtn\HarvestMoonBTN\data`.
- Projeto/desenvolvimento: `--data DIR` continua prioritário. `data/` no projeto
  ainda funciona, sem subpasta de idioma. O instalador não depende do repositório.

O `.bin` não é copiado nem alterado. A importação guarda os 19 membros de
`A_FILE` marcados `original` na tabela de recursos e os quatro fluxos XA. Exclui
os membros `built` (overlays MIPS), `SLUS_011.15`, HDT, licença do disco e o
agregado `A_FILE.BIN`. Mantém `disc/STR/` somente como caminho dos áudios XA
lidos pelo porte; não é uma instalação do executável de PS1.

`native-import.json` registra formato 2, jogo, caminho de origem, nomes, tamanhos
e SHA-256 dos 23 recursos. Não contém bytes do disco nem marca paridade com a
referência americana como comprovada. O manifesto antigo de desenvolvimento
(`manifest.json`, formato 1) continua independente e inclui material adicional
para reconstrução de PS1; não é gerado nem distribuído por estes instaladores.

A importação lê setores e calcula hashes em fluxo. Não carrega a imagem inteira
na RAM, não sobrescreve destinos existentes e publica a pasta somente após
concluir. Falha/cancelamento remove apenas os arquivos temporários da operação.
Atualização/desinstalação preserva os dados do usuário; para trocar de disco,
renomeie ou remova deliberadamente a pasta de dados antes de importar novamente.
Os hashes ficam disponíveis para consulta, sem uma verificação pesada em cada
início que impeça futuras modificações de recursos.

Também é possível importar sem interface, usando o mesmo código nativo:

```sh
hmbtn_native --import-bin "/caminho/jogo.bin" --data "/caminho/novo/data"
hmbtn_native --data "/caminho/novo/data"
```

O destino deve ainda não existir. Não se deve extrair recursos para `/usr/bin`,
`Program Files` ou para dentro do AppImage. Assim o jogo abre sem privilégios de
administrador e sem depender do diretório de trabalho.

## Build e empacotamento

Dependências de **desenvolvimento** incluem CMake, compilador C11, Python 3.10+
(ícone/testes), SDL3 ou seu download fixo. O usuário final recebe os binários.

```sh
cmake --preset native-release -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
cmake --build --preset native-release
ctest --preset native-release
cpack --config build/native-release/CPackConfig.cmake -G DEB -B build/artifacts
cpack --config build/native-release/CPackConfig.cmake -G RPM -B build/artifacts
cmake --install build/native-release --prefix "$PWD/build/AppDir/usr"
python3 port/packaging/audit_payload.py build/AppDir
python3 port/packaging/download_appimage_tools.py build/appimage-tools
python3 port/packaging/appimage.py build/AppDir build/artifacts/HarvestMoonBTN-linux-x86_64.AppImage \
  --tool build/appimage-tools/appimagetool.AppImage --runtime build/appimage-tools/runtime-x86_64
```

O AppImage deve ser montado em uma distribuição de base compatível, com
`dpkg-query` e avisos de licença disponíveis. O script inclui a cadeia de
bibliotecas dos backends SDL, excluindo glibc e drivers de GPU do host.
`Dockerfile.linux` oferece o ambiente Ubuntu 22.04 com os empacotadores. Todos
os builds host ficam em `build/native-*`; nenhum desses comandos usa o SDK PS1.

Windows, em PowerShell após configurar/compilar o host com MSVC e NSIS:

```powershell
cmake --install build/native-release --config Release --prefix build/runtime
python port/packaging/audit_payload.py build/runtime
$payload = (Resolve-Path build/runtime).Path
$output = Join-Path $PWD 'build/artifacts/HarvestMoonBTN-windows-x64-setup.exe'
New-Item -ItemType Directory -Force build/artifacts | Out-Null
& "${env:ProgramFiles(x86)}\NSIS\makensis.exe" "/DPAYLOAD=$payload" "/DOUTPUT=$output" port/packaging/windows.nsi
```

O instalador Windows seleciona explicitamente EXE, ícone, README e licenças; inclui
`SDL3.dll` somente para builds que usam SDL compartilhado. Não empacota a pasta
do projeto. O AppImage usa appimagetool 1.9.1 e runtime 20251108, com downloads
verificados por SHA-256. Sem FUSE disponível, execute com
`--appimage-extract-and-run`, sem instalar um pacote adicional.

## GitHub Actions

[packages.yml](../../.github/workflows/packages.yml) executa **somente manualmente**,
em Actions → Native installers → Run workflow. Escolha:

- `AppImage`, `RPM`, `DEB` ou `EXE`: somente aquele formato.
- `Linux`: AppImage + RPM + DEB.
- `Windows`: instalador EXE.
- `Todos`: todos os formatos das duas plataformas.

Em `Destino dos pacotes`, `Teste` guarda apenas artifacts da execução; `Release`
publica os formatos selecionados numa GitHub Release, exigindo uma nova tag
como `native-v0.1.0`. A publicação só acontece se todas as compilações solicitadas
passarem. Os pacotes incluem seus hashes SHA-256.

Os workflows separados de testes nativos e build PS1 também passaram a execução
manual. Nenhum desses workflows compila automaticamente em push ou PR.

No modo Teste não publica uma GitHub Release. Nenhum modo envia recursos locais. O workflow separado
[native.yml](../../.github/workflows/native.yml) mantém testes Debug/Release dos
dois sistemas. O build/checagem original de PS1 em `build.yml` foi preservado e
não é uma dependência dos instaladores.

## Validação local

- Configure/build Debug Linux e **21 CTests** aprovados; repetidos em Release
  Ubuntu 22.04 dentro do contêiner.
- Fixture sintética: hashes, arquivos XA, exclusão dos executáveis/overlays,
  caminhos Unicode, destino existente, setores/HDT inválidos e rollback.
- Disco local PT-BR: 23 recursos idênticos aos da extração anterior; importação
  Debug em cerca de 1,8 s e RSS máximo próximo de 3 MiB, sem swap.
- Preparação SDL: importação pelo evento de arrastar arquivo e cancelamento
  durante a operação; nenhum destino parcial deixado pelo cancelamento.
- Título e casa carregados usando somente a importação nova. AppImage construído
  e executado em modo extração, incluindo a casa com esses recursos.
- DEB, RPM e EXE NSIS gerados; os diretórios de runtime passaram pela auditoria
  de ausência de dados, SDKs e arquivos de PS1. Fluxo visual completo do EXE em
  Windows real e execução dos jobs no GitHub permanecem pendentes.
- AppImage: cópia estável, ícone e atalhos no menu/desktop conferidos com
  diretórios XDG isolados. Windows no Wine, com prefixo isolado: instalação,
  atalhos, importação pelo EXE e desinstalação preservando dados conferidos;
  os 23 hashes coincidem com Linux. Wine não substitui teste em Windows real.

Referências dos empacotadores: [CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html),
[CPack RPM](https://cmake.org/cmake/help/latest/cpack_gen/rpm.html),
[AppImage runtime](https://github.com/AppImage/type2-runtime),
[AppImage sem FUSE](https://docs.appimage.org/user-guide/troubleshooting/fuse.html).
