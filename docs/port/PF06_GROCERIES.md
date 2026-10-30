# PF-06 — Alimentos do supermercado

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). Bolinho de arroz, pão, óleo,
farinha e curry agora podem ser comprados nos seus balcões originais, guardados
no inventário/geladeira e usados pela cozinha existente. Não há segundo estoque
nem concessão de ingredientes pelo menu de compra.

## Referência C/BIN

`shop_screen_run_handlers[4..8]` chama `shop_grant_rice_ball`,
`shop_grant_bread`, `shop_grant_oil`, `shop_grant_flour` e
`shop_grant_curry_powder`. Todos chamam `shop_item_grant` com banco **2** e
modo **SHOP_GRANT_FLAG_ONLY = 0**. O nome/comentário desse modo não descreve
por completo o comportamento: o código executável entrega o produto na mão
antes da pergunta, mantém esse registro ao pagar e o remove se houver recusa.
Não existe concessão adicional depois do pagamento nem flag de compra única.

| Produto | Item / pedido | Preço | Descrição, banco 2 | Registro / sprite do balcão | Picture |
|---|---|---|---|---|---|
| Bolinho de arroz | 0x99 / 4 | 100 G | 0x1dd | 123 / 26 | 47 |
| Pão | 0x9a / 5 | 100 G | 0x1de | 124 / 25 | 48 |
| Óleo | 0xa5 / 6 | 50 G | 0xd9 | 120 / 12 | 49 |
| Farinha | 0xa6 / 7 | 50 G | 0xda | 121 / 13 | 50 |
| Curry | 0xa7 / 8 | 50 G | 0xdb | 122 / 14 | 51 |

As fontes são `shop_key_values`, as cinco funções de concessão,
`farm_tables.c:item_info/estate_records` e `estate.c:estate_code_status`.
As cinco entradas são geradas dessas definições durante o configure; preços,
classes e figuras continuam no catálogo comum, sem tabela financeira duplicada.

Evidência binária local, sem publicar recursos:

