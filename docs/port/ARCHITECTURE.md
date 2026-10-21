# Arquitetura proposta

O código `src/` e a configuração existente do PS1 devem continuar intactos inicialmente. A camada nativa ficará sob `port/`.

```text
src/ (lógica reconstruída)
  |
port/core/             -- inicialização, memória e ciclo de jogo
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

Esta estrutura é proposta, não implementada.
