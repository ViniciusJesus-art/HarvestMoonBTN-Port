# PF-06 — Consumo direto de alimentos e bebidas

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). Square agora permite consumir
alimentos/bebidas carregados que usam o callback de atributos já adaptado.
Arroz/pão comprados, prato preparado, coleta e suco produzido têm percurso
real, sem outro inventário ou controlador de energia.

## Origem C/BIN

`player_try_carry_throw` testa **Square**, a flag de carregar item e a mão
ocupada antes de chamar `player_use_held_item(1)`. `stored_item_kind` dá
prioridade à flag **0x400** (bebida), depois **0x1800** (comida). Óleo, farinha,
curry em pó e embrulhos não recebem flags de alimento por causa do conteúdo.

- Comida: modo `PLAYER_MODE_SHOW_ITEM`, ANI base **5**, som **0x4a / 74**.
  `player_run_action` oculta a figura ao chegar ao frame **2**. No fim da ANI,
  `player_entry_refresh` chama `item_run_use` e libera o registro da mão.
- Bebida: modo **0x49**, som **0x4b / 75**, banco de ação **0x49-0x3b = 14**,
  range **0**, carregado por `player_load_tool_anim`. A flag **BIT14** impede
  desenhar uma segunda figura carregada: a própria ANI contém o recipiente.
  No fim, usa o item, libera a mão e restaura o sprite base.
- `item_run_use` passa o primeiro argumento do catálogo, exceto com flag
  **0x800**: nesse caso usa o modificador do registro convertido para **s8**.
  Assim a qualidade de um prato/suco é a recuperação de stamina, com o mesmo
  comportamento de byte assinado do original; não é sempre o valor da tabela.
- `farm_clamp_status` limita stamina a **0..maxStamina**, fadiga a **0..100**
  e felicidade a **0..65535**. Se a fadiga foi alterada e o horário atual é
  exatamente **06:00**, um resultado 100 passa a **99**. Consumo normal não
  ativa a trava especial de fadiga usada por comandos de evento.

| Conteúdo | ID | Argumentos do catálogo | Efeito do percurso |
|---|---|---|---|
| Bolinho de arroz | 153 | stamina +2, fadiga -1 | +2 / -1 |
| Pão | 154 | stamina +2, fadiga -1 | +2 / -1 |
| Uva silvestre | 89 | stamina +10, fadiga -2 | +10 / -2 |
| Curry preparado | 178 | flag 0x800, fadiga -3 | qualidade 30: +30 / -3 |
| Suco de fruta | 180 | bebida + flag 0x800, fadiga -4 | qualidade 20: +20 / -4 |
| Cogumelo venenoso | 91 | stamina -10, fadiga +5 | Regras negativas e limites, teste de domínio |
| Vinho | 159 | fadiga -5, felicidade +1 | Callback/limites testados, aquisição ainda separada |

Um detalhe mantido do código extraído: `cooking_find_record` produz fiasco
**226** com modificador **1** quando não há receita correspondente. Como a
flag 0x800 usa o modificador, esse registro aplica **+1** de stamina e **+2**
de felicidade, apesar do primeiro argumento **-10** na tabela. Um registro
com modificador **246** aplica **-10**. Os testes também cobrem **128 → -128**,
**255 → -1** e **256 → 0**; não se presume recuperação sempre positiva.

Auditoria local dos recursos do usuário:

