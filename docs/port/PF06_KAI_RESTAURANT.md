# PF-06 — Restaurante de Kai

Recorte de 2026-10-30 do [roadmap](ROADMAP.md). Kai abre seu cardápio pelo
roteiro original de conversa. A adaptação reutiliza o modal e o recebimento
de refeições de Doug, com estoque, mensagens e acesso próprios.

## Referência C e BIN

`menu/shop_dialogues.c`: `shop_menu_setup_snacks`, `shop_goods_run` e
`shop_menu_open_choose`. `event/event_commands.c`: dispatch **(8,8,0,0)**,
`SHOP_DIALOGUE_ITEM_17` (**23**, setup **22**).

O NPC **25** ocupa o intervalo **0x75a000..0x75a800** de `12_event_s.bin`.
Sua entrada tem **124 instruções**: vincula o ator, vira-o para o jogador,
desvia para conversa/presente se a mão estiver ocupada e consulta a variável
**38**. Essa variável é o **byte alto da rota vigente**, não o local físico.
Somente o valor **6** alcança o dispatch da loja; a instrução seguinte
**(8,0,0,1)** aguarda seu fechamento antes de concluir o roteiro e restaurar
a direção. Não foi criado atendimento direto por área/horário.

`cue_history.c` e o bloco de agendas do `00_gf.bin` (**0x74800**, tamanho
**0xa800**) selecionam os offsets **0x8bb0/0x8ca0/0x8d90/0x8e80** para Kai:
verão comum, domingo, domingo com chuva e data de evento. Fora do verão não
há agenda, e o local é **54**, ausente. No verão comum, o tipo de rota **6**
aparece de **09:00 a 10:59** e de **13:15 a 16:59**, no local **44**, área
**56**. O almoço usa outra rota, no local **46**. Domingo ensolarado permite
encontrá-lo na praia sem vender; domingo chuvoso/datas de evento usam a rotina
da hospedaria. As agendas, incluindo atrasos e movimentos, continuam sendo
interpretadas pelo domínio já existente; esses horários não viraram filtros
novos de loja.

