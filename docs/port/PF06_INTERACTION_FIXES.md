# PF-06 — Correções de interações de 2026-10-29

Esta revisão corrige os limites da entrega de
[28/10](PF06_WORLD_FIDELITY.md): abrir o modelo não bastava para atravessar
portas internas, e o descarte no chão havia sido estendido indevidamente aos
objetos que o usuário quer devolver ao plantio. A fila de reprodução permanece
no [roadmap](ROADMAP.md); nenhum sistema novo de loja, movimento ou inventário
foi criado. `src/`, `sdk/`, `disc/`, `tools/` e o CMake PS1 foram preservados.

## Mensagem de Barley

`shop_barn_step` entrega `request.first=2/0x62` (saudação),
`request.second=2/0x6b` (cancelamento) e `request.third=2/0x6e`.
`shop_menu_open_choose`, porém, usa **2/0x46** durante a seleção, independentemente
do vendedor. O porte confundia `request.second` com a mensagem da escolha.
`feed_shop.c` agora usa 0x46 ao abrir/reabrir o menu de Barley e reserva 0x6b
para sair da seleção. Outros cancelamentos mantêm 0x6e.

O texto correto continua aparecendo abaixo do menu, como na rotina original.
Não há uma segunda conversa em paralelo: o teste compara a mensagem efetiva
da apresentação com a mensagem do único modal da loja, além de verificar os IDs
ao abrir, após resultados e ao cancelar. As expectativas antigas de 0x6b durante
a seleção foram corrigidas nos testes de criação/venda.

## Cursores

`src/main/text/text.c:text_draw_cursor` chama o ícone 11/1 em **x+8, y+6**.
Os layouts nativos já forneciam suas âncoras horizontais, mas as opções eram
desenhadas sem o deslocamento vertical. Um pequeno helper em `feed_shop_ui.c`
aplica os seis pixels à linha do texto, em Barley, mercado, Won, carpintaria,
ferramentas e confirmações. A lista de animais já usa coordenadas diretas de
ícone na fonte e mantém esse caminho. Testes de pixels usam o ANI/atlas real
para verificar as opções do mercado e de Barley; não são apenas asserts de
coordenadas de implementação.

## Travessia de portas internas

O original `player_zone_action` inicia `player_walk_start(10)` e o estado
`FARM_MODE_ZONE_WALK` inicia mais 27 ticks para `ZONE_ID_NO_RESET=0xfffe`.
`PLAYER_FLAG_NO_INPUT` suspende a sondagem de obstáculo `0xe080`; a posição
continua consultando o piso com `0xc080` em `player_update_position`.

`runtime.c` adapta essa caminhada automática no fluxo de porta já existente,
usando `door_tick` e `control.exit_pending`, sem criar uma área de destino nem
teletransportar o jogador. Aplica velocidade de caminhada 41, facing e animação
corretos, incluindo item carregado. O backend nativo só avança em piso válido,
para não terminar fora da geometria. Ao concluir, libera o controle e conserva
o mesmo mapa. `exploration.c` permite que `area_doors` atualize a pose e o som
durante essa caminhada, mesmo com relógio/controle manual suspensos.

O percurso testa os dois lados de **18 zonas** na clínica, biblioteca, vinícola
e pousada: personagem cruza a borda oposta da zona, termina no piso real, porta
fecha, item é preservado e teclas de direção/menu não interrompem a passagem.
Também mantém a recusa por amizade. Save continua recusado durante a travessia,
sem mudança de formato ou estado paralelo.

## Pedra e galho no plantio

Foi recuperado o ramo de colocação que já existia em `farm_carry.c`, limitado
ao campo ativo, seu catálogo e suas consultas de células. Itens com bit 0x10
de `class_flags`, incluindo pedra/galho, retornam a **célula ativa e livre**
de solo normal, arado ou regado. O chão de base é preservado; não há substituição
de planta, objeto ou célula inativa, nem colocação na água/caixa de remessa.
As figuras de inverno 0x13b/0x13c/0x13d são normalizadas aos IDs estáveis
de pedra/galho/madeira. Fazenda e estufa usam a mesma função existente.

