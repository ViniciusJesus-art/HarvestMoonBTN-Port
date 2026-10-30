# PF-06 — Acessórios de Saibara

Recorte de 2026-10-29 do [ROADMAP.md](ROADMAP.md). Adapta a encomenda original
à loja, fila, economia e inventário existentes. `src/`, SDK e recursos locais
não foram modificados. As animações de portas do usuário foram preservadas.

## Auditoria C/BIN

`menu/shop_dialogues.c:shop_tool_upgrade_menu_setup` oferece **Girl's present**
entre Improve Tool e as máquinas. O valor da linha é sorteado uma vez a cada
montagem do menu entre `girls_present_messages`: pulseira **0xf7**, colar
**0xf8** e brincos **0xf9**. Não é uma lista para escolher a joia. Comprar uma
ferramenta remonta o menu; descrição e cancelamentos conservam o sorteio.
`generate_farm_tables.py` extrai os três IDs do C; a legenda vem do text bank.

`shop_item_menu_step` escolhe **Orichalcum 0x61** para essa linha. Verifica um
registro na mão/mochila acessível e o saldo; depósitos não servem como material.
A conversa de compra exige mãos vazias. Ambos os requisitos são novamente
checados na confirmação nativa, antes de qualquer efeito.

| Regra | Original e adaptação |
|---|---|
| Custo | 1.000 G e um Orichalcum, qualquer uma das três joias |
| Descrição | Banco 2, mensagem 0x88 |
| Primeira confirmação | 2:0x1e1, respostas em (200,128) |
| Segunda confirmação | 2:0x9e, respostas em (224,128) |
| Falta de minério/dinheiro | 2:0xa3 |
| Cancelamento | 2:0x93, sem consumo/cobrança ou novo sorteio |
| Aceite | Apresentação do minério pela ANI existente, 2:0x9f e saída comum para área 32 |
| Prazo | Terceira manhã: timer original 2, cuja flag só é limpa na chamada seguinte à que chega a zero |
| Retirada | Conversa original 354, mensagem 2:0xa6, comando 31/7/1/0/0 e aplicação 81/20/2/0/0 |
| Mãos ocupadas na retirada | Pedido permanece pronto; não recebe nem perde item |
| Registro recebido | ID sorteado, modifier=0, count=1, categoria 0; na mão, por `event_cue_issue`, não no equipamento |
| Remessa | 2.000 G por item, pelo fluxo comum da caixa |

`event/milestones.c:milestone_set_timer/tick` e `estate.c` mantêm uma encomenda
comum, como nas ferramentas/máquinas. A adaptação amplia os IDs admitidos pela
fila `HmbtnMakers`; não cria fila ou propriedade permanente de joias.
O mesmo acessório pode ser encomendado novamente depois da retirada.

Evidência binária local:

- `A_FILE/11_GF_FARM.bin`, SHA-1
  `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`, base **0x800b7e20**.
- `girls_present_messages` em **0x801282dc** contém os três halfwords LE
  **247,248,249**; setup em **0x8011b2b0**.
- `shop_item_menu_step`, **0x8011bd6c**; em **0x8011be48..0x8011be64** aparecem
  material **0x61** e mensagens **0x1e1/0xa3/0x9e**, distintos do ramo de máquinas.
- `milestone_set_timer`, **0x80111e68**, e `milestone_tick`, **0x80111f88**,
  compartilham o protocolo de prazo já auditado nas ferramentas.
- Script **354** no `12_event_s.bin`: somente o bloco **0x4000** é interpretado
  como bytecode. As instruções **16..24** separam as três joias e emitem o cue;
  **30** trata falta de espaço na mão, **32** limpa o pedido.
- Catálogo do executável instalado `disc/SLUS_011.15`: registros de 40 bytes em
  **0x8004d83c/0x8004d864/0x8004d88c**, figuras **142/143/144**, compra **1000**,
  remessa **2000**, flags **0x8000**, classe **0xa1000000** e categoria **0**.

O gerador `rand_range` original é reutilizado tomando emprestado o RNG já
persistido da vila. Abrir/remontar esse menu passa a consumir esse mesmo estado;
não há seed independente ou sorteio no desenho. Isso não declara reprodução da
sequência global PS1: outros consumidores nativos ainda possuem RNGs próprios.

