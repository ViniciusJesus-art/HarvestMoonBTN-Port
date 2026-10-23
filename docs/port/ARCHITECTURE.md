# Arquitetura do porte

O código `src/` e a configuração existente do PS1 devem continuar intactos inicialmente. A camada nativa ficará sob `port/`.

```text
src/ (lógica reconstruída)
  |
port/core/             -- inicialização, memória e ciclo de jogo
port/game/             -- recortes de lógica adaptados para o host
port/graphics/         -- tradução de comandos gráficos do PS1
port/audio/            -- reprodução e mixagem
port/input/            -- gamepad, teclado e toque
port/filesystem/       -- leitura de dados do usuário, saves
port/platform/         -- adaptações Linux, Windows e Android
```

## Dependências a investigar

- Bibliotecas PSY-Q (`libgpu`, `libgs`, `libgte`, `libcd`, `libetc` e áudio).
- Endereços absolutos, ordenação e tamanho de estruturas e ponteiros.
- Overlays que compartilham regiões de memória.
- Renderização por ordering tables e buffers de primitivas.
- Sincronização por VSync e temporização de frames.
- Formato dos recursos no `A_FILE.BIN`.

## Princípios

1. Preservar o histórico e facilitar merges do upstream.
2. Não renomear ou reorganizar `src/` sem necessidade comprovada.
3. Manter o toolchain e targets PS1 disponíveis.
4. Isolar códigos específicos de plataforma.
5. Não armazenar discos originais, ROMs, assets proprietários nem SDKs de terceiros sem permissão.

## Base implementada

`port/core/` contém a entrada, o ciclo da aplicação de prévia e os helpers de
serialização little-endian. O tipo `HmbtnPsxAddress` representa um endereço serializado
de 32 bits; ainda não existe tradução para memória host. `port/platform/` mantém
janela/renderizador SDL3; `port/graphics/` decodifica bancos/ANI, compõe paletas/partes
e desenha a prévia dos recursos do título; `port/filesystem/` lê intervalos; `port/input/`
trata Escape, F11, fechamento e botões PS1 via teclado/gamepad. `port/game/` adapta
a abertura e os estados 0..9 do título, mais os estados 11..28 de apresentação
e cadastro, com estado host e relógio de cerca de 29,97 ticks/s. A área lógica
usa os 320 × 240 de `src/main/main.c`. A inicialização/ciclo de jogo completo ainda
não foram integrados.

Áudio geral, tradução geral dos gráficos e lógica de gameplay continuam
planejados; a música e os efeitos do título já têm uma implementação parcial. O build de PC não compila módulos de `src/` nem usa SDKs PS1; apenas reutiliza
as tabelas existentes dos layouts iniciais por um include gerado no build e não simula subsistemas
ausentes. CMake seleciona a base nativa antes de definir o toolchain PS1, usando
diretórios de build independentes. SDL3 fornece serviços host; não implementa GPU,
GTE ou SPU por nós. O recorte M2 implementa imagens indexadas e animações 2D; o
backend de M4 acrescenta oito tipos de primitivas prelit TMD, projeção inteira,
texturas afins e ordenação, em `graphics/model_decode.*` e `model_render.*`.
`scene_preview.*` carrega o modelo base da fazenda; a opção `--scene-preview`
permite validar sua geometria sem substituir a sequência original de New Game.
New Game também carrega os sprites do jogador/Thomas e o evento de chegada:
`game/event_script.*` despacha handlers C de `farm_intro.*`, e a renderização
insere os atores na ordenação do cenário. A tabela da cena acrescenta portas, caixa, posições/rotações e campo inicial.
As áreas 230/231/232 executam a infância, encontro, canção XA, despedida e
retorno a Thomas (`0x75..0x79`), seguidos da entrada na casa/área 80 e término
do roteiro. `game/ground.*` consulta a geometria original e `game/house_control.*`
libera caminhada/corrida na casa. Saída para a fazenda, colisões com atores e
demais interações permanecem pendentes. `game/house_interaction.*` e
`graphics/house_ui.*` executam TV, calendário, baú, livro/estante, opções e menu
da cama. `game/house_sleep.*` coordena câmera, animação e coberta até o limite
do pernoite, sem simular avanço diário ou salvamento. Detalhes em
[HOUSE_INTERACTIONS.md](HOUSE_INTERACTIONS.md), [FARM_INTRO.md](FARM_INTRO.md) e
[SCENE_RENDERER.md](SCENE_RENDERER.md). O atlas em CPU
mantém posições originais para resolver imagens/paletas, sem simular o hardware.

Os testes cobrem formatos, importação, leitura de arquivos, decodificação de
imagens/animação, atalhos, frames dummy/software e o pipeline completo com pacote
sintético, além de estados do menu, retenção de teclado e controle virtual.
A opção `--diagnostics` mantém a verificação de console sem vídeo e
`--window-test` desenha o diagnóstico sem recursos. Nenhum desses testes comprova
gameplay. O título reproduz bancos VAB/ADPCM e sequência SEQ via SDL3, com
fidelidade SPU ainda parcial. Detalhes em [ASSET_FORMATS.md](ASSET_FORMATS.md)
e [TITLE_FLOW.md](TITLE_FLOW.md).

Em hosts de 64 bits, ponteiros e `long` não podem ser usados como substitutos de palavras e endereços PS1. Dados de arquivo devem ser decodificados explicitamente, sem reinterpretar buffers como structs nativas. Os overlays deverão tornar-se módulos nativos ou tabelas de funções; carregar seus binários MIPS não executará código de PC.