- `A_FILE/11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
  **fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**.
- `player_use_held_item`, **0x800cb850**: chama `stored_item_kind` em
  **0x800cbafc**; seleciona a ANI da comida e som 74, ou modo **0x49** e
  som 75 em **0x800cbb4c..0x800cbb80**. `stored_item_kind`, **0x800d736c**,
  contém as máscaras **0x400/0x1800** em **0x800d7390/0x800d7398**.
- `farm_clamp_status`, **0x800d622c**. O catálogo do executável instalado
  `disc/SLUS_011.15` começa em **0x8004b1a4**, passo **40 bytes**; flags,
  callback e argumentos dos IDs acima foram lidos como LE. O endereço do
  callback serve à auditoria; o host recebe só uma classificação booleana.
- `player_anim_banks[14] = 360`; o carregador nativo já lê a linha original
  de sete setores e agora a reutiliza para beber.
- As uvas dos registros **154/155**, local **13**, só estão disponíveis no
  outono. Cabeçalhos de seção do BIN conferem área lógica **68**/local **13**;
  áreas **64/72** correspondem aos locais **14/12**, respectivamente.

Não houve edição de `src/`, recursos proprietários ou SDK. O host não substitui
a toolchain nem a verificação SHA-1 do build PS1, nem declara que o overlay
instalado seja idêntico à referência PS1.

## Adaptação e estado

O gerador do catálogo classifica o callback original `farm_clamp_status` em
`HmbtnFarmItem.status_use`. Nenhum endereço serializado vira ponteiro host.
Os argumentos já existentes continuam sendo a fonte dos efeitos; o terceiro
argumento, também usado como ANI de plantio por outros callbacks, fornece a
felicidade dos alimentos. Os pratos mantêm sua qualidade no registro comum.

`event_item_use` acrescenta estados de consumo direto ao controlador existente:
comida usa ANI base; bebida pede o banco 14 ao carregador de ações. A mão fica
intacta até o fim. Input de movimento/menu/estoque não interrompe a ação,
salvar espera sua conclusão e os efeitos/remoção são aplicados uma única vez.
Um registro da mão alterado externamente não autoriza consumir seu substituto.
Como no original, remove-se o registro inteiro, não apenas uma unidade de um
`count` artificialmente maior que um; as retiradas normais da geladeira separam
uma unidade para a mão.

O consumo comum permite relógio e mundo continuarem quando o local permite;
interiores conservam sua regra de pausa. Apresentações/consumos comandados por
eventos continuam pausando como antes. `event_item_begin` mantém a restauração
do item temporariamente posto de lado, protegendo bolo/Power Berry existentes.
Berries de aquisição automática continuam no percurso próprio.

Stamina/fadiga permanecem em `farm_action`; felicidade foi adicionada junto
aos mesmos atributos, sem outra cópia de estado. O codec **v19** acrescenta
somente esse atributo necessário, validando seu limite e retomando os valores
atuais. O relógio é um empréstimo vivo durante a ação e não é serializado.
Nenhum trabalho separado de compatibilidade foi aberto.

Em `runtime.c` foram acrescentadas apenas duas linhas no bloco de itens para
restaurar o banco base após beber. Os blocos de portas/animações personalizados
não foram alterados. Desenhar não consome itens nem modifica atributos.

## Validação

`food_use_test` cobre quatro direções, elegibilidade/recusas, comida/bebida,
ANI/som, deferimento e aplicação única, conversão assinada da qualidade,
limites de stamina/fadiga/felicidade, exceção das 06:00, registro inteiro,
substituição da mão, restauração de evento e encode/decode dos atributos.

`food_use_playthrough` recebe o checkpoint produzido pelo percurso de compras
dos cinco alimentos e preparo de curry. Com valores iniciais de teste **50**
de stamina e **20** de fadiga:

1. Come o curry comprado/preparado e retira uma unidade de pão pela geladeira;
   come o pão e compra/come mais um arroz no balcão original.
2. Avança **60 dias pelo calendário compartilhado**, incluindo os callbacks
   diários, até o outono; coleta duas uvas por Cross nas posições originais
   da montanha. Come uma, com relógio correndo durante a ANI.
3. Guarda a segunda por R2, seleciona-a no móvel da cozinha e prepara suco
   com fade/XA/receita originais; bebe pela ANI real do banco 14, que retorna
   ao sprite base ao acabar.
4. Verifica stamina **100**, fadiga **9**, saldo **450 G**, pão restante na
   geladeira e demais estoques preservados. Grava/reabre checkpoints depois
   de comer, com o suco na mão e depois de beber, comparando os dados.

Casa/galinheiro/panela vêm da preparação explícita do produtor de compras;
o mixer e os atributos iniciais são fixtures. Nenhum alimento é concedido:
compras, pratos e coletas são efetivos. Capturas locais conferem comida,
ocultação e bebida; não entram no Git.

Configure/build Debug e Release aprovados; build Windows MinGW aprovado em
contêiner local sem rede. Debug **13/13** selecionados (incluindo o produtor de
compras, consumo e três retomadas em processos novos), mais **2/2** regressões
de chuva/portas/fidelidade. Release **2/2**, também com produtor e três retomadas.
Windows/Wine aprovou unidade, produtor de compras, percurso de consumo e três
processos novos do app: depois de comer, com suco na mão e depois de beber.
Todas as execuções finais terminaram com saída **0**.

**48 processos de teste**: seis avulsos iniciais, 19 da seleção Debug, três das
regressões de mundo, oito da seleção Release e 12 Wine/targets. Inclui três
tentativas iniciais que falharam por IDs/região/estação incorretos na preparação
dos testes, corrigidos com o enum e os cabeçalhos de seção originais. O gameplay
não exigiu correção para essas tentativas. Conta CTest, wrappers, produtores,
filhos e lançadores; teto de **50** respeitado, sem suíte inteira.
Avisos locais de Mesa/EDID do Wine não impediram os checkpoints. Os builds
completos mantêm avisos de `const` já existentes em testes de mundo; esses
arquivos não foram alterados pelo recorte.

## Limites

O recorte cobre consumo direto dos itens carregados de categoria **0**, com
flags originais de comida/bebida e callback de atributos representado. Não
habilita novos vendedores nem simula aquisição de remédios/vinhos/refeições
exclusivas de outros locais. Aquisição automática de berries e restaurações
scriptadas continuam separadas; eventos ainda não portados não ganham execução
por causa desta adaptação. A farmácia é o próximo recorte do roadmap.

O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.
Windows/Wine não equivale à validação numa máquina Windows física.
