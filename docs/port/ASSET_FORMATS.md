# Recursos da tela de título — M2

A aplicação PC agora lê e apresenta recursos extraídos, com decodificação em C
nativo. Não executa o EXE/overlays MIPS nem a máquina de estados de `title.c`.
O código original de `src/` permanece preservado. Esta adaptação está em `port/`.

## Caminho de leitura

```text
data/A_FILE/00_gf.bin
  intervalo [0x2000, 0x25000)
    ANI: 0xD1C bytes
    banco 0x0D: restante do bloco
      atlas de palavras/paletas em CPU
        composição de frames em RGBA 320 × 240
          textura SDL3 → janela nativa
```

Os limites vêm de `SCENE_IMAGE_SECTOR`, `SCENE_IMAGE_SIZE` e
`title_load_sprite_file()` em `src/overlay/27_GF_TITLE/title.c`. O início do banco
é `0x4FAC - 0x4290 = 0xD1C` dentro do bloco lido. Não são ponteiros host.

`port/filesystem/resource_file.c` usa I/O SDL3 para caminhos UTF-8 e leitura de um
intervalo exato. Rejeita caminhos absolutos e componentes de travessia. Não consulta
a imagem do disco. O recorte M3 acrescentou a busca de `data` junto ao executável
e no layout de desenvolvimento; `--data DIR` é prioritário. Veja [TITLE_FLOW.md](TITLE_FLOW.md).

## Banco de imagens 0x0D

O cabeçalho contém tipo (byte), reservado (byte) e contagem (u16 little-endian).
Cada entrada tem 12 bytes: tamanho do payload nos 24 bits baixos de um u32,
flags nos 8 bits altos, depois x/y/largura/altura como quatro u16. A largura é
medida em palavras de 16 bits. O payload tem padding para múltiplo de quatro.

O decodificador adapta o caminho `texture_pack_load()` de `texture_pack.c`:

- Flag 4, sem flag 8: dados brutos em linhas.
- Sem flag 4/8: RLE em linhas. Controle assinado negativo copia bytes literais;
  positivo repete o byte seguinte. Controle zero produz um byte, conforme o laço
  do código original. Runs maiores que a saída esperada são rejeitados.
- Flag 8: RLE em colunas, com a coluna de bytes baixos de cada palavra seguida
  da de bytes altos. Esse caminho tem teste sintético; o banco de título local
  utiliza os caminhos brutos e RLE em linhas.

As posições originais são preservadas num atlas CPU de 1024 × 512 palavras, com
uma máscara indicando quais palavras foram carregadas. Isso permite resolver as
referências das animações sem carregar endereços PS1 em ponteiros nativos. Não é
uma implementação da GPU, de DMA ou das ordering tables.

## Arquivo de animação 0x0E

Cabeçalho de 36 bytes: magic u32 `0x0E`, cinco contagens u16 nos bytes 4..13,
reservado nos bytes 14..15 e cinco offsets u32 nos bytes 16..35.

| Seção | Registro | Conteúdo |
|---|---|---|
| Imagens | 8 bytes | x, y, largura em palavras, altura |
| Listas de partes | 4 bytes | contagem e offset relativo à seção de partes |
| Partes | 12 bytes | imagem, paleta, deslocamento x/y, atributos e campos auxiliares |
| Sequências | 4 bytes | número de frames e offset relativo à seção de frames |
| Frames | 10 bytes | lista de partes, deslocamento x/y, duração e controle |

Offsets, contagens, índices e destinos de salto são validados antes de usar as
tabelas. Imagens/paletas ausentes no atlas também produzem erro. O formato é
interpretado por bytes little-endian, sem reinterpretar buffers como structs.

A composição implementa pixels indexados de 4/8 bits, paletas de 16/256 cores,
cores BGR555, cor zero transparente, preto `0x8000` opaco, espelhamento X/Y,
recorte e mistura média quando a parte e o pixel habilitam semitransparência.
A ordem das partes corresponde à ordem resultante das inserções invertidas do
código PS1. Não há suporte geral a modelos, escala de sprites ou todos os modos
de mistura da GPU.

O avanço das sequências adapta as durações e controles documentados em `anim.c`:
duração em meios ticks, salto com bit 7 e retenção com `0xFF`. A prévia usa um
relógio próprio de cerca de 29,97 ticks/s. O primeiro recorte M2 não reproduzia a seleção aleatória e os
timers da tela original nem as flags de gameplay de fim/salto/pausa de `AnimState`.

## Conteúdo mostrado

Na cópia local, o bloco contém 72 referências de imagens, 38 sequências e um
banco com 73 entradas. O recorte M2 mostrou fundo (29), logotipo (31) e três
amostras animadas (1, 11, 21). M3 acrescentou Natsume (30), copyright (32),
prompt (33), opções (34/35), cursor (36), camadas móveis (37), personagens a
partir de 0/10/19 e grupos de flores 22..27. A barra inferior de diagnóstico foi
removida; a imagem agora é reservada ao jogo. Escape/F11 são atalhos da janela.
O comportamento e os limites atuais estão em [TITLE_FLOW.md](TITLE_FLOW.md).

Validação estrutural não autentica a cópia do jogo. A conferência completa dos
arquivos importados continua disponível por `python3 port/tools/game_data.py verify data`.
Os dados PT-BR foram lidos com sucesso; sua compatibilidade de gameplay permanece
pendente. Capturas e dados de validação reais ficam apenas em `build/`, fora do Git.

## Testes e próximo recorte

Os testes sem assets proprietários exercitam RLE bruto/literal/repetido/colunas,
truncamento em cada limite de bytes, limites do atlas, seções sobrepostas, índices
e saltos inválidos, transparência, espelhamento, paletas, clipping e avanço de
frames. Um teste do executável cria um pacote sintético e verifica renderização,
captura BMP e erros de arquivo ausente/truncado/incompatível.

A janela real Linux Wayland/OpenGL foi executada com os dados locais. A verificação
adicional com ASan/UBSan não pôde ser vinculada: os runtimes não estão instalados
neste host. Os testes CTest não dependem deles. Uma comparação
visual e temporal sistemática com a execução do PS1 ainda não foi realizada.
M3 já tem um primeiro recorte de estados/menu, documentado em [TITLE_FLOW.md](TITLE_FLOW.md).
A abertura e o fundo móvel foram posteriormente adaptados no recorte M3,
com seleção aleatória e áudio nativo parcial. Cadastro/saves e integração com
fazenda ainda estão pendentes; veja [TITLE_FLOW.md](TITLE_FLOW.md).
