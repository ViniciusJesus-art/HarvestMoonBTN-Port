# PF-06 — Colmeia e mel

Entrega de 2026-11-04. Reaproveita campos/cultivos, gatilhos, interpretador,
ANI/bancos originais, inventário, conversa/presentes, consumo, remessas e sono.
O runtime continua parcial; o bootstrap `--diagnostics` não executa o jogo.

## Referência C e BIN instalado

`farm_init.c: farm_event_available`, condição **76**, procura uma célula
madura de Moondrop (`0x31`), Pink Cat (`0x32`), Magic Red azul (`0x178`),
Magic Red vermelha (`0x179`) ou Toy Flower (`0x34`).
`field_find_free_tile(mode=1, flags=2)` compara valor e estágio; ignora o
solo e procura somente no campo externo. Não exige 90 flores, estação atual
igual à cultura, nem amizade com Louis. A estufa não atende esse requisito.
A condição 37, que conta 90 flores, pertence a outro evento.

A tabela da fazenda em `00_gf.bin`, **0xdd800**, tem registros de 36 bytes:

| Linha / evento | Condições originais |
|---|---|
| **63 / 277** (`0x115`) | Casa→fazenda, 06:00..11:59, condição 76, qualquer estação/dia/clima, flag de evento 528 ainda livre. |
| **56 / 171** (`0xab`) | Casa→fazenda, 06:00..11:59, contador diário `count0[19]` entre 1 e 127, qualquer estação/dia/clima, flag 422 ainda livre. |

As duas linhas usam janela `0x3ffffcc6` e flags `0x7ffc3800`;
não exigem mãos vazias ou vaga para ferramenta. As variantes sazonais
conferidas mantêm os mesmos registros. O gatilho marca a flag ao iniciar.

Os roteiros instalados de `12_event_s.bin` são executados diretamente:

- **277**, intervalo **0x698000..0x698800**, 18 instruções: colmeia `0x0883`,
  ator de abelha `0x201b`, banco `0x8000` e script `0x4000`. Dispatch **15/0**
  habilita a colmeia; a câmera salta para a abelha, faz fade/glide até a árvore,
  e os slots da câmera com sorteio 0..50 mantêm seu movimento vertical.
  O ator fica sem marcador, desaparece, aguarda 60 frames originais e devolve
  o controle. Não é necessário conversar ou confirmar uma mensagem.
- **171**, intervalo **0x415800..0x417800**, 18 instruções: ator Louis
  `0x201f`, banco e script. Usa câmera, caminhada do jogador, mensagens
  **1:492/493**, saída de Louis e dispatch **15/1**, que habilita o bônus.

A conversa instalada de Louis, **0x75e000..0x75e800**, já era atendida pelo
interpretador de conversas. Dar mel simples (`151`) inicia `count0[19]` em
zero quando estava em 255. A atualização diária o leva a um: a visita pode
ocorrer na saída da casa da manhã seguinte. A conversa distingue o ID da mão;
um embrulho não percorre automaticamente esse ramo especial de mel simples.
Não foram inventados gatilho por afinidade nem presente automático.

Auditoria do `11_GF_FARM.bin` instalado, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Conferência |
|---|---|
| **0x80104ec8**, imediatos **0x80106404..0x801064e4** | Condição das cinco flores maduras no campo externo. |
| **0x800f3f7c..0x800f3fb0** | `estate_flag_take_once`: máscara `0x04000400`, exige aquisição e ausência de coleta; marca `0x04000000`. |
| **0x800ea904..0x800ea91c** | Dispatch 15/0 acrescenta o bit de aquisição `0x400`. |
| **0x800ea920..0x800ea934** | Dispatch 15/1 acrescenta o bônus `0x00400000`. |
| **0x800e8dc8 / 0x800e9838** | Posição de ator por slots e leitura do centro da câmera. |
| **0x800d603c** | `item_sell_price`, bônus do mel existente. |

Os endereços são referências de auditoria, nunca ponteiros host. A conferência
leu as palavras little-endian e seus imediatos no BIN instalado. O mapa local
serviu apenas para localizar as funções; não substitui a leitura do BIN.

## Integração

`apiary.owned/collected` pertencem à sessão. O codec vigente **v22** guarda
as duas flags; o bônus, contador de Louis e flags dos eventos já tinham donos
e persistência. Não foi aberto trabalho de migração de versões.

