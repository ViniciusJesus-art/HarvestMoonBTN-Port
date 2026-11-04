# PF-06 — Lago da fazenda e ração de peixes

Recorte concluído em 2026-11-04, conforme o [roadmap](ROADMAP.md). O lago
recebe peixes pescados, usa ração comprada no supermercado, cresce/reproduz
nas manhãs alimentadas e permite retirar o maior peixe para uso ou remessa.
Pesca, inventário, compras, relógio, ANIs e remessa usam os sistemas existentes.

## Referência C/BIN

`field/pond.c` e `field/pond.h`: **99 vagas**, primeira vaga livre na entrada;
retirada prioriza grande **161**, médio **162**, pequeno **163**, depois a
ordem das vagas. As idades iniciais são **20/10/0** respectivamente.

`pond_daily_update` somente envelhece quando `POND_FED` está marcado e limpa
a marca depois da atualização. Peixes passam a médios aos **10** dias e
grandes aos **20**. Aos **30**, a idade volta a **20** e o número de filhotes é
`(rand_range(100) - população) / 25`, com divisão inteira assinada. Valores
negativos/zero não geram filhotes. O percurso é crescente pelas vagas: um
filhote inserido depois do pai pode envelhecer na mesma atualização; uma vaga
anterior não é revisitada. `animals.c: farm_end_of_day` chama essa atualização
depois dos outros animais. No porte ela fica no observer diário existente,
protegida pela mesma chave de data usada pelos demais domínios.

`people/player.c`: `player_act_zone_front`, `thrown_item_settle` e
`player_start_request`; `farm_probe.c: farm_status_kind`:

- Zona **10** retira uma unidade de ração do depósito, se a mão está vazia.
  O registro carregado é **132 / 0x84**, distinto da apresentação da loja.
- Zona **45** retira o maior peixe, exige mão vazia/face correta e bloqueia
  retirada no inverno. A restrição de inverno não foi aplicada ao ciclo
  diário nem inventada para o depósito.
- Superfície com bit **4** identifica o lago e tem prioridade sobre os outros
  tipos de água. Peixe lançado entra na primeira vaga livre; lago cheio mostra
  **3:0x382** e o peixe lançado é consumido conforme o original. Ração lançada
  marca o lago alimentado. Outro tipo de água não alimenta o lago.
- O pouso na água usa ANI de estágio **23 / 0x17**, som **16 / 0x10**.
  O recurso instalado contém cinco frames, seis ticks PS1 por frame. O porte
  avança a ANI decodificada no tick, desenha sem mutação e retira o efeito ao
  terminar. Descartes em solo e reposição de pedras/galhos nas células de
  plantio mantêm seu caminho existente. A flag da lula continua na praia.

No `00_gf.bin`, seção da fazenda na primavera **0x83800**: zona **13**, tipo
**10**, face **1**, depósito; zona **15**, tipo **45**, face **0**, retirada do
lago. A seção de inverno mantém essas zonas. A seção do supermercado, área
**180**, local **30**, começa em **0x2b06800**. O probe frontal de zona **37**
e o probe longo do registro de estoque resolvem a chave **132** para diálogo
**2**. Horários/folgas e presença de Jeff vêm da regra de acesso já usada
pelos produtos do supermercado.

`shop_dialogues.c`: `shop_fish_food_menu_setup`, `shop_buy_stock_step`,
`shop_quantity_input` e `shop_item_carry_step`. O catálogo de
`disc/SLUS_011.15`, endereço **0x8004b1a4**, passo **40 bytes**, confere
preço **20 G** e flags **0x8000** para a apresentação **108 / 0x6c**. O
registro do estoque/depósito continua sendo **132**, não o item de apresentação.

