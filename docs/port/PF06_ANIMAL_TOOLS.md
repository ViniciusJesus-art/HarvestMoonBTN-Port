# PF-06 — Ferramentas animais, escovação, ordenha e tosquia

Entrega de 2026-10-28. Próxima fila: [ROADMAP.md](ROADMAP.md). Alvos Linux/Windows;
Android futuro. O runtime continua um port parcial; este recorte não conclui
PF-06, todos os serviços do ferreiro ou o ciclo completo dos animais.

## Comportamento e fontes

Referência original `fcef049`; nenhum arquivo em `src/` foi modificado.

A conversa instalada de **Saibara**, personagem 4, abre o modal existente pelo
`event_cmd_dispatch(8,2,0,0)`, conforme `event_commands.c` e o roteiro original.
O próprio roteiro continua decidindo disponibilidade/horário; não há atalho de
NPC ou concessão gratuita. `shop_tool_upgrade_menu_setup` fornece a ordem e os
nomes, e `shop_stock_step` a escolha, mensagens, espaço, pagamento e entrega.

| Ferramenta | Item | Preço | Descrição | Banco da ação |
|---|---|---|---|---|
| Escova | 0x1C | 800 G | 0xAA | 20 |
| Ordenhadeira | 0x1A | 2.000 G | 0xAB | 18 |
| Tesoura | 0x1B | 1.800 G | 0xAC | 19 |

Nomes/ordem/IDs/descrições são extraídos da função original; preços e uso já
estão no catálogo gerado de `farm_tables.c`. A UI usa preço por linha, como o
flag 0 deste submenu original. Não há painel de quantidade para estas compras.
A lista esconde ferramentas já possuídas em equipamento, mochila ou baú,
consultando o inventário autoritativo. O baú não conta como vaga de entrega:
esta compra exige espaço no equipamento/mochila, preservando
`shop_item_room(item,1)` e `shop_item_add(...,mode=1)`.

Cancelar ou faltar espaço/dinheiro não muda estado financeiro ou posse. A
aceitação 0xAE precede o único armazenamento/débito; recursos ou mensagens
pendentes não antecipam o pagamento. Todas as ferramentas compradas levam à
mensagem original 0xAD de estoque esgotado. A instrução original de melhorar
ferramenta continua disponível. Presentes/máquinas e ordens de melhoria ainda
não implementadas são filtrados deste menu parcial, não compras simuladas.

`livestock_cow_cue`, `livestock_sheep_harvest`, `livestock_set_bit2_cue/_b` e
`farm_cue_praise` fornecem as regras adaptadas em `port/game/livestock.c`:

- Vaca jovem não produz. Adulta alimentada ontem produz uma vez por dia. Sem
  alimentação anterior, a tentativa marca a ordenha do dia sem conceder produto.
  Afeição 0/1 não gera leite; 2..100 dá S, 101..200 M e acima de 200 L.
- Ovelha adulta com lã disponível produz S até 100 de afeição, M até 200 e L
  acima. Tosquiar zera o contador e seleciona o pacote original tosquiado 8.
  Após sete incrementos, retorna o pacote 5; os incrementos consultam
  `fed_yesterday` antes de substituir essa informação pela alimentação atual.
  Portanto, a primeira alimentação não antecipa um dia de lã quando ontem não
  estava alimentada.
- Acima de 200 com `prize_winner`, o produto é G. O cálculo está validado por
  vetores; ganhar o prêmio em uma partida normal aguarda os festivais.
- Escovar marca `brushed_today` e inicia praise apenas na primeira aplicação
  do dia e com afeição suficiente. Esses handlers não somam afeição diretamente;
  não foi transplantada a regra diferente do cachorro/cavalo para vacas/ovelhas.
- Os handlers de produto examinados não têm recusa por `daysSick`. Não foi
  inventada essa condição. Mortos, espécie incorreta e maturidade insuficiente
  não produzem. Os registros atuais são não gestantes/sem ajudantes; os guardas
  originais desses sistemas entram quando existir o respectivo estado real.

Produtos originais: leite S/M/L/G = 0x8B/0x8C/0x8D/0xE3, preços
100/150/200/300 G; lã S/M/L/G = 0x91/0x92/0x93/0xE7, preços
100/400/500/600 G. A caixa, os contadores de remessa e o pagamento usam o mesmo
`farm_shipping` e a mesma economia existentes.

## Estado e apresentação