`apiary.c` adapta `estate_flag_take_once` e `player_act_zone_front(ZONE_HONEY)`:
Cross na zona **47**, olhando nas direções **1 ou 2**, com mãos vazias, recebe
**151/0/1** uma vez por dia. O mesmo registro vai para a mão; a apresentação
usa a fase de coleta existente, ANI `11+direção` e som 7. Mãos ocupadas,
orientação inválida, colmeia ausente e repetição não retiram mel nem arremessam
o item carregado contra a árvore. A manhã limpa somente a flag diária.
Não existe bloqueio sazonal para o mel, inclusive no inverno.

A colmeia permanece desenhada após coletar, com posição original do registro
0 e sprites sazonais **20/26/26/11**. Durante 277 usa a figura/banco do próprio
bloco. A abelha usa o vínculo temporário de ator 27 e Louis o de ator 31;
ambos são liberados para a agenda normal ao concluir. O sorteio da abelha
usa o estado nativo persistido da vila; não se declara paridade da ordem
global de RNG com o PS1.

Mel usa os fluxos existentes de mochila/consumo e remessas: comer recupera
**2 stamina** e reduz **2 fadiga**; o preço base é **50 G**, passando a **60 G**
com o bônus. O pagamento continua no mesmo fechamento diário, sem duplicação.
O efeito anterior do funeral sobre esse mesmo bônus foi preservado.

## Validação

`apiary_test` cobre as cinco flores, estágio incompleto/maduro, campo externo,
estufa, entrada e fronteiras de horário, climas/estações, execução única,
contador de Louis 0/1/127/128/255, mãos/orientações, coleta duplicada,
codec e rejeição de estado incoerente, reset diário idempotente e preços.

`apiary_playthrough` compra Pink Cat de Won, prepara/semeia a terra, rega e
passa seis manhãs pela escolha original da cama. A colmeia, o mel, a contagem
do presente e o bônus nunca são concedidos pelo driver. Dinheiro inicial,
posições, previsão ensolarada e uma célula inicial limpa são fixtures.
O percurso executa 277, coleta, tenta repetir com mãos ocupadas e vazias,
guarda/retira na mochila, come, coleta no dia seguinte e remete/paga 50 G.
Encontra Louis pela agenda e pelo location original, dá o mel, dorme, executa
171 e remete/paga 60 G. Confere ausência de repetição e desenha as quatro
variantes sazonais. Checkpoints de aquisição, coleta, presente e conclusão
são comparados na retomada e reabertos pelo aplicativo em processos novos.

Configure/build Linux Debug e Release aprovados. Debug: **11/11** selecionados
(colmeia/percurso, codec, sessão, variáveis/eventos, flores, Greg, lago,
conversa e regressão de chuva/portas/coleta/descarte), além de remessas e
percurso de Moondrop/Karen avulsos. Release: **3/3** (colmeia/percurso e codec).
Os wrappers de Debug/Release reabrem os quatro checkpoints em novos processos.

Build Windows MinGW sem rede aprovado. Unidade, percurso e as quatro retomadas
Windows no Wine passaram com saída zero. Capturas locais das abelhas, Louis e
variantes sazonais foram produzidas; abelhas/Louis/inverno foram inspecionados.
Nenhum recurso proprietário ou captura entrou no repositório.

**47 processos de teste**: **26 Debug**, **9 Release** e **12 Wine/targets**,
incluindo CTest, wrappers, filhos e tentativas iniciais. As quatro tentativas
iniciais corrigiram fixtures de solo, espera do gatilho após a transição,
modo de salto da câmera ainda ausente e busca de Louis pela agenda.
Não houve execução da suíte inteira; teto de 50 respeitado.

`runtime.c` recebeu somente dispatch/vínculos dos eventos, sem alterar o
bloco de portas. `git diff --check` passou. `src/`, `sdk/`, `disc/`, `tools/`
e o fluxo PS1 permaneceram intactos. O build host não substitui a toolchain
nem a verificação SHA-1 do original; Wine não valida hardware Windows físico.

## Limites e próximo recorte

O recorte de colmeia/mel está integrado. Não conclui todos os eventos de Louis,
festivais, família ou PF-06. Próximo: maçãs da árvore da fazenda, auditando
produção/estação/quantidade, coleta, uso e retomada pelos mesmos sistemas.
