# PF-06 — Vinícola: vinho e suco de uva

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). Manna agora abre a compra de
vinho/suco pelo bytecode original da conversa. A adaptação usa o modal de
lojas, saldo/inventário, apresentação ANI, geladeira, bebidas e cozinha já
existentes. Nenhum produto é concedido para contornar a aquisição.

## Referência C e BIN

`menu/shop_dialogues.c`: `shop_wine_menu_setup`, `shop_pay_step`,
`shop_menu_open_choose` e `shop_item_carry_step`. O menu é uma lista de duas
linhas, com preço unitário; selecionar compra um item, sem pergunta Yes/No.

`event/event_commands.c`: `(8,3,0,0)` solicita `SHOP_DIALOGUE_PAY` (19).
A conversa do **NPC 22 / Manna**, intervalo **0x758000..0x758800** de
`12_event_s.bin`, entrada marcada 0, contém 119 instruções. Os passos 2..7
exigem mão vazia, dia diferente de **6 (sábado)** e horário **09:00..11:59**;
disparam a compra e aguardam o fechamento antes de concluir a conversa.
Rotas, presença, seleção do alvo, guardas da porta e calendário permanecem
nos componentes existentes. O acesso vem da área **32** (local 34) à casa da
vinícola **144** (local 36), evento **13**; área 148 é o outro interior ligado
à casa. Os cabeçalhos/zonas foram lidos no `00_gf.bin` instalado.

