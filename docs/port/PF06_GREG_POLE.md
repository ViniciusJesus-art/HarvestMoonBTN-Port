# PF-06 — Vara melhorada de Greg

Entrega de 2026-11-04: evento **177 / 0xb1**, com gatilho, roteiro,
atores/ANI, entrega e uso da vara **57 / 0x39**. Reaproveita o lago, o
interpretador de visitas, a vila, o inventário e a pesca existentes.

## Referência C/BIN

`farm_init.c: farm_event_available`, condição **61 / EVENT_COND_FISH_50**,
consulta `game_state.animals.fishCount >= 50`. `field/pond.c:
pond_daily_update` atualiza esse valor ao terminar a manhã, mesmo sem ração.
Assim, depositar o 50º peixe durante o dia não antecipa a visita; retirar um
peixe também não muda imediatamente o requisito daquela manhã. O contador
vivo do lago continua derivado das vagas.

No `00_gf.bin`, a tabela da fazenda começa em **0xdd800**, com registros de
36 bytes. A linha **57**, também presente nas variantes sazonais, contém:

| Campo | Valor e efeito |
|---|---|
| Evento/local anterior | `177 | (2 << 10)`: entrada na fazenda vindo da casa. |
| Janela | `0x3ffffcc6`: **06:00 a 11:59**, sem restringir ano/data. |
| Flags | `0x7ffe3800`: estações/dias/climas admitidos e vaga para ferramenta. |
| Condição/modo | **61 / 2**: 50 peixes na contagem matinal e gatilho de entrada. |
| Conclusão do gatilho | Flag **251 + 177 = 428**, marcada ao iniciar. |

Mochila de ferramentas cheia adia a visita sem consumir a flag. Mãos ocupadas
não bloqueiam o encontro. A tabela não exige a vara inicial nem a flag de
posse dessa vara; tampouco usa a agenda comum de Greg como requisito.

O bloco do evento em `12_event_s.bin`, **0x41d800..0x423800**, possui quatro
entradas: prop **0x0839**, Greg **0x2020**, banco **0x8000** e script
**0x4000**, com 31 instruções. O porte executa esse bytecode instalado:

- Posiciona Greg, aproxima a câmera e mostra **1:506 / 0x1fa**.
- Libera o jogador e aguarda conversa, saída da área ou **três horas**.
- A conversa soma **20** de amizade e mostra **1:507 / 0x1fb**.
- Publica a vara perto de Greg durante **60 ticks PS1 / 30 nativos**.
- Com mãos vazias, armazena e apresenta a ferramenta com ANI **71 / 0x47**,
  som **7** e duração de **11 ticks nativos**, conforme `event_item_store`
  e `player_show_tool`. Mãos ocupadas recebem silenciosamente, preservando
  o registro carregado. A ferramenta usa equipamento/mochila, sem toolbox.
- Dispatch **9** marca a flag **0x66 / 102**. Greg sai pelo percurso original.
  A caminhada continua durante a apresentação da ferramenta.

Sair ou deixar o prazo vencer consome o encontro único, sem conceder vara ou
flag 102. Não foi inventada uma nova tentativa condicionada apenas à posse.

