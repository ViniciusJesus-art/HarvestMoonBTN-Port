# Auditoria inicial — SLUS_011.15

Base clonada do fork: `5f19c51`. Inspeção de código, sem execução do jogo.

| Área | Evidência | Trabalho necessário |
|---|---|---|
| Inicialização e loop | `src/main/main.c` inclui PSY-Q, despacha cenas e usa `WORK_REGION` em `0x801BE1B0` | Separar estado host, loop e serviços de plataforma |
| CPU | `src/include/cpu_asm.h` troca a pilha com assembly MIPS e usa scratchpad em `0x1F800000` | Substituir operações por implementação host após analisar seus chamadores |
| Overlays | CMake raiz define regiões `low` e `high`; `src/main/overlay_regions.c` usa símbolos do linker; `LOAD_OVERLAY` em `main.c` lê do A_FILE | Compilar módulos para host e substituir carregamento de código MIPS por seleção de cenas |
| Arquivos | `src/main/cd/afile.c` usa `CdSearchFile`, `CdRead`, `CdControl`, VSync e tabela de 33 membros | Definir leitura de arquivo host com limites e tratamento de erros; estudar o formato antes de extrair |
| Gráficos e geometria | `src/main/graphics/` e `src/main/geometry/` dependem de `libgpu`, `libgs` e `libgte` | Mapear ordering tables, VRAM, primitivas e operações de ponto fixo |
| Áudio | `src/main/sound/` e `cd_audio.c` | Mapear SPU, bancos e streaming antes de escolher implementação |
| Controles | `src/main/input/pad.c` e `pad_vibration.c` | Traduzir teclado/gamepad para o estado esperado pelo jogo |
| Tipos e ABI | `src/include/types.h` usa inteiros C; código original também usa `u_long` e ponteiros PS1 | Auditar tamanho, alinhamento e serialização; evitar assumir ABI de 32 bits no host |

## Fundação disponível

`port/core/psx_data.*` fornece acesso little-endian sem alinhamento obrigatório e tipo explícito para endereço serializado de 32 bits. Isso não resolve endereços absolutos, estruturas PSY-Q ou execução de overlays. O bootstrap não compila `src/`.

## Primeiro recorte de integração

Começar pelo caminho de inicialização até `src/overlay/27_GF_TITLE/title.c`: inventariar serviços chamados, definir acesso aos recursos necessários e portar a cena. O marco continua sendo apresentar a tela de título real com entrada funcional. Uma janela vazia ou o diagnóstico atual não satisfazem esse marco.

A auditoria é inicial: não houve revisão exaustiva de símbolos, formatos ou comportamento.
Posteriormente, SDL3 foi integrado à janela de diagnóstico do marco M1. Isso não
substitui a auditoria nem implementa os serviços PSY-Q acima. Veja o [roteiro](ROADMAP.md).
