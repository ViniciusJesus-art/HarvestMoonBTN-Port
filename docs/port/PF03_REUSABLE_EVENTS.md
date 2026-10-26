# PF-03 — Comandos compartilhados e convite de Duke

Entrega de 2026-10-26, referência de origem `d3c0f38`, sobre a árvore local com
MA-01..06 e PF-01/02 já presentes. Linux/Windows são os alvos; Android é futuro.
Esta entrega conclui o convite de Duke para a vindima (evento 140), nos dois
ramos, no mesmo interpretador dos eventos existentes. A vindima do dia seguinte,
Cliff e o restante da cadeia continuam em PF-06. O jogo completo e saves em disco
continuam ausentes; `--diagnostics` não executa o jogo.

## Seleção e sequência original

O evento foi registrado no roadmap antes da alteração da lógica. Foram lidos
o dispatch e os comandos C originais, o bloco 140 do BIN instalado e seu gatilho
nas tabelas da fazenda. Não foi criada uma sequência substituta em C.

O gatilho original é entrada na fazenda, no primeiro ano, outono dia 14, entre
06h e 12h (12h excluída), em qualquer dia da semana/clima coberto pela tabela,
sem condição extra de item. A flag `251+140=391` impede repetição. O runtime usa
a tabela instalada e seus predicados; não codifica novamente essa data no tick.
Essa flag é marcada no início, conforme `hmbtn_farm_visit_begin` já fazia;
não representa um novo checkpoint de conclusão ou um save.

O bytecode possui 18 instruções, incluindo o terminador. Usa os tipos
1, 2, 4, 12, 15, 19, 27, 35, 37 e 255: reset/posição, face, fade, mensagem,
escolha/desvio, alterações de variáveis, movimento e encerramento.

- Duke é o residente 21. O bloco inclui sua ANI (entrada 0x2015) e banco de
  texturas; ambos passam pelos decodificadores/validações existentes.
- Mensagem 1:418 apresenta o convite e a escolha. Aceitar segue 1:419 e 1:420;
  recusar segue 1:421. Nome do jogador é expandido usando seu cadastro real.
  Textos, retratos e escolhas são os recursos do usuário, sem strings recriadas.
- Aceitar escreve a variável 253, que referencia a flag 205, e executa **duas**
  instruções de amizade `-20`, com clamp em zero. Essa sequência surpreendente
  consta no bytecode e foi conservada; não foi presumido um prêmio de amizade.
  Recusar não faz essas alterações. Nenhum ramo cobra dinheiro ou dá itens.
- O próprio script move Duke e solicita término. O encerramento libera a
  identidade de evento, retoma a rotina do morador e devolve o controle.
  O convite não se repete ao permanecer na área ou sair e voltar.

## Reaproveitamento e arquitetura

Nenhum módulo original passou a ser compilado diretamente. A entrega adapta
as funções abaixo em `port/`, conservando o algoritmo e substituindo dependências
de endereços/globais PS1 por referências explícitas ao estado nativo.

| Origem em `src/overlay/11_GF_FARM/` | Adaptação/integração nativa |
|---|---|
| `event/event_commands.c`: `event_var_get`, `event_var_update`, `event_var_arith`, `event_var_compare`, `event_var_clamp`, `event_var_address` | Novo `port/game/event_variables.c/.h`: campos tipados, flags, calendários, contadores, afeto/amizade e máscaras já representados. Sem usar ponteiros serializados, stride de struct PS1 ou alias de união. |
| Mesmo arquivo: `event_cmd_test_var`, `event_cmd_test_var_slot`, `event_cmd_test_slot`, `event_cmd_test_slots`, `event_cmd_test_var_range`, `event_cmd_test_slot_range`, `event_cmd_set_var`, `event_cmd_set_var_from_slot`, `event_cmd_slot_arith`, `event_cmd_slot_arith_var` | Uma implementação dos tipos 13..22 em `hmbtn_event_variables_command`, usada por conversas, visitas e operações de slots da abertura. Removidas as implementações privadas de aritmética/comparação e os ramos estreitos equivalentes em `village_talk.c`, `farm_intro.c` e `farm_visit.c`. |
| `event/event_script.c`: `event_script_run` e leitura dos slots do bloco | Interpretador `event_script.c` existente conservado. O loader reconhece o ator residente válido do bloco 140 e continua exigindo as entradas/bancos esperados, sem ignorar recursos desconhecidos. |
| `event/event_commands.c`: `event_cmd_actor_reset`, `event_cmd_set_position`, `farm_nudge_cue`, `event_cmd_face_player`, `event_cmd_enter_mode` | Handlers existentes em `farm_intro.c`, com resolução dos atores ligados à sessão, integração de movimento e reset dos bindings ativos; término da visita aceita o comando original `(0,0,0,0)`. Sem campo de ator novo específico para Duke. |
| `event/event_commands.c`: `event_triggers_fire`; `event/event_request.c`: requisições | Leitor/predicados atuais em `farm_visit.c` e início em `scene_preview.c` passam a aceitar o evento 140. Bytes LE e IDs originais continuam nos adaptadores de recursos. |

