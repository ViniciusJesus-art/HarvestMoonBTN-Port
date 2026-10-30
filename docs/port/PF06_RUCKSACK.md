# PF-06 — Compra das ampliações de mochila

Recorte de 2026-10-29 do [ROADMAP.md](ROADMAP.md). As duas ampliações agora são
compradas no balcão original do supermercado. Capacidade, navegação, inventário,
economia e apresentação reutilizam a base existente. Nenhum arquivo de `src/`,
SDK ou recurso proprietário foi alterado.

## Origem C/BIN

`people/player.c:player_act_zone_front`, caso `ZONE_SHOP_COUNTER`, procura a
zona **37** a 224 unidades e o registro do objeto a 400 unidades na direção do
personagem. `shop_key_value_lookup` mapeia os itens **0x6d e 0x6e** para o pedido
**9**, `shop_grant_rucksack_upgrade`. Zona **36** é entrega, não compra.
A adaptação compartilha essas sondas e as guardas atuais do supermercado com
as sementes: área lógica **180**, local **30**, Jeff presente, **09:00..16:59**,
fechado domingo/terça e mãos vazias. Mochilas funcionam também no inverno.

O registro **115** de `farm_tables.c:estate_records` começa com item **0**, não
0x6d. A flag **0x8000** faz `estate_items_restore` substituir o item pela oferta
válida ou desabilitar o objeto. A adaptação resolve esse placeholder para a
mesma chave de compra; disponibilidade e arte consultam o inventário vivo.
Posição original **(1311,-200,-1000)**, com deslocamento de desenho
**(+150,-100,-150)**. A mesma peça do balcão muda de versão após o intervalo.

| Regra | Primeira ampliação | Segunda ampliação |
|---|---|---|
| Item | 0x6d | 0x6e |
| Preço de `item_info` | 3.000 G | 5.000 G |
| Nível de mochila | 0 → 1 | 1 → 2 |
| Capacidade acessível por bolsa | 2 → 4 | 4 → 8 |
| Oferta | Nível 0 | Nível 1 e contador zero |
| Descrição | Banco 2, 0xd4 | Banco 2, 0x248 |
| Arte do supermercado | Sprite 8 | Sprite 9 |

`milestone_set_status_bits` libera a primeira oferta quando o nível é zero;
a segunda exige nível diferente de 2 e `rucksackTimer<=0`. `shop_item_grant`
põe o contador em **10** a cada compra. `milestone_tick` o reduz uma vez por
manhã; ao chegar a zero a oferta já pode abrir. São **dez manhãs**, sem a chamada
extra do protocolo de ferramentas de Saibara.

`shop_item_grant` mostra primeiro o item com `shop_item_carry_step` /
`event_cue_issue`, espera o fim da ANI e abre a descrição. Cross **ou Circle**
após a descrição abre Yes/No em **(200,128)**; essa primeira tecla não compra.
No/Circle na resposta mostra **2:0xba**; saldo insuficiente mostra **2:0x4f**.
Aceitar com saldo mostra **2:0xbb**, cujo fechamento precede o débito e a
mudança de capacidade. Cancelar/recusar devolve o objeto ao balcão sem cobrança.

`player_start_rucksack` toca o som **8**, deixa de mostrar o item carregado e
usa a ANI base **29+direção**. Ao terminar, limpa o registro da mão. A adaptação
usa `event_item` para essas fases, mantendo relógio/input/save bloqueados durante
a apresentação. A capacidade é confirmada uma vez pela compra; a ANI não a
incrementa novamente. Nenhum objeto de mochila entra na bolsa ou no equipamento.

Evidência binária local:

- Overlay instalado `A_FILE/11_GF_FARM.bin`, SHA-1
  `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`, base LE **0x800b7e20**.
- `shop_grant_rucksack_upgrade` **0x80119414**: itens **109/110**, mensagens
  **0xd4/0x248** e modo **1**; `shop_item_grant` **0x8011c298**.
- Em **0x8011c570** chama `player_start_rucksack` **0x800c4fdc**;
  **0x8011c584..0x8011c5a8** incrementa/limita o nível e grava o timer **10**.
- `milestone_set_status_bits` **0x801124e8**; `milestone_tick` **0x80111f88**.
- `estate_items_restore` **0x800f5404**, `estate_code_status` **0x800f5ee0**:
  disponibilidade e sprites **8/9** do pack original do supermercado.
- Executável instalado `disc/SLUS_011.15`: catálogo em **0x8004c2ac/0x8004c2d4**,
  registros de 40 bytes com flags **0x0200**, categoria **3**, picture **-1** e
  preços **3000/5000**. Não se tenta carregar uma figura genérica inexistente:
  a apresentação toma emprestados os sprites do supermercado já carregado.