Auditoria do `11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Referência |
|---|---|
| **0x80104ec8** | `farm_event_available`, incluindo o limiar de 50. |
| **0x8010c3f8** | `pond_daily_update`, contagem matinal. |
| **0x800e6eac / 0x800e7088** | `event_cmd_has_room` e `event_item_store`. |
| **0x800e8a24** | `event_cmd_publish_offset`, prop orientado ao jogador. |
| **0x800e9d28**, imediato em **0x800ea48c** | Dispatch e flag 102. |
| **0x800e7798** | Modos de término/saída do evento. |

Endereços são referências de auditoria, nunca ponteiros host.

## Integração e estado

`HmbtnPond.morning_count` representa exclusivamente o snapshot original usado
pelo evento; não substitui `hmbtn_pond_count`. O codec vigente **v21** guarda
esse snapshot, pois recalculá-lo na carga anteciparia a elegibilidade.
Não foi aberto trabalho de migração de saves.

Os gatilhos recebem um empréstimo do lago da sessão. Greg usa o ator de visita
32, separado da agenda, e libera esse vínculo ao concluir/sair. A figura da
vara vem do bloco original do evento, sem conceder item pela renderização.
`event_item` apenas apresenta a ferramenta já armazenada, bloqueando controles
durante a ação; o desenho respeita o offset fixo de direção zero do original.
O armazenamento continua na operação comum do inventário.

As animações de portas do usuário foram preservadas. `src/`, `sdk/`, `disc/`,
`tools/` e o fluxo PS1 não foram alterados por esta entrega. Nenhum recurso
proprietário ou captura de validação foi acrescentado ao Git.

## Validação

`greg_pole_test` cobre 49/50, depósito/retirada após a atualização matinal,
persistência do snapshot distinto da população viva, entrada/local anterior,
fronteiras de horário, estações/dias/climas, espaço, mãos ocupadas, flag única
e apresentação sem nova mutação do inventário.

`greg_pole_playthrough` começa no checkpoint produzido por `pond_playthrough`:
vara inicial obtida com Greg, peixe pequeno pescado e reprodução real até
três peixes. Só o dinheiro para comprar ração e a organização das ferramentas
iniciais são fixtures; nenhum peixe, ração ou vara melhorada é concedido.

1. Compra **301** rações pelo balcão e alimenta/dorme pelos controles reais.
2. Atravessa o inverno sem alimentar o lago congelado. Após mais **87 manhãs
   alimentadas**, alcança **51 peixes**, mantendo a mochila de ferramentas cheia.
3. Confere adiamento por espaço; libera uma vaga dentro da casa e grava.
4. A partir desse checkpoint, verifica prazo de três horas, saída da área,
   entrega silenciosa com ração comprada na mão e entrega com apresentação.
5. Confere amizade/flags, liberação do ator, ausência de duplicação, equipa
   a vara recebida e pesca de fato na praia. Nenhuma agenda de NPC é alterada
   para localizar a margem: a busca usa a geometria de água instalada.
6. O wrapper reabre os checkpoints anterior, entrega silenciosa e final em
   três processos novos do aplicativo, além das comparações de save no percurso.

Configure/build Linux Debug e Release aprovados. Debug **12/12** selecionados
(gatilho/percurso de Greg, lago, ração, visitas, apresentação, pesca/percurso,
save/sessão e regressões de chuva/portas/coleta/descarte); Release **4/4**
(Greg, lago, apresentação e percurso). Os dois wrappers incluem produtor,
entrega e três retomadas em processos novos.

Build Windows MinGW em contêiner local sem rede aprovado. Wine aprovou a
unidade, o produtor do lago, o percurso completo e as três retomadas no
aplicativo Windows, todos com saída **0**. A execução nativa adicional com
vídeo/áudio dummy também passou. Capturas locais de oferta/recebimento foram
inspecionadas, sem acrescentá-las ao Git.

**48 processos de teste**: oito avulsos, 18 na seleção Debug, dez Release e
12 contando Wine/targets. Inclui quatro tentativas iniciais do percurso que
falharam e foram corrigidas: alimentação no inverno congelado, avaliação do
gatilho após a entrada, procura de pedra num campo vazio do fixture e comando
de arremesso enviado pelo driver ao liberar o jogador. O percurso final usa
ração realmente comprada para a mão ocupada e envia confirmação somente nas
mensagens. Não houve execução da suíte inteira; teto de **50** respeitado.

Comparação com o `runtime.c` da retomada confirmou ausência de alterações no
bloco das portas. `git diff --check` passou e os arquivos PS1 permaneceram
intactos. Avisos antigos de `const` nos testes de mundo e avisos Mesa/EDID do
Wine não impediram as verificações. O Wine não valida vídeo/áudio em hardware
Windows físico; esta entrega não declara paridade da ordem global de RNG PS1.

## Limites

O evento da vara está integrado ao runtime parcial. Concursos e outros
eventos/festivais continuam abertos. Próximo recorte: **colmeia e mel**,
auditando aquisição, coleta diária e progressão com Louis antes de habilitar
o conteúdo. O bootstrap `--diagnostics` não executa o jogo. Builds host não
substituem a toolchain/verificação SHA-1 PS1; Wine não valida hardware Windows.
