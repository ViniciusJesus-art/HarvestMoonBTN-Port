# PF-06 — Obra e cochos do celeiro ampliado

Registro histórico da obra com save v7. A coleção/compra/alimentação de animais
foi integrada posteriormente em [PF06_LIVESTOCK.md](PF06_LIVESTOCK.md), com save
v8 atual. As ausências de criação abaixo descrevem o estado desta entrega.

Entrega de 2026-10-27. A fila principal continua no [ROADMAP.md](ROADMAP.md).
**Obra, mapas e abastecimento concluídos neste recorte. Criação de vacas e
ovelhas ainda ausente; o aceite completo do celeiro permanece aberto.**

## Resultado e fronteira da entrega

Depois do galinheiro e da primeira casa, Gotz oferece a ampliação do celeiro por
**6.800 G e 500 madeiras**. A conversa original 374 abre o mesmo modal existente,
com descrição 2:0x136, oferta 2:0x13E e confirmação 2:0x5F. Cancelar, não ter
dinheiro/material ou tentar pedir durante uma obra não causa débito parcial.
Quatro manhãs concluem a obra; salvar/carregar no prazo não o reinicia.

A fazenda lógica 0 passa a usar recursos 12..15 e o celeiro lógico 96 usa
100..103 conforme a estação. Casa 80 e galinheiro 112 continuam nas variantes
já concluídas. Modelos, colisões, portas, zonas e texturas são os recursos
originais instalados, sem duplicar área nem reinicializar campo/inventário.

Os cochos normais acessíveis passam de dez a vinte. O conjunto original também
tem um cocho de maternidade no celeiro inicial (índice 20) e dois no ampliado
(20/21): máscaras reais **0x1003FF → 0x3FFFFF**. Isso não significa 22 animais.
Retirar/devolver feno, abastecer os dez novos cochos, apresentar os sprites de
feno, atravessar portas com o item na mão e limpar os cochos no dia seguinte
usam o mesmo gameplay já existente. O desenho agora filtra os registros pelas
zonas ativas da seção: não mostra feno de um cocho que não existe naquele mapa.

Vacas/ovelhas não são criadas, compradas ou simuladas nesta entrega. A auditoria
confirmou que `port/game/poultry.c` atende galinhas, enquanto as rotinas bovinas/
ovinas ainda estão somente na decompilação. O limite original de dez/vinte
animais foi identificado em `livestock_find_free`, mas não foi exposto como uma
capacidade supostamente jogável sem seus consumidores. Maternidade, máquinas,
leite, lã, reprodução e ciclo de vida seguem pendentes. O abastecimento e a
limpeza diária entregues não equivalem a alimentar animais ainda não portados.

## Origem e estratégia

`F` = `src/overlay/11_GF_FARM`; `P` = `port/game`. Nada em `src/` foi modificado.

| Origem | Adaptação/integração | Dependências e verificação |
|---|---|---|
| `F/menu/shop_dialogues.c`: `shop_silo_offers`, `shop_first_clear_flag`, `shop_lumber_menu_setup`, `shop_lumber_deal_step` | `P/construction.c/.h`, `feed_shop.c`, `port/graphics/feed_shop_ui.c`, `port/CMakeLists.txt` | Terceira oferta já extraída no configure; habilita somente a obra integrada. Nome `enlarge_barn` extraído da tabela de texto original; mesmo modal, estoque e economia. Cancelamento/falhas/ocupação testados. |
| `F/event/milestones.c`: `milestone_arm_countdown`, `milestone_tick` | `P/construction.c`, `daily.c` existente | Mesmo prazo de quatro manhãs; novo tipo pendente `HMBTN_BUILD_BARN`. `daily.c` não precisou mudar: chama a mesma conclusão de obras. Teste com datas repetidas e reinício a cada manhã. |
| `F/world/scene.c`: `scene_resolve_selection`, `farm_layout_index`, `house_level_offsets`, `barn_layout_offsets` | `P/world_map.c/.h`, `port/graphics/scene_preview.c` | Fazenda = 8 + 4×nível do celeiro + estação após casa1; celeiro = 96 + 4×nível + estação. Mantém IDs lógicos; testa entrada/saída e restauração nas quatro estações. |
| `F/people/player.c`: `player_zone_fodder_exchange`, `player_zone_place_fodder`; `F/animals/animals.c`: `farm_feed_bin_set`; `F/estate.c`: `estate_feed_bins_clear`, registros de cocho | `P/farm_feed.c/.h` existente, helper `hmbtn_farm_feed_bins`, `port/graphics/scene_preview.c` | Ações, posições, ANI, estoque e flags existentes reaproveitados. Helper consulta zonas originais para desenhar apenas cochos ativos; não guarda capacidade ou cópia de nível. Índices 10..19 usados no percurso real. |
| Flags 0x38/0x3C, `event_cmd_store_levels` e pontuação de `F/menu/status_pages.c` | `P/village.c`, `village_talk.c`, `farm_menu.c` | Flag do celeiro e comando 95 derivam do nível autoritativo; recusa escrita direta no flag. Pontuação deriva 60 para galinheiro+casa+celeiro. Sem total persistente paralelo. |
| `F/animals/animals.c`: `livestock_find_free`; `F/animals/livestock.c`: inicialização, atualização e ações; `F/menu/shop_dialogues.c`: `shop_barn_step` | **Ainda não adaptados** | Dependência da próxima entrega: aquisição/nomeação, registros reais, colocação/arte, capacidade 10/20 e alimentação antes de limpar os cochos. Não criar outra lógica para as regras já disponíveis no original. |