Quando o objeto retorna à célula, o slot de voo é retirado, o campo é marcado
para atualização e o contador de descarte não sobe. Fora da grade, mantém-se
HOLD/BLINK e a segunda figura antes de sumir. Ervas/produtos sem a classificação
de colocação continuam nesse descarte. Não há um gerenciador de objetos novo.

## Conferência do BIN e limites de atribuição

Além do C de referência, o overlay instalado foi desassemblado somente para
leitura com binutils MIPS da imagem local, sem alterar/recompilar o PS1:
`data/A_FILE/11_GF_FARM.bin`, base 0x800b7e20. Foram conferidos, entre outros,
`thrown_item_resolve_target` em 0x800cc6a8 e `shop_menu_open_choose` em 0x8011b840.
A segunda rotina confirma a instrução de mensagem 2/0x46 na seleção.

O manifesto instalado marca o disco como modificado. O overlay instalado tem
SHA-1 `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`, diferente da referência do
repositório `664eee540550138d6e3717bb7e1593aeb8fe9d38` (também presente no
build PS1 local). O trecho consultado de `thrown_item_resolve_target` não contém
uma escrita de colocação na célula: ele identifica o layout/célula e segue o
descarte. Assim, o retorno solicitado foi adaptado do ramo já existente no
porte e do catálogo/campos originais; **não é declarado como tradução literal
comprovada daquele trecho binário**. Os dados modificados não são tratados
como prova de equivalência integral ao disco de referência.

O disassembly fica somente em `/tmp/hmbtn-original-farm-disassembly.txt`.
Nenhum BIN, SDK, imagem ou recurso proprietário foi adicionado ao Git.

## Validação

- Configure/build Linux Debug/Release e Windows x64 MinGW Release aprovados.
- Regressão Debug: **17/17**, 6,82 s, protegendo inventário, saves, lojas,
  criação/produção, campo/estufa, portas, chuva, cultivo e remessa.
- Seleção final dirigida: `native_farm_carry`,
  `native_world_fidelity_playthrough` e `native_livestock_sale_playthrough`,
  com mensagens/cursores, 18 travessias reais, retorno ao campo, persistência
  do objeto devolvido e quebra fora da grade. A venda termina com save
  carregado pelo aplicativo em outro processo.
- Release: unidade de campo, unidade da venda e percurso de fidelidade.
- Unidade Windows de venda e percurso Windows de fidelidade executados pelo
  Wine. Isso não valida MSVC ou Windows real.
- Capturas do mercado, de Barley, da passagem da pousada e de pedra retornada/
  descartada inspecionadas. Orçamento conservador de até 50 processos de teste
  respeitado, incluindo tentativas, wrappers/filhos e Wine/aplicação.

Na primeira tentativa, o alvo de queda da fixture continha outro objeto do
layout: o teste passou a declarar explicitamente as células livres. A inspeção
também levou a exigir piso válido após a caminhada, em vez de aceitar somente
mudança de coordenadas. Suíte inteira, Windows real, instalação limpa, sessão
manual de áudio/janela e sanitizers não foram executados.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(farm_carry|world_fidelity_playthrough|livestock_sale_playthrough)$'
```

Logs: `/tmp/hmbtn-room-shop-tests.log`,
`/tmp/hmbtn-room-shop-final-tests.log`, `/tmp/hmbtn-room-shop-release-tests.log`,
`/tmp/hmbtn-room-shop-windows-build.log`, `/tmp/hmbtn-room-shop-windows-world.log`
e `/tmp/hmbtn-room-shop-windows-sale.log`. Capturas em
`/tmp/hmbtn-room-shop-shots/` e `/tmp/hmbtn-barley-cursor-shots/`.

O bootstrap nativo não executa o jogo; o runtime jogável permanece parcial.
O build host não substitui toolchain PS1 nem validação SHA-1 do original.