`HmbtnEventVariables` é uma visão emprestada, sem propriedade de dados. Flags,
contadores e relações continuam em `HmbtnSession.village`; data/cadastro em
`HmbtnGameState`; posições no ator ativo. Os slots pertencem à execução de cada
roteiro. Não foi criado um segundo estado de progressão ou sincronização manual.
O contexto recebe ponteiro/tamanho do span de slots; o teste usa 64 posições,
sem impor o tamanho 32 do bytecode original como limite universal da operação.
Os containers dos roteiros existentes conservam os tamanhos atuais.

`HmbtnSession` fornece um resolver de atores residentes ligados por identidade.
Duke usa `visit.actors[21]`, com `village.event_actors[21]` apontando para essa
instância ativa; a rota normal fica suspensa. Getters, comandos, movimento e
desenho encontram o mesmo ator. Consultar sua direção também passa pela
identidade ativa. Ao liberar, a rotina volta ao registro normal. O resolver
permanece válido durante desvinculação de recursos, pois pertence à sessão.
Os atores locais da infância mantêm seu significado próprio.

A integração no adapter gráfico apenas carrega/desenha/avança ANI; regras de
variáveis permanecem em `game/`. Removidos desenho e avanço duplicados dos
bindings do evento de ingredientes que o novo resolver tornaria alcançáveis
por dois caminhos. O agendamento global ainda depende da PF-04; o teste com
recursos conserva o acesso privado à cena já utilizado pelos demais percursos.

## Semântica e limites

Comparações conservam a polaridade original: resultado um significa falha da
regra, escolhido pelo `nextDone` do bytecode. Faixas são inclusivas; ano/dia
lidos são um acima do armazenamento; amizade/afeto e contadores recebem clamps
originais. Índices inválidos não acessam memória e variáveis sem suporte dão
boundary explícito. Uma operação ausente nunca é interpretada como concluída.

Soma/subtração/multiplicação usam wrap explícito em 32 bits sem overflow com
sinal em C; divisão por zero e resto por zero preservam o valor como no switch
original. `INT32_MIN / -1` tem resultado definido, sem trap/dependência MIPS.
O antigo resto por zero da conversa devolvia zero e foi corrigido; a abertura
passa a usar o mesmo algoritmo, eliminando a outra regra de overflow. Escrita de
flags respeita o argumento original `u16`, sem copiar seu armazenamento em bits.

A tabela inteira de variáveis ainda não está portada. Consultas de família/clima
conservam os valores iniciais já presentes no recorte anterior; este evento não
as usa. Escritas de sistemas ausentes continuam recusadas. Comandos especiais,
gatilhos com condições adicionais e casos por ID ainda existem. Esta entrega
não conclui todos os eventos, famílias de comandos ou clima, nem introduz
novos limites de conteúdo, rede ou infraestrutura de mods.

## Testes e reprodução

`native_event_variables`, sem SDL e sem dados instalados: operações aritméticas,
comparações, extremos, divisão/resto, flags, masks, clamp, calendário 0/1-based,
spans de slots/índices inválidos, campos compartilhados entre adapters, identidade
ativa e desvinculação. Um roteiro sintético pelo tick real da sessão verifica
que mensagens pendentes não repetem efeitos; concluir aplica o resultado uma
vez; falha/cancelamento de serviço interrompem com erro, conservando o cursor e
sem executar a progressão posterior. Nenhuma falha vira sucesso silencioso.

`native_vineyard_invitation_playthrough`, com `data/`: gatilho instalado e suas
fronteiras, aceitar/recusar/amizade baixa, palavras exatas de 1:418..421 após
expansão, ANI e caminhada reais, flags, ausência de mudanças em inventário/saldo,
liberação da identidade, controle e persistência entre áreas. Não força
`animation_done`, não injeta uma resposta no slot nem troca o script original.
Datas e amizade inicial são fixtures, não comandos novos de runtime.

```sh
./build/native-debug/port/hmbtn_native --data data --area-preview 0 --preview-date 2:14 --preview-time 06:00
```

Sessão nova no ano 1; o evento inicia pela tabela original. Z avança/confirma;
seta para baixo escolhe a segunda resposta. X também avança as falas conforme
o mapeamento atual. Sair do processo perde o progresso: saves ficam em PF-05.

Configuração e build `native-debug` aprovados, sem avisos no build. Suíte
**86/86 CTests aprovada**, zero falhas, **52,10 s**: 56 independentes dos dados
opcionais e 30 percursos com recursos locais. Inclui regressões de chegada/
infância, Thomas, funeral, torta, ingredientes, lojas, cultivo e serviços.
Capturas do convite/escolha foram inspecionadas com SDL dummy/software e ficaram
em `/tmp/`, sem incluir imagens ou recursos no Git. Windows/cross-build, Release,
Android, sanitizers, instalação limpa e sessão manual com janela/áudio reais
não foram executados. Resultado Linux não declara validação Windows.

## Arquivos desta entrega

- Novos: `port/game/event_variables.c/.h`,
  `port/tests/event_variables_test.c`, `vineyard_invitation_playthrough.c` e
  este relatório.
- Adaptados: `port/game/farm_intro.c/.h`, `farm_visit.c`, `village_talk.c`,
  `session.c`, `port/graphics/scene_preview.c` e `port/CMakeLists.txt`.
- Documentos: roadmap, status, arquitetura, matriz e estratégia.

`src/`, `sdk/`, `disc/`, `tools/`, CMake PS1, infraestrutura SDL/áudio/instalação
pelo BIN e alterações preexistentes foram preservados. PF-04 é a próxima entrega;
não foi iniciada aqui.
