# Presentes e aniversários dos moradores

Os scripts comuns dos moradores agora podem receber um item na mão de
produtos. Z/Cross inicia a conversa com o morador à frente antes da tentativa
de arremessar/coletar. As lojas de Barley/Lillia mantêm prioridade quando o
jogador está de mãos vazias; com um presente, o mesmo morador pode executar
seu roteiro de presente durante o expediente.

As falas, escolhas, comparações, saltos e alterações de amizade/afeição vêm
de `12_event_s.bin`. As mensagens e retratos usam os bancos BIN e as interfaces
existentes. A entrega não introduz diálogos substitutos, animações novas ou
outro carregador gráfico. A corrida continua permitindo atravessar NPCs.

## Reação, consumo e estado

`port/game/item_reaction.*` adapta `item_villager_reaction` de
`picture_cells.c`. `ItemInfo.picture` seleciona uma das 174 linhas de quatro
palavras de `picture_codes.c`; cada palavra tem dez códigos de três bits.
A visão nativa é gerada no configure a partir da tabela C original, mantendo
os literais octais e valores de 32 bits. Não há endereços host nessa tabela.

Ferramentas retornam −2 e itens sem imagem retornam −1. Os códigos 0..4 são
as cinco preferências; 5/6 são reações sem aceitação normal; 7 não fornece
uma reação de presente. Itens com flags `0x180`, como animais carregados,
convertem 0..2 em 5 e 3..4 em 6. O comando 57 grava a reação de base 1 no slot
solicitado. Com a mão vazia, retorna sem alterar esse slot, como no original.

Um presente embrulhado (`0xA4`) usa seu modificador como conteúdo na consulta
de reação. O comando 58 conserva a opção original de consultar o id da caixa
ou seu conteúdo. O comando 30 encontra/remove itens no recorte de depósitos
já suportados, incluindo mão e mochila; o modo indireto usa a metade inferior
assinada de 16 bits da variável. O item só desaparece quando o bytecode pede
a remoção. A rotina original limpa o registro encontrado inteiro, inclusive
se sua quantidade é maior que um; não foi criada outra regra de consumo.

O script também pode incrementar o contador original `player.counts70[1]`,
com limite 15, por meio do comando 81/22. O estado nativo fica em
`HmbtnHouseInteraction.event_count1`. Isso não implementa todos os sistemas
que possam consultar os contadores do jogador.

As flags de primeiro presente/repetição, valores imediatos e bônus de
aniversário seguem cada roteiro. O aniversário das cinco candidatas muda
quando coincide com o aniversário cadastrado do jogador. Não há um cálculo
global de pontos que substitua essas instruções. Exemplos verificados na
edição instalada:

- Lillia e peixe grande (`0xA1`): amizade 50 → 59 no primeiro presente do dia e
  59 → 62 no seguinte. No aniversário, o primeiro passa de 50 para 95.
- O ramo embrulhado de Lillia subtrai 1 do ganho antes do multiplicador;
  os casos verificados passam de 50 para 58, ou 90 no aniversário.
- No roteiro de Karen instalado, o multiplicador de aniversário está dentro
  do ramo embrulhado. Os testes mantêm essa condição e a troca de aniversário,
  inclusive quando a fala de aniversário não multiplica um item sem embrulho.

Esses detalhes registram o comportamento do bytecode local; não comprovam que
outras edições tenham os mesmos operandos. As preferências vêm da tabela C
preservada no repositório. A edição PT-BR instalada também altera código PS1;
a equivalência de suas tabelas executáveis com a fonte americana continua sem
validação completa.

Durante a conversa, movimento, menus, rotas e relógio ficam pausados. A imagem
do item permanece na mão até o script consumi-lo. Ao terminar, o estado visual
de carregar acompanha o inventário. Se uma ação ainda ausente interrompe o
roteiro, são restaurados os itens, contadores, flags e afinidade de todos os
moradores, inclusive alterações feitas num registro diferente do alvo.

## Validação e limites

`native_item_reaction` verifica códigos originais, grupos de palavras, limites,
ferramentas, ausência de imagem e conversão das reações de animais.
`native_village_talk` verifica consumo, contagem e restauração de item/pontos
em múltiplos moradores ao atingir uma ação ainda ausente.

`native_village_gifts_playthrough` executa 186 casos de primeiro/repetido
presente e apresentação de animal em 31 moradores, além de aniversários,
embrulhos, perdas e aniversário alternativo. Compara 197 solicitações de
mensagem com os glifos/controles dos BINs. Datas, itens e posições desses casos
são fixtures. Um percurso adicional coleta um item real no bosque e o entrega
à Lillia durante o expediente: confere prioridade sobre arremesso/loja,
pausa dos controles/relógio, consumo, retorno e captura local.

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_village_gifts_playthrough data \
  /tmp/hmbtn-gifts-lillia.bmp
```

No jogo, colete um item, leve-o na mão até um morador e pressione Z/Cross.
O porte ainda não oferece a loja de embrulho; os casos embrulhados são
fixtures que verificam o suporte do interpretador. Won, família, ajudantes,
cenas românticas, festivais e comandos de história além do recorte atual
continuam pendentes. Esses limites continuam explícitos no terminal, com
restauração do estado; os testes não afirmam paridade de todos os eventos.

A vara de Greg continua disponível conforme [GREG_ROD.md](GREG_ROD.md). A
pesca agora executa física da boia, animações de ação, captura e resultados,
com os limites descritos em [FISHING.md](FISHING.md). Não há saves persistentes.
Validação em Linux SDL dummy/software, sem equivalência Windows real ou
substituição do build PS1/SHA-1. `--diagnostics` executa somente o bootstrap.
