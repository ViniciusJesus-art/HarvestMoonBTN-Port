# Base nativa para PC

`core/main.c` fornece a entrada C11 e as opções do executável. Sem argumentos,
`core/app.c` abre uma janela SDL3, processa eventos e apresenta o menu nativo parcial.
Escape fecha; F11 alterna tela cheia. A área lógica é 320 × 240, com escala inteira
e barras quando necessário. A cadência de apresentação é limitada a até 60 Hz,
sem espera ocupada; o título avança a cerca de 29,97 ticks/s. O renderizador
de software é o padrão deste recorte 2D, para reduzir a alocação inicial do driver.
A variável SDL_RENDER_DRIVER pode selecionar outro renderizador.

O executável agora lê o bloco da tela de título em `data/A_FILE/00_gf.bin` e
adapta as operações de imagens/animação documentadas no C original. Ainda não
compila os módulos completos de `src/`. O recorte dos estados 0..9 do título foi
adaptado em `game/title_flow.*`, com entrada PS1 traduzida de teclado/gamepad.
New Game inclui apresentação, nomes, aniversário e resumo (estados 11..28).
A transição para a primeira cena, saves e fazenda jogável ainda não foram portados. Não há gameplay.
Depende de SDL3, sem SDK PSY-Q nem emulador. Veja os marcos no
[roteiro](../docs/port/ROADMAP.md).

`platform/sdl_host.*` cria e libera janela/renderizador. `graphics/asset_decode.*`
decodifica bancos, ANI e paletas em C sem dependência de SDL. `graphics/title_preview.*`
compõe a abertura e o menu e os apresenta com SDL3. `filesystem/resource_file.*` lê intervalos
de arquivos com checagem de limites. `graphics/diagnostic_screen.*` mantém o teste
visual sem recursos; não traduz comandos da GPU PS1. `input/window_input.*`
trata atalhos; `input/game_input.*` traduz teclado e gamepad para bits PS1,
com hotplug e retenção de pressões entre ticks. `core/psx_data.*`
mantém os helpers little-endian e o tipo de endereço PS1 de 32 bits.

Por padrão, procura-se `data` junto ao executável, até três pais acima e no diretório
atual. `--data DIR` é prioritário. Isso permite executar de outra pasta em Linux/Windows.
Para diagnóstico gráfico sem recursos: `hmbtn_native --window-test`.
Para diagnóstico sem display: `hmbtn_native --diagnostics`. Para execução finita:
`hmbtn_native --frames 3`. Com limite de frames, `--screenshot caminho.bmp` salva
o último; sem limite, salva o primeiro, antes da apresentação. Os testes usam vídeo dummy/software e áudio dummy; a validação de uma
janela real é registrada separadamente no roteiro.

O CMake raiz seleciona este build com `HMBTN_BUILD_NATIVE=ON`, antes de configurar o toolchain PS1. Consulte o [guia de compilação](../docs/port/BUILDING.md), a [arquitetura](../docs/port/ARCHITECTURE.md) e a [auditoria inicial](../docs/port/INITIAL_AUDIT.md).

`tools/game_data.py` importa e verifica uma pasta de dados sem exigir novamente o
disco original. `cmake/extracted_disc.cmake` permite reconstruir o disco PS1 a partir
dessa pasta. O runtime nativo lê atualmente o bloco gráfico do título e intervalos VAB/SEQ
do arquivo de som. Consulte a
[documentação dos dados](../docs/port/GAME_DATA.md).

O módulo `audio/` decodifica VAB/ADPCM e executa a SEQ do título via SDL3; interpolação gaussiana/reverb e paridade com a SPU ainda estão pendentes. Consulte os
[formatos e limites do recorte M2](../docs/port/ASSET_FORMATS.md).

O fluxo parcial e o build Windows estão em [TITLE_FLOW.md](../docs/port/TITLE_FLOW.md)
e [WINDOWS.md](../docs/port/WINDOWS.md).

O recorte do cadastro e a apresentação em janelas 16:9 estão em [NEW_GAME.md](../docs/port/NEW_GAME.md).