Auditoria LE em `A_FILE/11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**:

| Endereço | Conferência |
|---|---|
| **0x80117a40** | `shop_menu_setup_snacks`, cinco IDs/descrições na ordem original. |
| **0x80117bb4** | `shop_goods_run`, débito imediato, mensagens e recebimento em modo 2/3. |
| **0x80117e28..0x80117e34** | Água **0xab** escolhe mensagem **0x200**; demais pratos usam **0x131**. |

O catálogo de `disc/SLUS_011.15`, endereço **0x8004b1a4**, passo **40 bytes**,
confere categoria **3**, callback **0x800d622c** (`farm_clamp_status`) e:

| Opção | ID | Preço | Stamina | Fadiga | Descrição, banco 2 |
|---|---|---|---|---|---|
| Snowcone | 172 | 300 G | 0 | -10 | 0x12c |
| Milho assado | 309 | 250 G | +10 | -1 | 0x129 |
| Massa | 310 | 300 G | +20 | -1 | 0x12a |
| Pizza | 311 | 200 G | +15 | -1 | 0x12b |
| Água | 171 | 0 G | +1 | 0 | 0x12d |

Snowcone tem flags **0x1200** e usa a ANI de comida apesar de não aumentar
stamina. Água tem flags **0x1600** e usa a ANI de bebida. Felicidade não muda.
Nomes/textos vêm dos recursos fornecidos. O identificador de descrição da água
**2:0x12d** foi preservado conforme C/BIN, inclusive o texto de conversa que
aparece nesse registro na tradução instalada; não foi substituído por texto
inventado. Endereços PS1 servem à auditoria, sem conversão em ponteiros host.

## Adaptação

`generate_farm_tables.py` extrai as cinco linhas, nomes e mensagens para
`kai_menu.inc`; preços/efeitos continuam vindo do catálogo compartilhado.
`village_talk` reconhece o dispatch original de Kai. A abertura valida mão
vazia e modal livre, preservando presença e seleção de agenda pelo roteiro.

`feed_shop` passa a compartilhar `meal_tick` e os helpers de linhas
`hmbtn_meal_shop_rows/entry` entre Doug e Kai. A diferença está nos dados:

- Abertura **2:0x128**, seleção **2:0x46**, informações por Triangle e
  cancelamento **2:0x12e** sem débito ou entrega.
- Sem saldo: **2:0x4f**, depois **2:0x12f**, voltando à mesma seleção.
- Compra: débito único, **2:0x130**, depois **2:0x131** para comida ou
  **2:0x200** para água, antes de receber/consumir. A mensagem final é
  derivada do item a cada atendimento, sem contaminar a compra seguinte.

O recebimento comum guarda o registro completo da mão, executa comida
**ANI 5/som 74** ou bebida **banco 17/som 75**, oculta a figura extra no
momento apropriado e aplica os atributos somente ao terminar a animação.
O registro anterior retorna; nenhum prato fica disponível na mochila.
Água é gratuita e repetível, conforme o original. Não há fade ou avanço
explícito de tempo neste serviço. Controles e gravação ficam bloqueados
durante o consumo, usando as guardas existentes.

O codec v19 existente é suficiente. `runtime.c` é idêntico à cópia do início
deste recorte, preservando integralmente as alterações de portas do usuário.
Nenhuma edição em `src/`, SDK, recursos ou fluxo CMake PS1. O build host não
substitui toolchain/verificação SHA-1 PS1.

## Verificação funcional

`kai_shop_test` cobre as cinco linhas/preços/descrições, seleção circular,
informações, cancelamento, falta de saldo, cobrança única e água sem despesas.
Compra água/comida/água para conferir a mensagem final. Exercita quatro faces
de cada ANI, mochila cheia, restauração do inventário, efeitos adiados e
impedimento de recebimento duplicado.

`kai_shop_playthrough` usa recursos instalados e controles reais:

1. Confere agendas originais nas fronteiras **08:59/09:00/10:59/11:00** e
   **13:14/13:15/16:59/17:00**, domingo, chuva dominical, data de evento e
   ausência no outono, sem modificar a agenda de produção.
2. Abre a loja pela conversa real de Kai, cancela e tenta comprar sem saldo.
3. Compra água, os quatro pratos e água novamente, verificando mensagens,
   consumo, banco/ocultação, efeitos e cobrança únicos, e término do roteiro
   sem fronteira de comando não implementado.
4. Retoma checkpoints de água/comida/serviço encerrado, verifica a ausência
   de Kai no almoço e o retorno da loja à tarde. Em uma sessão de domingo,
   conversa com Kai na praia e confirma que não abre loja.

Saldo **2.000 G** e atributos iniciais são fixtures; os itens são comprados.
Termina com **950 G**, stamina **47/200**, fadiga **87**, felicidade **12**,
sem item na mão. Três checkpoints são abertos em processos novos do app.
Capturas locais conferem lista/cursor, informações, mensagens, comida e bebida;
nenhum recurso ou captura proprietária entra no Git.

Configure/build Debug e Release aprovados. Debug **16/16** selecionados:
regras/percurso de Kai, regras/percurso de Doug com três retomadas cada,
consumo, inventário/comandos, lojas/saves e regressões de chuva/portas/
fidelidade. Release **4/4**: Kai/unidade/percurso/três retomadas, Doug/unidade
e consumo. Build Windows MinGW aprovado em contêiner local sem rede.
Wine aprovou unidade, percurso e três retomadas no aplicativo Windows.
Todas as execuções de teste terminaram com saída **0**, sem tentativas falhas.

**46 processos de teste**: dois avulsos iniciais, 25 na seleção Debug, nove na
seleção Release e dez Wine/targets. Inclui CTest, wrappers, produtores, filhos
e lançadores; teto de **50** respeitado. Não foi executada a suíte inteira.
Avisos de Mesa/EDID no Wine e avisos antigos de `const` nos testes de mundo
não impediram os builds/testes; não constituem validação física de vídeo/áudio.
`git diff --check` e comparação de `runtime.c` com a cópia inicial aprovados.

## Limites

O restaurante está entregue neste recorte. Lago/ração de peixes é o próximo
recorte selecionado; demais eventos de Kai, festivais, família e hospedagem
continuam abertos em PF-06. O bootstrap `--diagnostics` não executa o jogo;
o runtime permanece parcial. Wine não substitui teste em Windows físico.
