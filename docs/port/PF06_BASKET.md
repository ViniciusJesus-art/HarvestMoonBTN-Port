# PF-06 — Cesta de remessas

Recorte concluído em 2026-10-30 do [ROADMAP.md](ROADMAP.md). A cesta é comprada
no supermercado, aparece na fazenda e pode ser transportada, deixada no chão,
preenchida e esvaziada na remessa. Usa o inventário, a economia, as portas e o
relógio existentes. `src/`, SDK, discos e recursos proprietários não foram alterados.

## Auditoria C/BIN

`shop_key_value_lookup` resolve o registro **117**, item **100/0x64**, para o
pedido **10**, `shop_basket_step`. A compra usa `shop_item_grant` no modo
`SHOP_GRANT_ITEM`: primeiro mostra a cesta com ANI, depois descrição/Yes-No,
aceite, fechamento, débito e colocação do objeto na fazenda.

| Regra | Original e adaptação |
|---|---|
| Preço | 5.000 G, da linha `ITEM_BASKET` em `item_info` |
| Descrição | Banco 2, 0xd5 |
| Confirmação | Yes/No em (200,128), mesmo modal da mochila |
| Cancelamento/saldo | 2:0xba / 2:0x4f, sem posse nem cobrança |
| Aceite | 2:0xbb; débito único depois do fechamento |
| Entrega | Fazenda, posição (-5000,0,2300), mão livre |
| Posse | Uma cesta; não pode ser guardada na mochila, equipamento ou depósitos |
| Capacidade | 30 produtos; valor e 25 contagens de remessa |
| Pegar/deixar/inserir | Cross, com ANI base 11+direção / 23+direção |
| Esvaziar | Square junto à superfície de remessa 0x80; conserva a cesta nas mãos |
| Coleta | Transferência à remessa, pagamento pelo relógio comum às 17:58 |

O alcance direto segue `farm_record_hit_test` e as dimensões do catálogo:
**400 por eixo X/Z**, com limites estritos e sonda frontal de **224** unidades.
A captura de um arremesso usa a caixa menor de `estate_request_near_active`:
**200 em X/Z e 20 em Y**, também estrita, no ponto de aterrissagem. Os dois
alcances não são intercambiáveis. Deixar usa o ponto frontal de **200** unidades
(`player_facing_point`, modo 4), conferido contra piso/solo/ocupação.

**Cesta cheia:** `estate_basket_add` recusa a inserção sem alterar os 30 produtos
existentes. O chamador original `PLAYER_MODE_BASKET_ADD` mostra **3:0x383** e
chama `player_drop_held_item`, que limpa o produto tentado. O arremesso também
é retirado ao atingir a cesta cheia. A adaptação conserva esse descarte do
excedente; não devolve o item à mão nem aumenta o conteúdo. Isso corrige o
critério anterior de não perder o excedente, que não correspondia ao C/BIN.
Itens sem preço não entram na cesta.

`estate_basket_add` acumula `item_sell_price`, incluindo o bônus de mel quando
ativo **na inserção**. `estate_basket_merge` transfere valor e contagens para a
mesma remessa, limita cada contador a **999** e limpa o conteúdo da cesta uma
vez. A transferência não concede dinheiro imediatamente nem chama novamente a
venda de cada item. Como no original, peixes na cesta aumentam o bin de peixes,
mas não o contador separado usado para a meta de 200 peixes da venda direta.

Evidência binária local:

- `A_FILE/11_GF_FARM.bin`, base LE **0x800b7e20**, SHA-1
  `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`.
- `shop_basket_step`, **0x80119470**; usa `shop_item_grant` e as flags existentes
  de estoque/posse. A entrega segue o ramo já auditado em `shop_item_grant`.
- `estate_basket_add`, **0x800f3e44**, e `estate_basket_merge`, **0x800f3ef0**;
  `estate_marker_stamp`, **0x800f43cc**; captura de arremessos em
  `estate_request_near_active`, **0x800f4994**.
- Executável instalado `disc/SLUS_011.15`, linha do catálogo em **0x8004c144**:
  flags **0x8010**, categoria **1**, regra de solo **16**, picture **-1**, preço
  **5000**, remessa **0**, classe/alvos **0x610** e extensões **400/400**.
- Registros originais **117** (estoque do supermercado) e **195** (cesta móvel).
  `estate_code_status` escolhe sprite **24** no supermercado;
  `item_code_to_picture` escolhe sprite **0** do pack **6** para o objeto móvel.
- Os handlers de `player.c` confirmam as ANI, som **9** ao inserir, **0x13** ao
  transferir para a caixa e supressão da imagem carregada durante o esvaziamento.

O build host não substitui a toolchain nem a validação SHA-1 do build PS1.
Não se afirma igualdade dos recursos instalados com o SHA-1 de referência PS1.

## Implementação e estado

