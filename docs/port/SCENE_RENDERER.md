# Primeira geometria de cenário — M4 parcial

O executável agora desenha o **modelo base da fazenda** com código C nativo.
`--scene-preview` é uma prévia de desenvolvimento sem personagens ou áudio.
New Game agora inicia a chegada com o jogador e Thomas; `--intro-preview`
abre esse evento diretamente. Veja [FARM_INTRO.md](FARM_INTRO.md).
Ainda faltam colisões, plantações e outros objetos dinâmicos. Esta etapa não
executa o jogo completo.

```bash
./build/native-debug/port/hmbtn_native --scene-preview
./build/native-debug/port/hmbtn_native --scene-preview --window-size 1280x720
```

No pacote Windows: `hmbtn_native.exe --scene-preview`. A busca de `data/`,
`--data DIR`, F11, Escape, gamepads, redimensionamento, `--frames` e `--screenshot`
funcionam como no título. Setas/D-pad movem a **câmera de diagnóstico**; Enter/Start
restaura a vista inicial perto da casa. Não há texto host sobre a imagem.
O movimento dessa câmera não representa o movimento original do personagem.

## Arquivos e dados usados

| Arquivo nativo | Responsabilidade |
|---|---|
| `port/graphics/model_decode.*` | Leitura e validação do modelo TMD prelit do jogo |
| `port/graphics/model_render.*` | Câmera, projeção, ordenação e rasterização em CPU |
| `port/graphics/scene_decode.*` | Tabela dos 15 modelos adicionais, visibilidade, posição e rotação |
| `port/graphics/scene_preview.*` | Áreas 0/230, campo, atores, câmera e apresentação SDL |
| `port/graphics/asset_decode.*` | Banco de texturas/paletas já usado pelo título |

A origem é `src/overlay/11_GF_FARM/farm_load.c`,
`src/overlay/common/farm_section_table.h`, `world/scene_view.c` e
`src/main/geometry/model.c`. Esses arquivos e o build PS1 permanecem preservados.
O host adapta as operações para C11 e não chama o SDK PS1 nem executa MIPS.

A geometria da área 0 usa estes intervalos de `data/A_FILE/00_gf.bin`:

| Intervalo | Conteúdo |
|---|---|
| `[0x83800, 0xBA800)` | Seção da área 0: cabeçalho e modelos |
| `0x85800` | Início do TMD principal, a `0x2000` bytes da seção |
| `[0xBA800, 0xDD800)` | Banco de texturas e paletas da área |

O modelo principal contém 294 objetos e 4.246 primitivas; com os modelos
adicionais visíveis, a capacidade é 4.362. A arte do campo usa ainda o intervalo
`[0x385F800,0x386A800)`. Os recursos da infância estão em [FARM_INTRO.md](FARM_INTRO.md).
As posições e texturas
vêm dos recursos extraídos; não há desenho substituto da fazenda. O banco grande
de cerca de 62 MiB não é carregado inteiro. O overlay `11_GF_FARM.bin` não é código
executável para PC.

## Formato e renderização

O cabeçalho TMD contém `0x42`, flags e contagem de objetos. Cada objeto tem sete
palavras de 32 bits; offsets são relativos ao início da tabela, doze bytes depois
do início do TMD. Vértices são triplas signed de 16 bits com padding. Modelos com
múltiplos objetos têm o bloco de centros usado pelo recorte/frustum original.
A prévia valida esse bloco, mas ainda não usa o frustum de centros.

O formato de primitivas foi preparado pelo jogo. Os bytes de comprimento do TMD
convencional também carregam flags de profundidade; não podem determinar o passo
do parser. A implementação segue os tamanhos de `model_draw_tmd_object()`:

| Chave sem o bit de semitransparência | Tipo | Bytes |
|---|---|---|
| `0x2101` / `0x2901` | Triângulo / quadrilátero de cor plana | 16 / 16 |
| `0x3101` / `0x3901` | Triângulo / quadrilátero Gouraud | 24 / 28 |
| `0x2501` / `0x2D01` | Triângulo / quadrilátero texturizado de cor plana | 28 / 32 |
| `0x3501` / `0x3D01` | Triângulo / quadrilátero texturizado Gouraud | 36 / 44 |