Auditoria local LE em `A_FILE/11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Conferência |
|---|---|
| **0x8011b210** | `shop_wine_menu_setup`; imediatos dos IDs **0x9f/0xa0**, descrições **0xb2/0xb3** e colunas **11/16**. |
| **0x80119af0** | `shop_pay_step`; entrada **0xb0**, cancelamento **0xb1**, compra **0xb4**, reabertura **0xb5** e falta de saldo **0x4f**. |
| **0x80119cf4..0x80119d20** | Consulta de preço, débito antes da confirmação **0xb4** e transição para a apresentação do item, correspondentes ao C extraído. |

O catálogo de `disc/SLUS_011.15`, endereço **0x8004b1a4**, passo **40 bytes**,
confere flags **0x9400**, categoria **0**, figuras **45/46**, classe
**0xc0000000**, callback **0x800d622c** (`farm_clamp_status`) e valores abaixo.
Os endereços são referência de auditoria; o host usa IDs/dados e não converte
callbacks serializados em ponteiros nativos.

| Produto | ID | Preço | Stamina | Fadiga | Felicidade |
|---|---|---|---|---|---|
| Vinho | 159 | 300 G | 0 | -5 | +1 |
| Suco de uva comprado | 160 | 200 G | +5 | -2 | 0 |

Suco de fruta preparado é **180**, com flag **0x800** e recuperação de stamina
a partir da qualidade do registro, fadiga **-4** e figura **63**. O suco comprado
160 usa o valor fixo **+5**, pois não tem essa flag. Não são intercambiados.

## Adaptação

- `generate_farm_tables.py` extrai os dois nomes, ordem, IDs e descrições do
  menu original para `wine_menu.inc`. Preços vêm do catálogo comum.
- `village_talk` aceita o despacho original de Manna. A conversa continua
  responsável pelos horários e pelas condições de acesso. O checkpoint de
  inventário depois da compra segue a regra comum das outras lojas.
- `feed_shop` adapta os estados de `shop_pay_step`: abertura **2:0xb0**,
  prompt **2:0x46**, Triangle para descrição, Circle para **2:0xb1** e saída.
  Sem saldo, mostra **2:0x4f**, **2:0xb5** e retorna à mesma seleção, sem perda.
- Cross com saldo suficiente debita **uma vez, imediatamente**, espera o
  fechamento de **2:0xb4**, apresenta pela ANI e entrega um registro na mão
  com modificador zero/quantidade um. Mochila cheia não impede a entrega;
  mão vazia é a condição original da conversa. O estoque é repetível.
- A UI compartilha a lista/preços/cursor usados na clínica. Não há outro
  inventário, moeda, estoque persistido ou controlador de energia.
- Guardar/retirar e consumir usam os caminhos existentes. Square executa a
  bebida pelo banco **14**, range **0**, som **75**; efeitos e remoção ocorrem
  no fim da ANI, respeitando os limites de stamina/fadiga/felicidade.

O codec v19 existente já representa os itens e atributos necessários. Não
foi alterado neste recorte. `runtime.c` permanece idêntico ao início do
trabalho; animações de portas do usuário preservadas. Não houve edição de
`src/`, SDK, disco ou fluxo CMake PS1. A validação host não substitui a
toolchain nem a verificação SHA-1 original.

## Verificação funcional

`wine_shop_test` cobre as duas linhas, preços e descrições, cancelamento,
saldo insuficiente/reabertura, proteção contra seleção durante revelação do
texto, momento do débito/ausência de repetição, quatro direções de apresentação,
mochila cheia, depósito/retirada na geladeira, efeitos distintos de vinho/suco,
bônus da receita e retomada de estado. O início de outra loja durante o modal
é recusado; empréstimos do estado da sessão são mantidos pelo reset comum.

`wine_shop_playthrough` usa recursos instalados e controles reais:

1. Entra pela porta da vinícola, conversa com Manna, cancela e tenta comprar
   sem saldo; o retorno ao menu mantém saldo/inventário.
2. Compra vinho e suco por **300/200 G**, confere débito único e ANI, e guarda
   ambos pela UI da geladeira. Grava/reabre o checkpoint das compras.
3. Compra outro suco, bebe-o (**+5/-2**), retira o vinho da geladeira, grava/
   reabre com ele na mão e bebe (**0/-5/+1**). Não concede bebidas ao teste.
4. Avança pelo calendário até o outono, coleta uva de um registro real e
   guarda pela mochila. Na cozinha, seleciona essa uva e o suco comprado que
   permaneceu na geladeira: mixer/receita/fade/XA originais produzem suco de
   fruta **180**, qualidade **23** (**20 + bônus 3** do suco 160).
5. Bebe o resultado pela ANI real e verifica stamina **78**, fadiga **9**,
   felicidade **11**, saldo **300 G**, ingredientes consumidos uma única vez
   e três checkpoints estáveis em processos novos do aplicativo.

Saldo inicial **1.000 G**, casa ampliada, mixer e atributos iniciais são
fixtures explícitas. Uvas são coletadas, bebidas compradas e resultado
preparado pelo fluxo real. Capturas locais conferem menu/cursor, descrições,
recusa, entrega e cozinha; não são adicionadas ao Git.

Configure/build Debug e Release aprovados. Debug **15/15** selecionados,
incluindo vinícola e clínica com seus percursos/retomadas, consumo, comandos,
inventário, lojas, saves e regressões de chuva/portas/fidelidade. Release
**4/4** selecionados, incluindo vinícola/percurso/três retomadas. Build Windows
MinGW aprovado em contêiner local sem rede; Wine aprovou unidade, percurso
completo e as três retomadas pelo aplicativo Windows. Capturas do Windows
conferem também a lista/preços/cursor mantidos durante a descrição.
Todas as execuções finais terminaram com saída **0**.

**46 processos de teste**: três avulsos iniciais, 24 da seleção Debug, nove da
seleção Release e dez Wine/targets. Inclui uma tentativa inicial do percurso
que usou a área errada na fixture; corrigida com a localização de Manna e as
zonas do BIN, sem exigir alteração de gameplay. Conta CTest, wrappers, filhos
e lançadores; teto de **50** respeitado, sem suíte inteira. Avisos de Mesa/EDID
do Wine não impediram a validação. Avisos antigos de `const` nos testes de
mundo permanecem fora deste recorte. `git diff --check` aprovado e comparação
de `runtime.c` com o início confirma preservação integral.

## Limites

Este recorte cobre a compra com Manna e os usos descritos. Vindima, emprego de
Cliff e demais eventos da vinícola não são declarados completos. Refeições/
bebidas da hospedaria, festivais e progressão restante continuam em PF-06.
O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.
Wine não equivale à validação numa máquina Windows física.