Nenhum módulo original C é compilado diretamente no host por esta entrega.
Os blocos já adaptados e as tabelas extraídas são estendidos; renderização, áudio,
instalação por BIN e backends existentes são preservados. Custos/quantidades de
conteúdo não são uma exigência de emular limites de hardware PS1.

## Estado e persistência

- `HmbtnSession.construction.barn_level` é a única autoridade do nível do celeiro;
  a mesma estrutura possui tipo de obra, prazo e nível da casa.
- Feno permanece em `game.inventory.supplies[0]`, madeira em `[1]`, item em
  `game.inventory.held_item`, cochos em `session.feed.barn`, dinheiro em
  `game.economy`. A UI não possui cópia desses dados.
- Flag 0x3C, comando 95, resolução de mapas e pontos do status consultam o nível.
- Save **v7 aceita somente v7**, acrescenta nível do celeiro em AUXI e persiste
  `HMBTN_BUILD_BARN`. Valida níveis 0/1, pré-requisitos e coerência entre obra e
  prazo. Depósitos, receitas e demais campos v6 continuam serializados no formato
  atual, sem migrador para versões anteriores.
- Segunda casa/estufa continuam ocultas na loja até sua integração funcional;
  depois da obra do celeiro, Gotz continua vendendo madeira.

## Testes executados

Linux Debug, recursos já instalados pelo usuário:

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado. A recompilação completa
  apresentou avisos preexistentes de qualificadores `const` na fixture
  `world_map_playthrough.c`; produção compilou sem erro.
- Primeira seleção: **8/8 CTests**, 0,03 s — novo teste do celeiro, casa,
  construção, saves, mapa, conversas, cochos e menu.
- Regressões relacionadas: **20/20 CTests**, 19,09 s — saves/arquivos e retomada
  do app, estado/sessão/calendário/inventário/scripts, campo/grama, novos percursos,
  casa/carpintaria, chuva/portas, conversas, galinhas, loja e interiores.
- Após ajustar somente a captura para esperar o primeiro tick livre depois da
  animação de feno: percurso do celeiro novamente aprovado, inclusive retomada
  pelo app em outro processo (**1/1 CTest**, 1,21 s).
- Total: **49 processos de teste**, incluindo wrappers/filhos, três execuções
  diretas do percurso e uma tentativa inicial que identificou falta de tick de
  colocação de Gotz na fixture. Nenhum ator fictício foi criado para contornar
  sua agenda. Limite de 50 respeitado; não executada a suíte inteira.
- Capturas inspecionadas: oferta/descrição, nome de menu original, fazenda,
  interior e feno nos novos cochos. Capturar não modifica gameplay/obra/cochos.

Novos alvos: `native_barn_upgrade` e `native_barn_upgrade_playthrough`.
O primeiro também testa comando 95, rejeição de saves incoerentes e vínculos
após carga. O segundo usa a conversa real de Gotz, quatro noites, reload no prazo,
cochos novos, portas com feno, limpeza diária, campo/depósitos e quatro estações.

Não executados: Windows/cross-build, Release, sanitizers, Android, instalação
limpa e sessão manual com janela/áudio. Não há validação de gado, pois ainda
não existe domínio nativo correspondente.

## Como experimentar

```sh
ctest --preset native-debug -R '^native_barn_upgrade(_playthrough)?$' --output-on-failure
```

Para gerar uma partida de inspeção com as obras anteriores já satisfeitas,
dinheiro/material preparados e o percurso completo executado:

```sh
./build/native-debug/port/hmbtn_barn_upgrade_playthrough data 0 /tmp/barn-current.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/barn-current.hms
```

O save termina na casa ampliada, no inverno, após testar as quatro estações;
o celeiro pode ser visitado normalmente. Não é uma opção de aquisição gratuita
no jogo. `--area-preview 96` isoladamente cria estado inicial e não concede a
obra; saves v6/anteriores exigem jogo novo. Nenhum recurso proprietário novo foi
adicionado ao Git.

## Arquivos desta entrega

- Gameplay: `port/game/construction.c/.h`, `feed_shop.c`, `world_map.c/.h`,
  `farm_feed.c/.h`, `farm_menu.c`, `village.c`, `village_talk.c`, `save.c/.h`.
- Integração/apresentação: `port/CMakeLists.txt`,
  `port/graphics/feed_shop_ui.c`, `scene_preview.c`.
- Testes: novos `port/tests/barn_upgrade_test.c`, `barn_upgrade_playthrough.c`;
  ajustes em `construction_test.c`, `farm_feed_test.c`, `house_upgrade_test.c`.
- Documentação: este relatório, `ROADMAP.md`, `ROADMAP_STATUS.md`,
  `ROADMAP_PENDENTE_CODEX.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`,
  `PORTING_STRATEGY.md`, `PF05_NATIVE_SAVES.md`, notas nos relatórios de casa e
  carpintaria para identificar seus formatos históricos.

## Próxima implementação recomendada

Integrar a aquisição, presença e alimentação reais de vacas/ovelhas, adaptando
`livestock_find_free`, `shop_barn_step`, inicialização/arte e rotinas diárias,
antes de considerar o celeiro concluído. Consumo pelos animais deve ocorrer antes
da limpeza diária já existente. Uma única coleção deve ser usada por loja,
ações, status, renderer e saves; a obra entregue não exige outra refatoração
arquitetural geral. Produção/reprodução e demais cuidados continuam na PF-06,
com critérios próprios no roadmap.