- `A_FILE/11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
  **fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**.
- Wrappers em **0x80119338**, **0x80119364**, **0x80119390**,
  **0x801193bc**, **0x801193e8**: argumentos MIPS `a0=item`, `a1=2`,
  `a2=descrição`, `a3=0`; chamada a `shop_item_grant`, **0x8011c298**.
- Executável instalado `disc/SLUS_011.15`, payload **0x800**, base
  **0x80010000**: `item_info` em **0x8004b1a4**, registros de **40 bytes**.
  Leitura LE conferiu IDs, categoria **0**, pictures **47..51**, preços
  **100/100/50/50/50**, venda **0** e classe **0xc0000000**.
- `estate_records`, **0x8004ed04**, registros de **24 bytes**: entradas
  **120..124**, local **30**, flags **0x041f**. Óleo/farinha/curry ficam em
  X **-1423**, Y **-200**, Z **-1800/-1400/-1000**; arroz em
  **(-1323,-200,-600)** e pão em **(-1423,-200,-200)**.

A auditoria usa endereços de 32 bits e leitura little-endian. O host não
substitui a toolchain nem a verificação SHA-1 exigida pelo build PS1; não se
alega que o overlay instalado corresponde integralmente à referência PS1.

## Comportamento adaptado

O balcão compartilha a sondagem já usada pelas sementes/mochilas/cesta:
área **180**, local **30**, zona **37** a **224** unidades, chave do registro a
**400** unidades e limites estritos de **±150** em X/Z. Zona **36** continua
sendo entrega. Exige mãos vazias, Jeff presente e horário **09:00..16:59**;
fecha domingo/terça. O estoque dos cinco alimentos não é sazonal.

A apresentação usa `event_item_show`, ANI **11+direção** e as pictures do
catálogo/cache existente. A descrição espera uma tecla Cross ou Circle antes
de abrir Yes/No em **(200,128)**. Essa tecla não efetua a compra. A recusa
mostra **2:0xd8**, saldo insuficiente **2:0x4f**, e aceitar mostra **2:0xbb**.
Só depois de fechar a aceitação ocorre o débito na economia e na contabilidade
comuns. O registro exibido passa a ser o produto comprado, quantidade **1**,
modificador **0**. A adaptação revalida saldo/mão antes de concluir; não cobra
duas vezes. Recusas limpam o registro temporário sem cobrar.

No modo zero o original não chama `estate_record_publish` para ocultar ou
repor o estoque. Por isso as cinco artes permanecem nas prateleiras durante a
apresentação e permitem novas compras após esvaziar a mão, sem contador diário.
Bolsa cheia não impede comprar, pois o destino é a mão. R2 usa a capacidade
atual da mochila; cheio, mantém o produto na mão. A classe original permite
armazenar na geladeira, recusando armário e caixa de ferramentas.

`feed_shop` reutiliza fases, mensagens e confirmação; `exploration` abre o
fluxo no mesmo ponto de interação. `feed_shop_ui` reaproveita o painel de ouro
e a mãozinha já corrigida. `scene_preview` resolve as cinco artes do pack do
supermercado, sem alterar o estado no desenho. `inventory`, `economy` e
`cooking` continuam sendo os proprietários e consumidores dos mesmos registros.
`runtime.c` e as animações de portas do usuário ficaram integralmente intactos.

O codec existente já representa mão, mochila, geladeira, dinheiro, contabilidade
e receitas. Não foi necessária mudança de formato ou trabalho adicional sobre
compatibilidade de saves.

## Validação

Configuração/builds em `build/native-debug`, `build/native-release` e
`build/native-windows-x64`, sem alterar o build PS1. Regressões selecionadas
respeitam o teto de 50 processos por rodada, incluindo wrappers e filhos.

- `groceries_test`: os cinco IDs/pedidos/preços/descrições/artes; sete dias e
  24 horas; presença/local de Jeff; área, zona 36/37, mãos ocupadas e borda
  estrita da sonda; recusa por No, falta de saldo, aceitação tardia, repetição,
  bolsa cheia, cobrança/contabilidade únicas, depósitos e ingredientes comprados.
- `groceries_playthrough`: recursos instalados, cada balcão real, ANI e
  descrição, recusa/saldo, seis compras (pão repetido), R2 para guardar/retirar,
  geladeira pelo móvel/menu e empilhamento. Comprou por **450 G** e preservou
  o saldo final **550 G**. Casa inicial ampliada, galinheiro e panela são
  preparação explícita do teste; ingredientes não são concedidos.
- Cozinha pelo móvel/menu: seleciona panela, arroz e curry comprados na
  geladeira; executa fechamento/fades/XA original **22**, gera o prato **178**
  e registra a receita. Consome apenas arroz/curry, preservando pão **2**,
  óleo **1**, farinha **1** e dinheiro. O teste de regras também recusa panela
  não adquirida sem mutar ingredientes.
- Checkpoints com curry na mão, estoque na geladeira e prato/receita prontos:
  encode/reload e comparação do estado, mais retomada em processos novos do app.
- Capturas locais conferidas: confirmação, produto carregado, casa e seleção/
  resultado da cozinha. Recursos e capturas não entram no Git.

Resultados: configure/build Debug e Release aprovados; Debug **10/10**
selecionados (regras novas, percurso com três processos novos do app e oito
regressões), Release **2/2** (regras/percurso e três retomadas). Build Windows
MinGW aprovado no contêiner local sem rede; Wine aprovou unidade, percurso e
três retomadas do executável Windows, todos com saída **0**. Os avisos locais
de Mesa/EDID do Wine não impediram carregar/apresentar os três checkpoints.

**37 processos de teste**, contando CTest, wrappers, filhos, Wine/targets e as
duas tentativas iniciais do percurso corrigidas: a preparação da geladeira
precisava fechar a ajuda antes de navegar e a casa da fixture exigia seu
pré-requisito do galinheiro. Nenhuma exigiu correção no save ou no gameplay.
Suíte inteira não executada. Os builds Release/Windows mantêm avisos já
existentes de `const` nos testes `world_fidelity_playthrough`/`world_map_playthrough`;
os arquivos deste recorte não introduziram avisos.

## Limites

Este recorte entrega compra, transporte, armazenamento e uso pela cozinha já
implementada. Comer diretamente os alimentos, embrulhar presentes, farmácia,
comidas das outras lojas e festivais continuam separados. O percurso culinário
cobre curry; não declara execução de todas as receitas com pão/óleo/farinha.
A aquisição da casa/panela tem percursos próprios nos recortes anteriores; aqui
eles são preparação para isolar o uso dos alimentos efetivamente comprados.

Windows/Wine não equivale a validar uma máquina Windows física ou seus drivers.
O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.
