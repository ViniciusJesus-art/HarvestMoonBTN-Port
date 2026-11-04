# PF-06 — Maçãs da árvore da fazenda

Entrega de 2026-11-04. Integra os três registros originais de maçãs aos
sistemas existentes de coleta, inventário, cozinha, presentes e remessas.
O runtime continua parcial; o bootstrap `--diagnostics` não executa o jogo.

## Referência C e binários instalados

`src/main/game/farm_tables.c: estate_records`, run 0, contém:

| Registro | Item / local | Flags | Posição |
|---|---|---|---|
| 1 | Apple **87 / 0x57**, local **1** | `0x04a8` | `(-7285, 0, -743)` |
| 2 | Apple **87 / 0x57**, local **1** | `0x04a8` | `(-6875, 0, -1153)` |
| 3 | Apple **87 / 0x57**, local **1** | `0x04a8` | `(-6301, 0, -1030)` |

O bit sazonal **8** habilita somente o outono; **0x80** permite coleta e
**0x20** habilita a reposição em `estate_record_table_enable`. A rotina diária
`estate_record_tables_refresh`, chamada pelo pernoite em `clock_events.c`,
renova os registros. `estate_field_pickup` exige estação/enable/coleta e
limpa o enable daquele registro. Não é necessário plantar/comprar a árvore,
ter colmeia, pagar ou atingir amizade; não existe sorteio de produção.

No `SLUS_011.15` instalado, SHA-1
**531587505a2659b21560b264fd04067fb928841c**, os registros foram conferidos
como estruturas little-endian de **24 bytes**, com coordenadas de **32 bits**:

| Offset no arquivo | Endereço original |
|---|---|
| **0x3f51c** | **0x8004ed1c** |
| **0x3f534** | **0x8004ed34** |
| **0x3f54c** | **0x8004ed4c** |

O cabeçalho do executável informa carga em `0x80010000`, depois dos 0x800
bytes de header. Endereços aqui são referências de auditoria, nunca ponteiros
host. O gerador existente continua extraindo os mesmos registros do C.