## Integração e estado

A loja usa as duas confirmações, apresentação de material e saída já usadas
pelas máquinas. O minério consumido vem do inventário central; o débito vai à
mesma economia/contabilidade. Fila ocupada bloqueia novos pedidos e deriva a
flag de trabalho de Saibara. Cada manhã só conta uma vez pelo observador diário.

O comando de retirada 31 solicita a apresentação existente de item na mão.
O fim da conversa conserva essa ANI enquanto `event_item` estiver ativo;
a pose de repouso não a sobrescreve. O término da ANI materializa o registro
na mão, como nos demais consumidores desse controlador. O script aplica o
pedido uma vez; o estado transitório de apresentação impede save nesse intervalo.
Nenhum item é adicionado à coleção de ferramentas.

O codec continua **v16**. A encomenda utiliza os campos já existentes, e a joia
usa os registros normais de item carregado/mochila. Apenas a validação da fila
passa a aceitar os três IDs com até três manhãs. Não houve trabalho adicional
de compatibilidade entre versões de save.

## Validação

- Linux Debug: **7/7** regras/regressões (acessórios, máquinas, ferramentas,
  ferramentas animais, loja, conversa e save); wrapper de acessórios **1/1**,
  com três retomadas em aplicações novas; regressões **4/4** de ferramentas
  animais, máquinas, portas/chuva e fidelidade do mundo.
- Linux Release: build e **2/2**, unidade e wrapper do percurso com três retomadas.
- Windows x64/MinGW: build local sem rede; no Wine, unidade, percurso e três
  retomadas independentes (pedido, item guardado e remessa concluída), todos exit 0.
  Avisos Mesa/EDID/wasapi não impediram os checks. Windows físico não foi validado.
- Capturas da seleção, confirmações, retirada e três joias na mão, produzidas
  por um percurso Debug adicional; menu e item recebido inspecionados.
- Bloco `HMBTN_EXP_TRANSITION` até `HMBTN_EXP_SLEEP` de `runtime.c` comparado
  byte a byte com o estado inicial deste recorte: **idêntico**.

**46 processos** de teste, incluindo tentativas, wrappers, filhos e lançadores:
8 na seleção inicial; 3 tentativas de percurso; 6 no wrapper Debug; 11 nas
regressões; 7 no Release; 1 no percurso de capturas; 10 no Wine (lançador + alvo).
Não foi executada a suíte inteira; o limite de 50 foi respeitado.
As tentativas localizaram a sobrescrita da ANI, corrigida no runtime, e dois
ajustes no driver de teste: não apertar Cross durante o recebimento e considerar
pagamentos de remessas anteriores durante as noites.

`accessories_test.c` verifica os três sorteios/metadados, falta de material e
saldo, exclusão dos depósitos, mão ocupada, dois cancelamentos, consumo/débito
únicos, fila compartilhada, três manhãs idempotentes, validação e retomada.
`accessories_playthrough.c` escava os três Orichalcum e passa pela conversa 354,
loja e ANI reais; dorme nove noites no fluxo comum, recusa retirada com a mão
ocupada, recebe as três joias, guarda/retoma e vende pela caixa real.
Saldo inicial, energia, mochila e seeds são fixtures; minérios e joias não são
concedidos. Loads diretos encurtam deslocamentos sem relação com o serviço.

Reprodução, selecionando apenas os testes pertinentes ao orçamento da rodada:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_accessories(_playthrough)?$'
```

A seleção acima usa **7 processos**. Logs locais em
`/tmp/hmbtn-accessories-{unit,process,regression,release-tests,wine-unit,wine-playthrough}.log`;
retomadas Windows em `wine-{pending,collected,complete}.log` com o mesmo prefixo.
Capturas em `/tmp/hmbtn-accessories-shots/`, fora do Git.

Relacionamentos completos, embrulhos, festivais e casamento não são declarados
concluídos por esta entrega. O bootstrap `--diagnostics` não executa o jogo;
o runtime permanece parcial. Próximo recorte: aquisição original das duas
ampliações de mochila no supermercado, usando capacidade e navegação existentes.
O build host não substitui a toolchain/verificação SHA-1 do build PS1.