Auditoria LE do `11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Origem conferida |
|---|---|
| **0x8010c330** | `pond_add_fish`, limite 99 e primeira vaga livre. |
| **0x8010c3f8** | `pond_daily_update`, flag alimentado e percurso das vagas. |
| **0x8010c4b0 / 0x8010c4d4** | Marcar alimentação e retirar por tamanho. |
| **0x8010c550..0x8010c668** | Idades 0/10/20, tamanhos e reprodução aos 30. |
| **0x80118ce4** | Compra de ração: IDs 132/108, teto 999, mensagens e débito. |
| **0x8011a9dc** | Configuração do painel da ração. |
| **0x800f3a44 / 0x800f3ac4** | Adicionar/retirar estoque de ração. |
| **0x800c7ac0 / 0x800cc4e0 / 0x800bf3f4** | Zonas frontais, pouso do item e classificação da água. |

Endereços servem à auditoria, sem conversão para ponteiros host. `src/`, SDK,
recursos e fluxo CMake PS1 foram preservados. O build host não substitui
toolchain/verificação SHA-1 PS1.

## Integração

`HmbtnSession.pond` possui os registros, a marca de alimentação e um stream
nativo de RNG persistido, usando o gerador compartilhado. Não se declara
paridade com a ordem global de chamadas de RNG do executável PS1.
`farm_carry` e o resumo do menu tomam empréstimos desse mesmo estado. A
contagem exibida é derivada das vagas, sem segundo contador de população.

O estoque de ração permanece em `game.inventory.supplies[3]`. A compra usa o
modal `feed_shop`, o item de apresentação existente e o painel de quantidade:

- Abertura **2:0xd0**, estoque cheio **2:0xd1**, quantidade **1..999** com
  incrementos de 1/10 e retorno circular; prompt **2:0x4d**.
- Informação **2:0x25b**, falta de saldo **2:0x4f**, falta de espaço de estoque
  **2:0xd2**; voltam à quantidade sem alterar dinheiro/estoque.
- Cancelamento **2:0xba**; remove a apresentação e republica o produto.
- Aceite **2:0xd3**; somente depois de fechar a mensagem acrescenta o lote e
  debita uma vez. Reconfere saldo/estoque antes de efetivar. A apresentação
  **108** é retirada ao sair, inclusive sem compra; ela não é ração utilizável.

A figura de estoque **132**, sprite original **7**, fica oculta durante esse
atendimento e reaparece ao fechar. O depósito e a retirada de peixes usam a
fase/ANI de coleta do controlador de alimentação existente. A água usa um
empréstimo da ANI de estágio já carregada, sem outro carregador ou relógio.

O codec vigente foi adaptado para guardar vagas/idades/alimentação/RNG do
lago, mantendo o estoque existente e a exigência de gravação em estado estável.
Não foi feito trabalho de migração/compatibilidade entre saves. Os perfis de
debug, alterações locais do usuário e as portas foram preservados; `runtime.c`
é idêntico à cópia feita na retomada desta entrega.

## Verificação

`pond_test` cobre prioridade/primeira vaga/capacidade, ausência de alimentação,
fronteiras 10/20/30 dias, densidade, ordem dos filhotes, águas distintas,
depósito, bloqueio de inverno, lago cheio, aplicação diária única e retomada
dos registros/RNG/empréstimos. `fish_food_shop_test` cobre acesso/folgas,
apresentação, preço, quantidade, informações, cancelamento, saldo/estoque
insuficientes, estoque cheio, aceite atrasado e cobrança única.

`pond_playthrough`, com recursos instalados e controles reais:

1. Obtém a vara pela conversa de Greg e pesca até obter um peixe pequeno.
2. Guarda o peixe, cancela a compra de ração, tenta comprar sem saldo, lê as
   informações e compra **41 unidades por 820 G**. Somente o saldo inicial
   de **2.000 G** é fixture; nenhum peixe/ração é concedido pelo teste.
3. Coloca o peixe no lago pela física de lançamento, passa uma noite sem ração
   e confere idade inalterada. Retira ração do depósito e alimenta o lago.
4. Passa **40 manhãs alimentadas**, confere médio aos 10 dias, grande aos 20,
   reprodução, limpeza da marca e proteção contra atualização diária repetida.
   Abre o resumo para desenhar a população e a ração reais.
5. Retoma checkpoints alimentado/crescido, retira o peixe grande e o remete;
   confere pagamento de **200 G** e retomada final. Os três checkpoints também
   são abertos em processos novos do aplicativo.

Capturas locais conferem peixe, quantidade/custo, respingo e retirada. Nenhum
recurso proprietário ou captura foi adicionado ao Git.

Debug: seleção de **16** CTests, com **15** aprovados na primeira execução.
`native_fishing` ainda exigia o antigo HOLD/BLINK ao lançar um peixe no mar;
foi atualizado para verificar a ANI original de água (cinco frames, 15 ticks
nativos), mantendo o teste do pré-requisito da lula. A repetição isolada passou.
Passaram regras/percurso do lago, compra de ração, pesca/percurso, inventário,
economia, saves/sessão e regressões de solo/coleta/chuva/portas. Os quatro
avulsos iniciais também passaram; um percurso inicial com peixe grande foi
ampliado para pescar pequeno e cobrir as duas mudanças de tamanho efetivas.

Configure/build Linux Debug e Release aprovados. Release **4/4** selecionados
(lago, compra de ração, pesca e percurso/três retomadas). Build Windows MinGW
aprovado em contêiner local sem rede; Wine aprovou as duas unidades, o percurso
completo e as três retomadas no aplicativo Windows. As execuções finais
terminaram com saída **0**. Portas preservadas por comparação de `runtime.c`;
`git diff --check` e ausência de alterações em `src/sdk/disc/tools` conferidos.

**48 processos de teste** nesta rodada: quatro avulsos, 23 da seleção Debug
com a repetição isolada, nove Release e 12 Wine/targets. Inclui CTest, wrapper,
produtor, processos filhos, lançadores e a tentativa com a expectativa antiga
que falhou. Teto de **50** respeitado; a suíte inteira não foi executada.
Avisos antigos de `const` nos testes de mundo e avisos Mesa/EDID do Wine não
impediram os builds/testes. Essa execução não valida vídeo/áudio em hardware
Windows físico nem a ordem global de RNG do PS1.

## Limites

O lago e a cadeia de ração estão entregues neste recorte. O evento da vara
melhorada de Greg aos 50 peixes é a próxima entrega selecionada; concursos,
demais eventos/festivais e PF-06 continuam abertos. O bootstrap `--diagnostics`
não executa o jogo; o runtime permanece parcial. Wine não substitui validação
em Windows físico.