No `11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**, conferidos:

- `estate_record_tables_refresh`, **0x800f40a4**, chama o habilitador em
  **0x800f53bc**. Instruções **0x800f53d4/0x800f53e0** testam os bits
  **0x20/0x40**; **0x800f53dc** acrescenta o enable **1**.
- `estate_field_pickup`, **0x800f413c**: **0x800f4158** combina a máscara
  sazonal com **0x81**, e **0x800f417c** limpa o enable com **0xfffe**.
- `farm_zone_at_probe`, **0x800bde84**, e `player_start_request` mantêm a
  precedência da zona da colmeia. O segundo registro fica parcialmente nessa
  zona; a coleta pelo lado oeste fica dentro da caixa da maçã e fora da zona
  de interação da árvore. O percurso usa essa aproximação, sem mudar a zona.

`estate_code_lookup(ITEM_APPLE)` resolve o sprite **18**, célula zero do
pack de outono. `estate_search_records` e o projetor nativo existente usam
os mesmos offsets `(150, -100, -150)`. A posição não foi duplicada no host.
O ItemInfo original fornece preço **50 G**, consumo **+2 stamina/-1 fadiga**
e aceitação na geladeira. As receitas existentes aceitam maçã, incluindo
suco de frutas com mixer, qualidade base **20**.

## Integração

`farm_gather` passa a aceitar os registros 1..3 e continua atendendo a coleta
externa anterior. Cada maçã usa seu bit em `gather.taken`, já incluído no save
vigente **v22**. Nenhum campo, versão, formato ou caminho de save foi alterado.
A manhã libera as três pelo bit diário original; a estação decide se estão
disponíveis. Reabrir mapa ou save não reabastece a árvore.

`exploration` mantém a prioridade dos ovos e tenta a coleta comum se nenhum
ovo foi tomado. A mão e a apresentação continuam na mesma operação: uma
unidade, modificador zero, ANI `11+direção`, som 7, distância/altura/direção
e mãos vazias. A mochila cheia não impede segurar a fruta. Depósitos,
consumo, cozinha, presentes e remessas continuam usando os donos atuais.

`scene_preview` expõe a figura original das maçãs disponíveis, também durante
visitas/eventos da fazenda. O desenho consulta o estado; não gera frutas,
não limpa bits de coleta e não atualiza a simulação.

## Validação

`farm_gather_test` conserva os casos de forragem e acrescenta três maçãs,
quatro estações, local incorreto/estufa, mãos ocupadas, altura, mochila cheia,
coleta individual sem repetição, sprite/preço, codec e reset diário
idempotente. Confere fim do outono, ausência no inverno e retorno no outono
seguinte pelo relógio compartilhado.

`apples_playthrough` começa no penúltimo dia do outono, sem conceder fruta:

1. Colhe as três posições originais. Come a primeira, guarda a segunda na
   mochila e remete a terceira por **50 G**; tenta colher novamente.
2. Retoma o checkpoint e confirma que as três continuam indisponíveis.
3. Usa a maçã da mochila no preparo real de suco, com seleção de ingrediente,
   mixer, fade/XA/resultado e qualidade 20; retoma e bebe o suco.
4. Dorme pela escolha original da cama: recebe os 50 G e as três renovam.
5. Dá uma maçã a Lillia pelo diálogo/agenda reais, guarda outra na geladeira
   pela interface e remete a terceira por mais 50 G.
6. Dorme até o inverno: recebe o segundo pagamento, a árvore fica sem coleta
   e a maçã guardada continua na geladeira. Retoma o estado final.

Data inicial, posições, primeira casa/mixer e previsão ensolarada são fixtures.
A segunda maçã é abordada pela borda oeste, preservando a zona da colmeia.
Não são concedidas maçãs, suco, amizade ou dinheiro pelas ações do driver.
Os checkpoints de coleta, suco, estoque e inverno são comparados ao reabrir
e carregados pelo aplicativo em quatro processos novos por plataforma.

Configure/build Linux Debug e Release aprovados. Debug **14/14** selecionados:
coleta/percurso, save/sessão, consumo/cozinha, ovos/transporte, colmeia,
interação da casa, carregar/remeter e regressões de portas/chuva/coleta/descarte.
Release **4/4**: coleta/percurso, codec e colmeia. O percurso completo de
colmeia/mel foi executado novamente, sem alterar eventos ou portas.

Build Windows MinGW sem rede aprovado. Unidade da coleta, percurso e quatro
retomadas em processos Windows no Wine aprovados. Capturas locais de frutas,
suco e inverno foram produzidas; a figura das maçãs foi inspecionada.
Nenhum recurso proprietário ou captura foi acrescentado ao repositório.

**46 processos de teste**: **24 Debug**, **10 Release**, **12 Wine/targets**,
incluindo CTest, wrappers e filhos. Inclui uma tentativa do percurso que
atingia a zona da colmeia ao mirar o centro do segundo registro; o driver
passou a abordar a borda oeste, conforme a geometria/interação originais.
Não houve execução da suíte inteira; teto de 50 respeitado.

`runtime.c`, `daily.c` e `save.c` permaneceram byte a byte iguais à retomada
desta entrega. `git diff --check` passou. Portas e demais alterações do usuário
foram preservadas; `src/`, `sdk/`, `disc/`, `tools/` e o fluxo PS1 ficaram
intactos. Build host não substitui a toolchain/verificação SHA-1 do original;
Wine não substitui teste em hardware Windows físico.

## Limites e próximo recorte

Esta entrega conclui a coleta original das maçãs da fazenda. Não conclui todas
as receitas, espécies, festivais, eventos, família nem PF-06. Próximo recorte:
ovos termais, auditando o arremesso de ovos realmente produzidos na fonte,
a conversão/apresentação original, consumo/remessa e retomada.
