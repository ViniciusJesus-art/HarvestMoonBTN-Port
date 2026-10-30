# PF-06 — Embrulhos do supermercado

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). O balcão de embrulhos agora
transforma um produto carregado em presente por **100 G**, com mensagens,
animação, conteúdo, inventário e entrega pelo sistema de presentes existente.
Não há coleção separada de presentes nem alteração no C original.

## Origem C/BIN

`people/player.c:player_act_zone_front`, caso `ZONE_SHOP_COUNTER`, permite
mãos ocupadas somente para a chave **0x177**. `shop_key_values` mapeia essa
chave ao pedido **3**, `shop_screen_run_handlers[3] = shop_buy_item_step`.
O registro **118**, local **30**, posição **(1311,-200,-1400)** e flags
**0x041f** representa o serviço. `estate_code_status` seleciona seu sprite
**10** no pack do supermercado. O estoque não é ocultado pela compra.

O fluxo captura o ID do item da mão, consulta `item_is_pickup_not_tool` e
cobra o preço de `ITEM_GIFT`, não o valor do conteúdo. Esse predicado exige
`(itemFlags & 0x8100) == 0x8000` e categoria diferente de **1**; presentes já
embrulhados são recusados antes dessa consulta.

| Situação | Mensagem, banco 2 | Efeito |
|---|---|---|
| Mão vazia | 0xb8 | Sem cobrança/entrega |
| Presente já embrulhado | 0xbc | Preserva presente e conteúdo |
| Produto recusado | 0xb9 | Preserva o item |
| Produto aceito | 0xb6 | Aguarda fim do texto e abre confirmação |
| No/Circle | 0xb7 | Preserva o item, sem cobrar |
| Dinheiro insuficiente | 0x4f | Preserva o item, sem cobrar |
| Yes com saldo | 0xbb | Cobra após fechar a mensagem e apresenta o presente |

A confirmação usa `shop_result_update`, painel em **(224,128)**. Diferente
dos alimentos/mochilas, `shop_buy_item_step` abre a pergunta automaticamente
quando a descrição chega ao fim; não exige uma tecla intermediária. O débito
usa `estate_gold_change(-100,1)`, seguido de
`event_item_can_take(conteúdo,1,0,1)`. `shop_item_show_step` cria
`{id=ITEM_GIFT (164), modifier=ID do conteúdo, count=1}` e chama
`player_show_item`. A ANI é **11+direção**, com entrega na mão ao terminar.
A remoção original é por registro da mão, sem diminuir apenas uma unidade de
um eventual `count` maior que um; a compra normal do percurso fornece uma
unidade. Quantidade/modificador do produto não são codificados no embrulho.

Evidência local extraída dos recursos do usuário:

- Overlay `A_FILE/11_GF_FARM.bin`, base LE **0x800b7e20**, SHA-1
  **fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**.
- `shop_buy_item_step`, **0x80119008**: consulta preço do item **0xa4**,
  mensagens **0xb6..0xbc/0x4f**, painel **224/128**, pagamento
  **0x80119278..0x8011929c** e chamada da apresentação em **0x801192e0**.
- `shop_item_show_step`, **0x8011b784**: `stored_item_set` com ID **0xa4**,
  conteúdo no modificador e quantidade **1**; chama `player_show_item`,
  **0x800cb540**. Predicado original em **0x800d6090**.
- `disc/SLUS_011.15`: catálogo em **0x8004b1a4**, passo de **40 bytes**;
  presente com flags **0x8020**, categoria **0**, picture **-1**, preço
  **100**, venda **0**, classe **0x80000000**. Registro 118 em **0x8004f814**.
- Picture **-1** não significa ausência de arte: `picture_load_to_vram`
  carrega pelo ID, e o bloco ANI/BANK original de 2048 bytes existe em
  `00_gf.bin:0x3c6b000`. A adaptação usa esse mesmo cache por ID na mão/mochila.
- Conversa original de Gotz em `12_event_s.bin:0x759000..0x75a000`:
  pão possui reação **1**; o primeiro presente apreciado soma **3**, e o
  passo **132** subtrai **1** pelo embrulho. O passo **135** aplica o delta e
  **136** remove o presente. Resultado no percurso: amizade **50 → 52**.
  Preserva-se o bytecode instalado, inclusive a subtração, sem substituir por
  um bônus presumido a partir de outro helper C.

Leitura LE e endereços PS1 de 32 bits servem apenas à auditoria; não viram
ponteiros host. O build nativo não substitui a toolchain/verificação SHA-1 PS1,
nem declara identidade completa do overlay instalado com a referência PS1.

## Adaptação

`feed_shop` compartilha sondas de zona **37** a **224** unidades e registro a
**400**, limites estritos de **±150**, horário **09:00..16:59**, fechamento
domingo/terça e presença/local de Jeff. Somente o pedido de embrulhar dispensa
mãos vazias. O modal reutiliza descrição, confirmação, resultado e cue já
existentes, preservando relógio/input/save durante a transação.

