# Instalação dos dados do jogo

O disco é necessário uma vez, na importação. Depois, a pasta extraída pode ser
verificada e usada para reconstruir o disco PS1 sem consultar a imagem de origem.
O executável nativo já lê o bloco da tela de título em `A_FILE/00_gf.bin` para a prévia
M2 e o menu parcial M3. Ele ainda não executa cadastro/saves/gameplay. Veja os
[formatos implementados](ASSET_FORMATS.md) e o [fluxo atual](TITLE_FLOW.md).
Use `hmbtn_native --data /caminho/data` ou coloque `data/` junto ao executável.

## Importar e verificar

Python 3.10+; nenhuma dependência adicional. Execute na raiz do repositório:

```bash
python3 port/tools/game_data.py import "/caminho/jogo.bin" --out data
python3 port/tools/game_data.py verify data
```

No Windows, use `python` em lugar de `python3`, se necessário. A entrada é uma
imagem de uma faixa MODE2/2352 com o layout americano SLUS_011.15; outros formatos
e versões não foram implementados. A cópia PT-BR local foi importada com sucesso,
mas sua compatibilidade com o código americano não foi confirmada em execução.

A importação recusa uma pasta de destino existente, trabalha em uma pasta temporária
e só publica o resultado após verificar os arquivos. Uma falha não substitui uma
instalação anterior. Para outra edição do jogo, escolha outra pasta de destino.

```text
data/
  manifest.json        versão do formato, hashes da origem e integridade dos arquivos
  A_FILE/              32 membros separados: dados e overlays originais
  disc/                arquivos restantes, incluindo STR/*.XA, SYSTEM.CNF e licença
```

O manifesto registra tamanho e SHA-256 de cada um dos 44 arquivos instalados.
A verificação usa apenas a pasta: não guarda um caminho obrigatório para a origem.
O SHA-1 de origem identifica a cópia importada; diferenças da referência americana
são registradas e não equivalem a confirmação de compatibilidade.

A imagem original não é copiada para a instalação. O agregado `A_FILE.BIN` é
desempacotado e removido da pasta extraída para evitar duplicação. Seus membros
continuam tendo nomes `.bin`: são arquivos de recursos/módulos, não imagens de CD.
São preservados também os arquivos necessários à reconstrução PS1, inclusive o
executável original para identificação e comparação e `DUMMY.DAT` para o layout.

Os sprites ainda estão nos formatos internos dos pacotes; esta etapa não os
converte para PNG nem converte áudio. Essa conversão não é obrigatória caso o
runtime futuro interprete os formatos originais diretamente.

## Build PS1 usando a pasta

No ambiente com toolchain PS1, escolha **uma** fonte: `ORIGINAL_DISC` (fluxo original)
ou `HMBTN_EXTRACTED_DATA_DIR` (pasta instalada):

```bash
cmake -S . -B build/psx -G Ninja \
  -DORIGINAL_DISC= -DHMBTN_EXTRACTED_DATA_DIR="$PWD/data"
cmake --build build/psx --target disc
cmake --build build/psx --target check
```

No Docker do projeto, os caminhos devem corresponder aos volumes montados:

```bash
mkdir -p build/psx
docker run --rm --user "$(id -u):$(id -g)" --network none \
  -v "$PWD:/src:ro" -v "$PWD/build/psx:/build:rw" hmbtn-build \
  cmake -S /src -B /build -G Ninja \
  -DORIGINAL_DISC= -DHMBTN_EXTRACTED_DATA_DIR=/src/data
docker run --rm --user "$(id -u):$(id -g)" --network none \
  -v "$PWD:/src:ro" -v "$PWD/build/psx:/build:rw" hmbtn-build \
  cmake --build /build --target disc
```

O Docker não recebe o diretório que contém o disco original. Os targets `afile`
e `disc` usam os recursos extraídos e os overlays compilados. A integridade é
verificada na configuração e novamente quando as entradas mudam.

`check` e o build padrão continuam exigindo os hashes americanos originais,
inclusive os de A_FILE e do disco quando a fonte de dados está configurada.
Com recursos PT-BR, a geração experimental pode funcionar e essa verificação
estrita falhar: isso é esperado e não foi desabilitado. Não significa que a tradução
foi portada ou validada. Os binários compilados continuam sendo os americanos.

## Distribuição e próximo passo

`data/` já é ignorada pelo Git. Dados extraídos não devem ser publicados com o código
nem incluídos nos pacotes do porte. O futuro instalador poderá reutilizar a etapa
de importação; a distribuição autônoma desse instalador ainda exige empacotar os
scripts e manifestos utilizados aqui. Não há interface de instalação implementada.

O build e os testes nativos continuam independentes dos dados proprietários:

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

A leitura por intervalos com limites e a decodificação dos recursos do título já
estão em `port/`. A leitura/decodificação de outros recursos e de áudio XA ainda
precisa ser integrada. Nenhum overlay MIPS é executado e endereços serializados
não são reinterpretados como ponteiros host.