Essas são as oito variantes presentes no modelo principal local. São suportados
endereços relativos, até 1.024 objetos e 16.384 primitivas. Arquivos truncados,
índices/offsets inválidos, endereços absolutos PS1, primitivas com iluminação por
normais e formatos desconhecidos são rejeitados. Bytes são lidos explicitamente;
nenhum endereço serializado vira ponteiro host.

A câmera usa matriz look-at de ponto fixo 12, projeção de distância 1.000 e centro
de tela `(160,120)`. O deslocamento inicial segue yaw `-512`, distância `-9000`
e altura `-4500` de `farm_scroll_reset()`. O alvo da prévia é escolhido para olhar
a casa; não é uma câmera de evento.

O desenho implementa rejeição de faces/limite próximo, UVs afins, triângulos e
quadriláteros, interpolação de cores, páginas de 4/8/16 bits, CLUT BGR555, cor zero
transparente, preto opaco, modulação, dithering e os quatro modos de mistura em
texturas semitransparentes. A tabela de ordenação usa profundidade média, flags
`0x8000/0x4000` e ordem inversa de inserção nos empates, conforme o caminho original.
O frame é composto em 320×240; SDL apresenta uma textura com escala nearest.

Não há alocação de buffers próprios por frame: seção, atlas, pixels e lista de
polígonos são alocados uma vez. O frame é redesenhado quando a câmera, os objetos ou a cena mudam.
Enquanto as imagens das lembranças cobrem a tela, o cenário não é rasterizado.

## Limites de fidelidade

- Divisão perspectiva e normalização usam operações inteiras portáveis; ainda
  não reproduzem as aproximações/flags exatas da GTE. Recorte frustum e regras
  de borda/interpolação da GPU precisam de comparação com o PS1.
- Além do TMD principal, portas e caixa fechadas, modelos adicionais e campo
  inicial estão presentes;
  água/animações de textura e interações/condições de gameplay ainda faltam;
  partes incompletas não devem ser interpretadas como a cena final.
- Semitransparência não texturizada usa o modo médio padrão; a herança do estado
  de página entre comandos ainda precisa ser implementada quando exigida.
- Há translação e rotação dos objetos; morphs ficam na primeira chave.
  Não há iluminação por normais nem renderer
  geral de todas as áreas. Não existe confirmação de paridade visual 1:1.

## Por que a prévia é separada de New Game

O startup original carrega a área 0 e solicita o evento `0x74` em `farm_main.c`.
Mostrar apenas a fazenda estática após o cadastro substituiria esse evento.
Por isso `--scene-preview` é uma entrada explícita de desenvolvimento.

Nos dados locais, o evento está em `[0x2CC800,0x2CE800)` de `12_event_s.bin`.
O bloco contém um ator, texturas e um script de 1.248 bytes (78 registros de
16 bytes). Os comandos envolvem posição/animação de atores e jogador, movimento,
diálogos, esperas, variáveis/saltos, fade, música, mudança de modo e outro evento.
Esse recorte foi acrescentado em [FARM_INTRO.md](FARM_INTRO.md), incluindo a
transferência do cadastro. As áreas 230/231/232 e os eventos `0x75..0x79` já foram integrados, incluindo
encontro, canção e retorno ao presente. O próximo limite é o interior da casa/área 80.

## Verificação

Testes sintéticos cobrem as oito primitivas, truncamento, índices e offsets,
limites da câmera, recorte, cobertura, transparência, páginas de textura,
misturas e ordenação. Um teste do executável carrega um arquivo sintético,
confere geometria na captura 1280×720 e rejeita dados ausentes/corrompidos.
Nenhuma fixture inclui recursos proprietários.

No marco da geometria, configure/build e 16 testes CTest passaram no Linux. Build MinGW Windows x64
e pacote atualizados com os mesmos fontes, em contêiner limitado a 1.536 MiB,
sem rede e uma tarefa de compilação. Esta revisão não foi executada no Wine;
Windows real e MSVC continuam pendentes.

Janela real Linux X11/software: 900 frames, movimento nas quatro direções e
reset, saída zero; pico RSS 74.456 KiB (cerca de 73 MiB). Uma execução dummy de
três frames mediu 15.476 KiB (cerca de 15 MiB). São medições desse ambiente.
Artefatos locais ignorados: `build/native-debug/farm-preview.png`,
`farm-desktop.bmp` e `scene-window-validation.log`.