`HmbtnFarmAnimal` em **`session.livestock`** possui `brushed_today`,
`milked_today`, `prize_winner` e o contador existente da lã. Nenhum desses dados
é copiado para a cena/loja. A atualização diária conserva vacas antes de ovelhas,
alimentação antes da limpeza dos cochos e limpeza das flags diárias no mesmo
observador, sem nova atualização paralela.

`HmbtnFeedShop.inventory` é empréstimo de leitura para derivar o estoque;
`blacksmith` seleciona uma variação da máquina de menus já existente.
`inventory.c` compartilha a busca de vaga entre consulta e armazenamento, sem
inventário temporário usado como segunda fonte de verdade.

`HmbtnFarmAction` tem uma fase transitória de ferramenta animal. Ela prende o
índice do alvo, aguarda a ANI original e aplica o efeito uma vez. Bancos, som de
uso e pose vêm do caminho de `people/player.c`. O produto usa a apresentação
compartilhada `event_item.c`, adaptada de `player_show_item`, e termina na mesma
mão do inventário. Scripts de torta/bolo conservam seus comandos e regras;
não foi liberado genericamente um comando de script ainda não implementado.
Desenhar não ordenha, escova, avança contador nem entrega produtos.

O save passa a **v11**, somente v11: os três booleanos adicionais ficam em LSTK.
Não há migradores de v10/anteriores, conforme decisão do usuário. Persistem as
mesmas células, depósitos, economia, registros/RNG, obras e calendário.
Ações/apresentação em andamento continuam não salváveis; os checkpoints
estáveis preservam cuidados/ordenha sem permitir produto duplicado ao carregar.

Nenhuma nova unidade original é compilada diretamente. São adaptações dos
blocos acima, mais dados extraídos, com SDL3, áudio, leitores, renderer, recursos
e instalação pelo BIN preservados. Sequência completa de cues/vozes XA,
reprodução/gestação, venda, máquinas, pastagem e funerais continuam pendentes.

## Arquivos desta entrega