O pagamento/remoção acontece uma vez, depois da aceitação. A adaptação confere
novamente saldo e conteúdo da mão antes de concluir; uma aceitação atrasada
não destrói outro produto nem autoriza saldo insuficiente. Cancelar e recusar
não limpam a mão. Não se exige espaço na mochila para receber o embrulho.

`event_item_show_record` adapta o parâmetro original de `player_show_item`:
a apresentação recebe o registro completo e conserva o modificador na entrega.
As chamadas antigas por ID continuam produzindo `{id,0,1}`. A arte de presente
usa o bloco original por ID já suportado pelo cache, sem figura substituta.
`scene_preview` também mostra o papel de embrulho do balcão, sprite **10**.

R2/mochila preservam ID/modificador/quantidade. A classe do presente permite
bolsa, mas não corresponde a geladeira, armário ou caixa de ferramentas; essas
recusas não movem nem destroem o registro. `inventory_item_message` compartilha
a regra de `storage_item_message`/`cooking_item_prompt_tick`: a descrição de um
presente é **0x3e8 + conteúdo**, no banco **3**. Mochila, móveis e cozinha
consultam essa mesma regra, evitando exibir o texto genérico do item 164.

`village_talk` já interpreta o modificador em seus comandos **57/58**. O
percurso entrega o presente ao Gotz usando presença/agendamento e conversa
original, com reação, consumo e amizade calculados pelo bytecode existente.
Nenhuma regra adicional de afinidade foi criada.

Mão/mochila/conteúdo/economia/afinidade já são representados no codec atual;
nenhuma mudança de formato ou trabalho de compatibilidade foi necessário.
`runtime.c` e as animações de portas do usuário ficaram intactos.

## Verificação

- `wrapping_test`: balcão/guardas, mãos ocupadas, inválidos/presente/sem item,
  No/Circle/saldo, débito/contabilidade após fechar, saldo/conteúdo alterados,
  remoção de um registro completo, ANI/modificador, bolsa cheia, depósitos,
  consulta da descrição e encode/decode sem alteração do inventário.
- `wrapping_playthrough`: compra real de pão por **100 G**, cancelamento/falta
  de saldo sem perda, pagamento único de **100 G**, ANI e recebimento do
  embrulho; tentativa de reembrulhar recusada. R2 guarda/retira, descrição do
  pão embrulhado na mochila e nos móveis e recusas reais de geladeira/armário.
  Gotz aparece pela agenda e recebe o mesmo produto, amizade **50 → 52**,
  consumo único e saldo **800 G**. Dinheiro, primeira casa e galinheiro são
  preparação explícita do teste; produto/presente não são concedidos.
- Checkpoints na mão, na bolsa e depois da entrega: comparação encode/reload,
  seguida de retomada em processos novos do app. Capturas locais conferem
  painel, ANI, presente e mochila; recursos/capturas não entram no Git.

Configure/build Debug e Release aprovados, além do build Windows MinGW no
contêiner local sem rede. Debug passou nos **14/14** selecionados da rodada
inicial (incluindo compras dos alimentos e conversas/presentes). Após integrar
a descrição do conteúdo, passaram as três unidades selecionadas e o percurso
completo/retomadas **1/1**. Houve uma tentativa intermediária de percurso que
aguardava insuficientemente o fechamento do texto; o driver passou a avançar
as páginas e aguardar o retorno à mochila. Não foi falha do gameplay.
Release aprovou unidade e percurso completos, com os três encode/reloads.
Windows/Wine aprovou unidade, percurso e três processos novos do app retomando
mão, mochila e entrega; todas as execuções finais tiveram saída **0**.

**49 processos de teste**: dois avulsos iniciais, 23 da seleção Debug inicial,
seis da seleção intermediária (incluindo a tentativa do driver que falhou),
seis da repetição final do wrapper, dois Release e dez Wine/targets. Inclui a tentativa que falhou,
CTest, wrappers, filhos e lançadores. Teto de **50** respeitado; suíte inteira
não executada. Avisos Mesa/EDID do Wine não impediram apresentar os checkpoints.
Os builds completos mantêm os avisos de `const` já existentes nos testes
`world_fidelity_playthrough`/`world_map_playthrough`; o recorte não os alterou.

## Limites

O recorte entrega embrulhar e integrar os presentes aos consumidores atuais,
sem declarar todos os eventos/recebedores/festivais portados. Comer diretamente
os alimentos permanece pendente; farmácia e outras lojas também. O próximo
recorte está definido no roadmap.

O bootstrap `--diagnostics` não executa o jogo; o runtime ainda é parcial.
Windows/Wine não valida uma instalação Windows física ou seus drivers.