- Registro 115 em **0x8004f7cc**, 24 bytes: local **30**, item **0**, flags
  **0x841f** e posição **(1311,-200,-1000,0)**, conferidos como inteiros LE.

O build host não substitui a toolchain nem a verificação SHA-1 do build PS1.
Não se afirma igualdade do overlay instalado com a referência PS1.

## Adaptação e estado

`inventory.c` mantém oferta, compra e redução diária do contador no mesmo
inventário que já possui `rucksack_level`. O débito usa a economia central e sua
contabilidade. Bolsa cheia não impede comprar; nenhum registro de ferramenta,
quantidade, modificador ou item é descartado ao aumentar a capacidade.

`feed_shop` reaproveita o modal, mensagens e Yes/No existentes; sementes e
mochila compartilham o reconhecimento do balcão. `exploration` pede as duas
animações ao controlador existente. `scene_preview` somente resolve o sprite
exposto/carregado conforme o estado; desenhar não compra nem reduz o contador.

O codec passa a **v17** para incluir `inventory.rucksack_mornings_left`.
Validação: nível 0..2, contador 0..10 e contador zero quando o nível é zero.
A mesma notificação diária idempotente reduz o intervalo, inclusive em avanço
de tempo e após retomada. Não foi aberto trabalho de compatibilidade entre
versões de save.

`runtime.c` foi comparado integralmente com o estado inicial deste recorte:
**idêntico**. As animações de portas modificadas pelo usuário foram preservadas.

## Validação e limites

- Debug: unidade inicial e percurso com capturas aprovados; seleção **7/7**
  (mochila, inventário/regras, inventário/estado, menu, item, save e sementes);
  wrapper com três retomadas em aplicações novas **1/1**.
- Regressões Debug **5/5**: batata com compra de sementes/retomada,
  inventário/interação, portas/chuva, fidelidade do mundo e acessórios com
  três retomadas. O reconhecimento compartilhado do balcão conserva as sementes.
- Release: build e **2/2**, unidade e wrapper com três retomadas aprovados.
- Windows x64/MinGW: build local sem rede aprovado; no Wine, unidade, percurso
  e três retomadas independentes aprovados, todos exit 0. Avisos Mesa/EDID/wasapi
  não impediram a execução. Janela/áudio em Windows físico não foram validados.
- Capturas inspecionadas: objeto no balcão, descrição e preço, Yes/No,
  animação de vestir e oito espaços acessíveis de itens/ferramentas.

**45 processos de teste**, contando lançadores e filhos: unidade inicial 1,
percurso com capturas 1, seleção de regras 8, wrapper Debug 6, regressões 12,
Release 7 e Wine 10 (lançador + alvo). Sem suíte inteira; limite de 50 respeitado.

`rucksack_test.c` usa o registro original e testa limites estritos da sonda,
fechamentos/presença/mão ocupada/zona de entrega, duas ofertas/preços, Circle
antes de Yes/No, No, falta de dinheiro, fechamento antes do débito, bolsa cheia,
registros preservados, três capacidades, ANI/som, compra única e dez manhãs
idempotentes com retomadas e validação do contador.

`rucksack_playthrough.c` começa no inverno com capacidade inicial; compra as
duas mochilas no balcão real, cancela e recebe recusa por saldo em cada nível,
mostra e veste os itens com ANI real, dorme dez noites pelo fluxo comum e retoma
no meio da espera. Navega o menu real até o último espaço de ferramentas/itens
nos três níveis, troca registros e verifica os novos espaços também pelas APIs
compartilhadas dos depósitos. As capturas usam os sprites instalados.
Dinheiro e conteúdo comum das bolsas são fixtures; nenhuma ampliação é concedida.
Loads diretos encurtam deslocamentos sem relação com a compra. O wrapper retoma
primeira compra, espera de cinco manhãs restantes e estado final em processos novos.

Reprodução, selecionando apenas os testes pertinentes ao orçamento da rodada:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_rucksack(_playthrough)?$'
```

A seleção acima consome **7 processos**. Logs em
`/tmp/hmbtn-rucksack-{unit,process,regression,release-tests,wine-unit,wine-playthrough}.log`;
retomadas Wine em `wine-{level1,waiting,complete}.log` com o mesmo prefixo.
Capturas em `/tmp/hmbtn-rucksack-shots/`, fora do Git.

Cesta, embrulhos, notificações do jornal, relacionamentos completos e festivais
seguem em recortes próprios. Próximo: compra, transporte, enchimento e remessa
da cesta original. O bootstrap `--diagnostics` não executa o jogo; o runtime
permanece parcial.