| Arquivos | Alteração |
|---|---|
| `port/game/livestock.c/.h` | Regras de cuidado/produção, flags por animal, ação e seleção de pacote pelo contador comum. |
| `port/game/feed_shop.c/.h`, `village_talk.c` | Variação Saibara do modal; catálogo/estoque por empréstimo; abertura/espera do roteiro real e transação única. |
| `port/game/inventory.c/.h` | Busca única de vaga compartilhada por consulta e entrega. |
| `port/game/farm_action.c/.h`, `exploration.c` | Custo compartilhado, fase animal e integração com tick/ANI existentes. |
| `port/game/event_item.c/.h` | Generaliza a apresentação de produto, conservando contratos anteriores dos scripts. |
| `port/game/save.c/.h` | Três booleanos em LSTK e versão 11, sem outras cópias ou migradores. |
| `port/graphics/feed_shop_ui.c`, `port/cmake/generate_farm_tables.py`, `port/CMakeLists.txt` | Menu, metadados originais gerados e registro dos testes. |
| `port/tests/animal_tools_test.c`, `animal_tools_playthrough.c` | Vetores de regras e percurso com recursos instalados. |
| `port/tests/event_item_test.c` | Corrige expectativa antiga de acesso à geladeira e mantém asserts em Release. |
| `.github/workflows/native.yml` | Inclui `native_animal_tools` na seleção automática de portabilidade (9/configuração, 36 na matriz). |
| `docs/port/ROADMAP.md`, `ARCHITECTURE.md`, `PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `ROADMAP_STATUS.md`, `PF05_NATIVE_SAVES.md`, `WINDOWS.md`, este relatório | Estado real, propriedade, versão de save, fontes, validação e próxima tarefa. |

As alterações previamente existentes em `README.md` e `README-PT-BR.md` foram
preservadas. Não houve alteração em `src/`, `sdk/`, `disc/`, `tools/`, CMake
raiz/presets nem publicação no remoto. O build PS1 não participa da validação PC.

## Validação

- Referência inicial: **4/4** (gado, save, regras de inventário e loja de ração).
- Configure/build Linux Debug e Release e cross-build Windows x64 MinGW Release
  aprovados, incluindo os alvos de teste. SDL3 local reutilizada, sem download.
- Debug: seleção de **16 casos** com 28 processos incluindo wrappers/filhos.
  Quinze passaram na rodada; `native_event_item` encontrou uma expectativa
  antiga de que `which=2` para itens ainda não tinha geladeira. Esse acesso já
  existia antes desta entrega. O teste passou após usar o seletor inválido 4;
  o contrato de produção não foi alterado para satisfazer a expectativa antiga.
  Os 16 casos selecionados, portanto, estão aprovados, sem repetir toda a rodada.
- A seleção protege gado, ferramentas, inventário/regras/estado, itens por
  script, save/retomada, ração/cochos/lojas, exploração e cultivo; percursos de
  aquisição/alimentação do gado, ferramentas animais, saves, estufa, Orangecup
  e inventário. Unitário novo também passou em execução dirigida.
- Release: **3/3**, em **1,23 s**: regras de ferramentas, itens por script com
  asserts ativos e percurso completo + aplicativo em outro processo.
- Windows/Wine: unidade nova, percurso completo e carga pelo aplicativo em
  outro processo passaram. Isso valida os binários MinGW; não é validação de
  MSVC real nem de uma sessão Windows real. A CI mantém `/W4 /WX` e a matriz
  Linux/Windows Debug/Release, aguardando a próxima execução remota.
- `native_animal_tools` cobre preços/IDs/descrições, estoque, instrução, cancelar,
  espaço antes de fundos, pagamento posterior/único, posse após recarga,
  maturidade/espécie/morte, limiares S/M/L/G, alimento anterior, repetição,
  escovação, lã/7 atualizações/arte e flags no save, ANI e entrega pela mão.
- `native_animal_tools_playthrough` usa a conversa real de Saibara, menus/recursos
  instalados, três compras e recusas. Fixtures fornecem dinheiro, dois animais
  nomeados não gestantes, afeição e vaca um dia antes da maturidade. Não fornecem
  as ferramentas. Alimenta nos cochos reais e executa oito noites, ações de
  brush/milk/clip, repetição, mochila, portas, remessa/pagamento, sete datas da lã
  e reloads intermediários. Os percursos anteriores protegem a aquisição dos
  animais com Barley; essa fixture não se apresenta como progresso obtido por
  compra de todos os pré-requisitos dentro deste mesmo teste.
- Capturas dos menus, descrição, produto e ovelha tosquiada foram conferidas;
  captura estável compara o save antes/depois para impedir mutação por draw.
- Limite de **50 processos** respeitado em contagem conservadora, incluindo
  tentativas, scripts/wrappers, Wine e sua execução Windows, e conferência das
  capturas. Uma fixture unitária inicialmente repetia o aniversário em cada
  atualização e envelhecia a ovelha artificialmente; passou a avançar a data.
  Uma tentativa Wine não iniciou o teste porque seu prefixo temporário anterior
  não existia; prefixo isolado no build resolveu, sem alteração no jogo.
- Não executados: suíte inteira, MSVC/nova CI, Windows real, Android, instalação
  limpa, sanitizers, festivais, reprodução, máquinas ou sessão manual de jogo.
  `git diff --check` passa nos arquivos desta tarefa; o whitespace já existente
  no README editado pelo usuário foi preservado.

Logs locais: `/tmp/hmbtn-animal-tools-baseline-tests.log`,
`/tmp/hmbtn-animal-tools-ctest.log`, `/tmp/hmbtn-animal-tools-event-item.log`,
`/tmp/hmbtn-animal-tools-release-tests.log`,
`/tmp/hmbtn-animal-tools-playthrough.log`,
`/tmp/hmbtn-animal-tools-windows-build.log`,
`/tmp/hmbtn-animal-tools-windows-unit.log`,
`/tmp/hmbtn-animal-tools-windows-playthrough.log` e
`/tmp/hmbtn-animal-tools-windows-resume.log`.

## Como experimentar

A fixture local produzida nesta entrega está em `/tmp/hmbtn-animal-tools.hms`:

```sh
./build/native-debug/port/hmbtn_native --data data \
  --load-save /tmp/hmbtn-animal-tools.hms
```

Ela termina no celeiro, depois de oito noites, com as três ferramentas compradas
no inventário e lã na mão após a segunda tosquia. É uma fixture, não uma campanha
completa obtida pelo jogador. Saves antigos precisam de jogo novo; só v11 é lido.
Arquivos temporários podem ser removidos pelo sistema.

Para reproduzir o percurso com seus recursos instalados:

```sh
mkdir -p /tmp/hmbtn-animal-tools-shots
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_animal_tools_playthrough \
  data 0 /tmp/hmbtn-animal-tools.hms /tmp/hmbtn-animal-tools-shots
```

`ctest --preset native-debug -R '^native_animal_tools'` executa regras e percurso
com retomada em outro processo quando os recursos de teste estão configurados.
A próxima implementação é **venda de vacas e ovelhas com Barley**, no roadmap.