`game.basket` é o único proprietário de posse, posição/área, conteúdo e valor.
Quando carregada, o registro da mão **100/0/1** referencia essa mesma cesta;
o objeto no chão deixa de ser apresentado e não existe uma segunda coleção.
`basket_action` só guarda fase de ANI, som, aviso e oscilação transitórios.

`feed_shop` reutiliza o modal de compra já usado pelas mochilas. Os recursos
originais são obtidos pelo cache de arte existente: sprite 24 do supermercado
para o estoque e pack 6/sprite 0 para a cesta carregada, no chão e nos menus.
Não há imagem gerada ou figura genérica substituta para o picture -1.

`basket.c` adapta os handlers com inventário/economia explícitos. `farm_carry`
recebe um empréstimo de `game.basket` para os arremessos. `farm_shipping` expõe
o mesmo preço/bin e a transferência das contagens/valor, sem repetir tabelas.
A colisão do objeto é consultada no controle de caminhada **e corrida**,
separadamente da opção existente que permite correr passando por moradores.
Portas conservam prioridade de interação e usam o transporte atual da mão.

O codec passa a **v18** com os campos persistentes da cesta, sem serializar
ponteiros ou fases. Valida posse/colocação/mão, contagem 0..30, soma de bins,
valor limitado pelo catálogo, área/posição e exclusão da cesta dos demais
slots/depósitos. Save durante ANI, aviso, arremesso ou transição continua
bloqueado; entre ações é permitido com a cesta carregada. O empréstimo ao carry
é refeito na sessão e depois do reset matinal.
Não houve trabalho específico sobre compatibilidade de saves.

`runtime.c` não recebeu alterações neste recorte. As animações de portas
modificadas pelo usuário foram preservadas.

## Validação

- Linux Debug: unidade e percurso com capturas aprovados; seleção **8/8** de
  cesta, inventário/regras, inventário/estado, item, save, controle, carry e
  remessa. Unidade repetida após tornar efetivo o vetor de corrida/colisão.
- Wrapper Debug **1/1**, com três retomadas em aplicações novas: cheia no chão,
  cheia carregada e esvaziada após pagamento. Percurso Debug final aprovado
  após separar o alcance direto de 400 da captura de arremesso de 200.
- Regressões Debug **5/5**: mochila com suas três retomadas, portas/chuva,
  fidelidade do mundo, inventário/interação e acessórios com três retomadas.
- Linux Release: build e **2/2**, unidade e wrapper com três retomadas aprovados,
  incluindo o vetor de pegar a cesta a 623 unidades (sonda a 399 do centro).
- Windows x64/MinGW: build local sem rede; Wine: unidade, percurso e três
  retomadas independentes aprovados, exit 0. Avisos Mesa/EDID/wasapi não
  impediram os checks. Windows físico não foi validado.
- Capturas temporárias do estoque, oferta, cesta carregada, cesta no chão,
  aviso de lotação e esvaziamento inspecionadas; não entram no Git.

**50 processos de testes**, incluindo lançadores, wrappers e filhos: unidade e
percurso iniciais 2; seleção de regras 9; unidade corrigida 1; wrapper Debug 6;
regressões 14; percurso Debug final 1; Release 7; Wine 10 (lançador + alvo).
Não foi executada a suíte inteira; o limite de 50 foi respeitado.

`basket_test.c` verifica compra/cancelamento/saldo/posse única, colocação inicial,
quatro direções de pegar/deixar, piso de remessa recusado ao deixar, bolsa e
depósitos bloqueados, limites de alcance/área, colisão a pé/correndo, 30 produtos,
recusa e descarte do excedente, contagens/valor, transferência única/clamp e
retomada com empréstimos reconstituídos. Conteúdo inválido é recusado pelo codec.

`basket_playthrough.c` compra a cesta no balcão real, cancela e recebe recusa
por saldo, pega na fazenda, atravessa a porta da casa e volta, deixa e reencontra
o objeto após trocar de área, insere 30 produtos, incluindo um arremesso real,
recebe o aviso de lotação e retoma cheia no chão/nas mãos. Square longe da caixa
não transfere; junto à caixa real transfere uma vez, conserva a cesta e paga
pelo relógio comum. Dinheiro e mercadorias de teste são fixtures; a cesta não é
concedida. Loads/posicionamentos diretos encurtam deslocamentos não relacionados.

Reprodução, selecionando apenas os checks pertinentes ao orçamento da rodada:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_basket(_playthrough)?$'
```

A seleção acima usa **7 processos**. Logs finais locais em
`/tmp/hmbtn-basket-{playthrough-final,release-tests,wine-unit,wine-playthrough}.log`
e `wine-{full,held,sold}.log` com o mesmo prefixo. Artefatos temporários de etapas
anteriores podem ser removidos pelo ambiente; resultados e limites estão aqui.

Cavalo, demais recipientes, embrulhos, festivais e a recuperação da cesta em
callbacks de mapas/obras ainda não adaptados seguem em recortes próprios.
O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.
Próximo: compra dos alimentos/ingredientes do supermercado pelo fluxo original.
