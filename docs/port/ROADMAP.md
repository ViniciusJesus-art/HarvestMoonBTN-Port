# Roadmap do porte nativo — Linux e Windows

Atualizado em 2026-10-28. Android é futuro. **Este arquivo é a fonte principal
para selecionar, ordenar e concluir as próximas tarefas.** O produto continua
parcial; o bootstrap `--diagnostics` não executa o jogo e o runtime ainda não
executa o jogo completo.

## Direção e navegação

- [Estratégia permanente](PORTING_STRATEGY.md): objetivo, reaproveitamento de C,
  arquitetura desejada, propriedade de estado e critérios de adaptação.
- [Arquitetura atual](ARCHITECTURE.md): o que o build e o runtime fazem hoje.
- [Matriz de funções](PORTING_MATRIX.md): origem, implementação nativa,
  dependências, estratégia e testes por sistema.
- [Status](ROADMAP_STATUS.md): resumo e limites de validação.
- [Pendências complementares](ROADMAP_PENDENTE_CODEX.md): classificação do backlog
  anterior e entregas posteriores à migração, subordinadas à ordem deste arquivo.
- [Concluídos anteriores](ROADMAP_CONCLUIDOS.md) e
  [histórico](ROADMAP_HISTORICO.md): registros preservados, não filas concorrentes.

Priorizar lógica C original: compilar diretamente quando viável, adaptar em
`port/` quando necessário e reimplementar apenas com impedimento registrado.
`src/` não será modificado. Preservar infraestrutura nativa e instalação pelo
BIN do usuário. Compatibilidade MIPS/binária, compilação PS1 e SHA-1 do original
não são critérios de entrega PC. O legado não será removido nesta fase.

## Direção revisada: port funcional e extensível

O produto é um jogo nativo para Linux/Windows, com Android futuramente. O C
original acelera a implementação: aproveitar funções prontas, adaptar suas
dependências e escrever código novo apenas onde necessário. Não reconstruir a
máquina PS1 nem transportar seus limites de memória, overlays, ABI ou quantidades
fixas como restrições permanentes do produto.

O conteúdo original define o comportamento de referência. Quantidades de plantas,
itens, áreas e eventos devem poder crescer; índices e limites do BIN pertencem ao
leitor/adaptador do formato original. Novos conteúdos devem usar as mesmas regras
e o mesmo estado. Coop é uma possibilidade futura: comandos e estado sem efeitos
de renderização ajudam, mas rede, lockstep, rollback e determinismo global não
são requisitos desta fase.

**PF-01..05 concluídas. PF-06 em andamento: batata, grama/feno, culturas base de verão/outono e
clima diário/chuva entregues; sementes sazonais de Won, desbloqueios do supermercado, flores simples e variantes de Magic Red entregues; Moondrop com suas visitas também entregue; carpintaria/galinheiro e primeira casa entregues na cadeia da estufa; obra/mapas/cochos do celeiro também entregues; aquisição/presença/alimentação de vacas e ovelhas também entregues no recorte base do celeiro; segunda casa e lareira de inverno entregues; estufa de cultivo e Orangecup entregues; próximo recorte, aquisição de ferramentas animais e produção de leite/lã. Produção, reprodução e ciclo animal completo continuam pendentes. Geladeira, armário e núcleo da cozinha funcionais; aquisição de utensílios e apresentação completa da cozinha pendentes.** Nabo e batata têm percurso completo de compra de
sementes até venda, com saves preservando o crescimento. A PF-04 separou
atualização e desenho nos fluxos existentes, inclusive título/cadastro. O reaproveitamento
é feito por funções adaptadas em `port/`, sem compilar módulos `src/` diretamente.

## O que muda no planejamento

MA-01..06 permanecem concluídas e aproveitadas. Elas centralizaram dados e execução,
definiram serviços e extraíram o tick livre; não adicionaram compilação direta de
módulos C de `src/`. Não repetir nem desfazer esse trabalho.

MA-07..25 foram **substituídas como fila**, sem serem marcadas como concluídas.
O [plano MA anterior](ROADMAP_MA_LEGADO.md) preserva seus detalhes. Ele misturava
refatorações e funcionalidades em uma cadeia longa; isso não demonstra que o
código atual precise ser refeito ou que existam 25 bloqueios para portar o jogo.
A base funciona parcialmente e tem testes; há acoplamentos reais em
`graphics/scene_preview.c`, regras distribuídas e sistemas incompletos. Resolver
cada impedimento quando ele afetar uma entrega, sem exigir reorganizar todo o
projeto antes de portar a próxima função.

A PF-04 consolidou a separação de atualização e desenho no recorte jogável
atual. Novos sistemas devem usar o tick público e manter regras/transações no
domínio. Registrar qualquer adaptador provisório com o responsável pela retirada;
não ampliar regras de jogo em código de desenho. Essa separação não conclui os
sistemas de conteúdo ainda ausentes.

## Fila funcional prioritária

Os IDs PF identificam entregas verificáveis, não uma promessa de poucas alterações
ou de jogo completo após sete tarefas. Cada entrega pode exigir vários recortes
internos. Executar a tarefa inteira quando viável; dependências abaixo são as
reais do domínio, não uma obrigação de concluir todas as linhas anteriores.

| Entrega | Dependências reais | Trabalho e critério de conclusão | Validação |
|---|---|---|---|
| **PF-01 — Inventário e dinheiro: concluída** | Estado MA-01/02 existente; verificar dependências das funções selecionadas | Adaptar `event_items_take`, `event_item_store` e `estate_gold_change` em `port/`, reconciliando os algoritmos nativos já equivalentes em vez de duplicá-los. Isolar resolução de operandos de script, mensagens e animações das operações de domínio. Menus, coleta, eventos e lojas cobertos usam as mesmas operações. Registrar origem e diferenças; remover somente os caminhos efetivamente substituídos. Capacidade é consultada no estado e a adaptação não cria novos limites PS1. Não exige generalizar todo o interpretador. | Inventário cheio/vazio, ordem dos slots, remoção/armazenamento, cancelamento, sementes/baú, débito insuficiente, teto configurado, ledger e aplicação única; percursos de inventário, lojas, Greg, presentes e ingredientes; suíte nativa. |
| **PF-02 — Primeiro cultivo completo e passagem dos dias: concluída** | PF-01 para receber/vender produtos; relógio/campo atuais | Adaptar os recortes necessários de `field_daily_tick`, `clock_advance_minute` e coordenação de `clock_events.c`. Integrar semear → regar → dormir → crescer → colher → vender com uma cultura original. Extrair sono/pernoite da cena no escopo necessário. Preservar alimentação, ovos, flags e pagamentos existentes, com ordem explícita e execução única. Definição da cultura separada do estado de cada célula, sem novo teto fixo do catálogo original. Demais culturas/clima não são declarados completos. | Ciclo completo com recursos instalados; dias sem água, estação inválida/virada, regrowth quando aplicável, sono/múltiplos dias, pausas, ovos/ração e remessa sem repetição. |
| **PF-03 — Eventos reutilizáveis para ampliar a progressão: concluída** | Runtime/serviços atuais; operações PF-01 apenas para comandos que as usam; PF-02 somente para eventos dependentes de cultivos/dias | Evoluir o interpretador existente a partir de `event_script.c`/`event_commands.c`; compartilhar variáveis, flags, aritmética, mensagens, atores e gatilhos conforme os roteiros selecionados exigirem. Escolher e registrar um evento original ainda não suportado antes de editar, incluindo todos os seus comandos/dependências. Entregá-lo jogável, retirar duplicações dos comandos migrados e proteger abertura/visitas/conversas existentes. Erro/ausência nunca é sucesso. Novos eventos usam esse mesmo runtime. | Vetores originais dos comandos selecionados, operandos inválidos, espera/retorno/desvio, efeito único, cancelamento/erro; novo percurso e regressões de infância, Thomas, funeral, torta e ingredientes. |
| **PF-04 — Atualização independente da apresentação: concluída** | Sessão/serviços/exploração MA-04..06; coordenadores vigentes no momento da entrega | Consolidar tick público de roteiros, portas, viagens e sono; `app.c` agenda update e draw separadamente, inclusive título/cadastro. Cena guarda recursos/caches/desenho; transações pertencem ao gameplay. Retirar os adapters provisórios de coordenação usados em PF-02/03 e migrar percursos de teste para APIs públicas. Pode ser antecipada se um acoplamento bloquear outra entrega. | Mesmo estado com mesmos ticks e 0/30/60 apresentações; desenho sem mutação; transições/sono/cadastro e falhas de recursos; suíte completa. |
| **PF-05 — Saves nativos e retomada: concluída** | Proprietários de estado atuais identificados; definir quais estados transitórios são salváveis | Formato versionado explícito, sem dump de structs/ponteiros/layout PS1. Salvar e restaurar todo o recorte jogável declarado, incluindo conteúdo referenciado por IDs estáveis. Versão explícita; durante desenvolvimento não há exigência de compatibilidade com saves antigos. Manter rejeição clara e integridade da versão atual. Não depende de terminar todas as espécies/eventos. | Ida/volta, dois saves/sessões, reinício do processo, inventário/dinheiro/data/campo/animais/flags, arquivos truncados/corrompidos, versões e retomada sem efeitos repetidos. |
| **PF-06 — Completar sistemas do jogo original: em andamento** | Por sistema: estado, operações, comandos e ciclo diário efetivamente usados | Fila incremental: batata, grama/feno, culturas base de verão/outono e clima diário/chuva entregues; sementes sazonais de Won, desbloqueios do supermercado, flores simples e variantes de Magic Red entregues; Moondrop e visitas de Karen entregues; carpintaria/galinheiro e primeira casa entregues; obra e cochos do celeiro entregues, aquisição/capacidade/alimentação bovina/ovina entregues, segunda casa/lareira, estufa de cultivo e Orangecup entregues; ferramentas animais e leite/lã a seguir; produção/ciclo animal completo pendentes; aquisição de utensílios/cozinha completa pendentes; demais culturas/clima completo, espécies, lojas/ferramentas, moradores/relacionamentos, família, áreas, festivais/minijogos e progressão. Adaptar funções já presentes no original e manter os backends. Antes de cada implementação, publicar aqui o sistema selecionado, fontes, lacunas e percurso de aceite. Galinhas, rotas, afinidade e pesca atuais são reconciliadas no domínio necessário; não recomeçar tudo. | Percurso completo e testes de regras de cada sistema; matriz atualizada distingue completo, parcial e ausente. PF-06 só termina com cobertura declarada de toda a progressão original, não por um exemplo isolado. |
| **PF-07 — Distribuição Linux/Windows e consolidação** | Recorte publicável e instalação BIN existentes; validação de plataformas começa em cada entrega | Configurar/compilar/testar Linux e Windows durante o desenvolvimento. No marco final, validar instalação limpa, saves, atualização, janela/entrada/áudio e progressão completa; registrar diferenças deliberadas. Preservar BIN do usuário e distribuição sem recursos proprietários. Android, conteúdo novo e coop ganham planejamento próprio posteriormente. | Builds/testes reais das plataformas disponíveis, instalação limpa e percurso manual documentado; não presumir Windows pela execução Linux nem concluir port completo apenas pelos CTests. |

### PF-03 concluída — evento 140, convite de Duke para a vindima

Seleção registrada em 2026-10-26 antes de alterar lógica. Roteiro original 140,
18 instruções do BIN instalado; gatilho de entrada na fazenda em
`event_triggers_fire`, lido por `farm_visit.c`. Ano 1, outono, dia 14, janela
original de horas/clima e flag de execução única; conferir as condições
empacotadas no teste com a tabela original, sem gatilho inventado.

Fontes: `event_script.c` (dispatch/branches), `event_commands.c`
(`event_cmd_test_slot`, `event_cmd_set_var`, `event_var_get/update/arith/compare`,
reset/posição/face/movimento de atores, mensagens, fade e `event_cmd_enter_mode`),
`event_request.c` (gatilhos), bytecode e mensagens 1:418..421 do BIN.
Comandos usados: 1, 2, 4, 12, 15, 19, 27, 35, 37 e terminador 255.
Dependências novas: ator residente Duke (21) ligado à identidade compartilhada;
variáveis/aritmética/comparações comuns a visitas e conversas. Aceitar define a
flag 205 e executa as duas alterações originais de amizade; recusar segue a
outra fala. Não substituir a sequência por uma regra presumida.

Aceite: convite jogável nos dois ramos, recursos e animação originais, controles
restaurados, estado/flag persistentes entre áreas e execução única. Validar
operações compartilhadas, operandos inválidos, esperas/desvios, serviços atrasados
ou com falha e regressões de infância/Thomas/funeral/torta/ingredientes.
A vindima do dia seguinte e o destino de Cliff são eventos distintos e ficam
em PF-06; não declarar essa cadeia completa ao concluir o convite.

Validação final: configuração/build `native-debug` aprovados, **86/86 CTests**,
zero falhas, **52,10 s**. Novo módulo `event_variables.c` compartilha operações
13..22 entre conversas/visitas e slots da abertura; Duke usa a identidade
residente ligada à sessão, sem segundo interpretador. Relatório, fontes,
limites e reprodução em [PF03_REUSABLE_EVENTS.md](PF03_REUSABLE_EVENTS.md).
Critérios acima atendidos para o convite; a cadeia da vindima não está concluída.

### PF-04 — Entrega validada em 2026-10-26

- [x] `game/runtime.c` coordena o tick público `hmbtn_session_tick`: roteiros,
  conversas, portas/viagens, sono/manhã e fases livres, preservando a ordem atual.
  Callbacks persistentes usam a sessão; empréstimos de recursos valem no tick.
- [x] `core/app.c` agenda atualizações e apresentações separadamente, com entrada
  consumida uma vez. Título/cadastro também possuem tick e draw distintos.
- [x] Draw não avança sessão, poses, RNG ou cadastro. Backends SDL/recursos/áudio
  permanecem no host; animações e conclusão de efeitos avançam sem draw.
- [x] Percursos migrados para APIs públicas; nenhum teste inclui um `.c` de
  produção. Removidos os coordenadores e acumuladores antigos substituídos.
- [x] Comparativos com zero/uma/duas apresentações por tick cobrem Duke sim/não,
  porta/troca de área, sono, campo/remessa, cadastro e handoff para abertura.
  Falha de carga retorna erro sem concluir a viagem ou repetir transações.
- [x] Configuração/build Linux Debug aprovados; **89/89 CTests**, zero falhas,
  **113,53 s**: 57 independentes de dados opcionais e 32 com recursos instalados.
  Após ajuste de constância em uma fixture, build sem avisos e seu percurso de
  vila novamente aprovado (1/1). `src/` e infraestrutura PS1 preservados.

Fontes, interfaces, arquivos, limites e reprodução em
[PF04_UPDATE_PRESENTATION.md](PF04_UPDATE_PRESENTATION.md). Critérios da PF-04
atendidos para os fluxos implementados. Não houve validação Windows/Release,
janela/áudio reais, sanitizadores ou instalação limpa nesta entrega. Conteúdo
incompleto continua em PF-06; saves ainda não estavam implementados naquela entrega (concluídos em PF-05).

### PF-05 — Entrega validada em 2026-10-26

- [x] Política registrada antes da implementação: save entre ticks em exploração
  livre; ações/eventos/menus/transições bloqueiam com motivo. Dormir e salvar
  usa a cama atual e grava após atualizar o dia e concluir o fade da manhã.
- [x] `game/save.c` codifica campos explícitos em seis seções, com versão,
  catálogo/IDs lógicos, tamanhos e CRC-32. Não grava structs/ponteiros/tickets.
  Formato inicial v1; versões desconhecidas são rejeitadas antes da carga.
- [x] Inventário/suprimentos/capacidades, economia/remessa/ledger, calendário,
  cadastro, campo, galinhas/ovos/incubação, cochos/coleta, moradores/rotas/
  relações/flags/contadores, RNGs e estado estável do jogador são retomados.
  Contadores de ferramenta, controles e chave diária também persistem.
- [x] `filesystem/save_file.c` escreve temporário/flush/close/rename, sem truncar
  o slot anterior. Carga usa sessão/cena candidata; recursos e agendas são
  reassociados sem inicialização de novo jogo ou efeitos de nova entrada na área.
- [x] Continue do título, Read diary e dormir/salvar usam as mesmas interfaces.
  Slot padrão nas preferências do usuário; `--save-file` e `--load-save` permitem
  arquivos independentes. Falha não troca a cena nem anuncia gravação.
- [x] Configuração/build Linux Debug aprovados; **92/92 CTests**, zero falhas,
  **123,82 s** (59 independentes de dados opcionais e 33 com recursos locais).
  Testes incluem processos separados, Continue no app real, cama, falhas,
  idempotência de campo/remessa e evento original de Duke sem repetição.
  Após o ajuste final para previews sazonais, save/mapa novamente **4/4**, 14,87 s.

Critérios da PF-05 atendidos no recorte estável declarado; não há saves durante
execução de scripts/ações, importação de memory card ou seleção/transferência de
animais. Windows/cross-build (toolchain ausente), Release, Android, sanitizadores,
instalação limpa e janela/áudio reais não foram executados. Há avisos de constância
preexistentes na fixture `world_map_playthrough.c`, preservada nesta entrega.
Fontes, formato, interfaces, arquivos e reprodução em
[PF05_NATIVE_SAVES.md](PF05_NATIVE_SAVES.md). `src/`/SDK/fluxo PS1/BIN preservados.

### PF-06 — Batata: recorte validado em 2026-10-26

- [x] Conferidos `CROP_POTATO`, itens 0x23/0x42 e funções originais de campo,
  plantio e desenho. O catálogo já traz 3 + 4 dias regados, sem rebrota,
  compra por 150 G e venda por 80 G; não houve reescrita do crescimento.
- [x] `farm_field.c` habilita a batata no mesmo filtro de suporte; todos os
  consumidores usam as regras existentes. `scene_preview.c` seleciona o sprite
  original 8, estágio + 2 na primavera e 0 para o talo no verão.
- [x] Mesmo driver de nabo/batata testa mesa central, lista/descrição/quantidade,
  compra de três pacotes, plantar/regar/dormir, uma noite seca, maturidade,
  colher e receber 80 G uma única vez, preservando os pacotes restantes.
- [x] Save no estágio 1 com um dia regado acumulado; destruição da sessão/recursos
  e carga nativa sem perda de estado, seguida da conclusão do cultivo. Outro
  processo abre o mesmo save. Formato v1 preservado, sem novo estado paralelo.
- [x] Regras de estação/inverno, células inválidas/inativas e grade/consumo único
  cobertas; vetores de nabo e rebrota de pepino preservados.
- [x] Configuração/build Debug aprovados; **93/93 CTests**, zero falhas,
  **124,82 s** (59 independentes dos dados opcionais e 34 com recursos locais).
  Quatro direcionados: **4/4**, 1,43 s. Quantidade e planta madura inspecionadas
  nas capturas SDL dummy/software, mantidas somente em `/tmp`.

Critérios do recorte da batata atendidos. **PF-06 continua aberta.** Windows,
Release, Android, sanitizadores, instalação limpa e janela/áudio reais não foram
executados. Original/SDK/build PS1/BIN e mudanças anteriores preservados.
Fontes, arquivos e reprodução em [PF06_POTATO.md](PF06_POTATO.md).

### PF-06 — Grama e feno: recorte validado em 2026-10-26

- [x] Comprar 0x36 na mesa, plantar pela ANI/grade atual e consumir um pacote.
- [x] Crescer sem rega conforme `CROP_GRASS` (3 + 4 + 4 dias), usando o campo e o observer diário existentes.
- [x] Cortar apenas madura: produzir uma unidade em `inventory.supplies[0]`, sem duplicação; respeitar capacidade inicial 999 e rebrota/skip-day originais.
- [x] Conservar grama entre verão/outono; reiniciar progresso no inverno e retomá-lo na primavera; proteger regras de ferramentas/colisão.
- [x] Usar terreno/UV/CLUT e efeito sazonal originais, sem acoplar crescimento ao desenho ou manter cache paralelo de formas.
- [x] Retirar/devolver feno e abastecer cocho no celeiro a partir do mesmo estoque.
- [x] Salvar/carregar crescimento e corte, destruir a sessão antiga, retomar rebrota e abrir o save em outro processo.

`src/` intacto; não houve nova engine de campo, estoque, alimentação ou formato
de save. **95/95 CTests**, zero falhas, 124,59 s, Linux Debug com dados locais.
Após ampliação das fixtures sazonais, **9/9 direcionados**, 2,66 s. Capturas
inspecionadas em SDL dummy/software. Windows real, Release, sanitizers, Android,
janela/áudio manual e instalação limpa não foram executados nesta entrega.
Fontes, arquivos, reprodução e limites em [PF06_GRASS.md](PF06_GRASS.md).

Critérios do recorte atendidos; **PF-06 permanece aberta** para a progressão
original ainda ausente. PNG continua adiado.

### PF-06 — Culturas base de verão: recorte validado em 2026-10-26

- [x] Menu base original do grupo B na mesa: chave 0x129/registro 113, grama, tomate, milho e cebola; saudação 2:0x273 e descrições originais.
- [x] Gameplay e UI consultam o mesmo catálogo por estação e o relógio autoritativo; não comprar no balcão ou nos conjuntos de outra estação.
- [x] Habilitar 0x27..0x29 no motor de campo/catálogo atual, sem outro algoritmo de crescimento, inventário ou economia.
- [x] Plantio/consumo único por ANI e grade original; noites regadas/secas, estágios, colheita e rebrota/skip-day quando previstos.
- [x] Sprites originais 12/13/14 no verão e 7/8/9 murchos no outono, sem crescimento durante desenho.
- [x] Remessa, contagem e pagamento únicos; cebola verifica adiamento em data sem coleta.
- [x] Save/load intermediário, comparação canônica e outro processo; recomposição dos recursos de área sem depender de texturas deixadas pela primavera.
- [x] Proteger primavera, grama/feno, cochos e saves. Identificar a condição original de abóbora sem liberar conteúdo cuja cadeia ainda está pendente.

**99/99 CTests**, zero falhas, **128,11 s**, Linux Debug com recursos locais;
**10/10 direcionados**, 5,90 s. Capturas inspecionadas em SDL dummy/software;
preview da loja no verão executado. Windows real/cross-build, Release,
sanitizers, Android, janela/áudio manual e instalação limpa não executados.

`src/` e alterações anteriores preservados. Save v1 não mudou. A abóbora depende
de `ESTATE_STATUS_GROUP_B_DONE`/`milestone_check_group_b` (inclui abacaxi), cadeia
não implementada neste recorte. Fontes, arquivos, testes e reprodução em
[PF06_SUMMER_CROPS.md](PF06_SUMMER_CROPS.md).

Critérios do recorte base atendidos; **PF-06 permanece aberta**. PNG continua
adiado; não foi implementado clima, estufa ou progressão de sementes exóticas.

### PF-06 — Culturas base de outono: recorte validado em 2026-10-26

- [x] Grupo C original na mesa: registro 114/chave 0x12A, grama, berinjela, cenoura e batata-doce; saudação 2:0x274 e descrições 0xCF/CB/CC/CD.
- [x] Catálogo sazonal compartilhado, nome completo dentro da moldura e quantidade posterior à escolha; manter presença/horários de Jeff e os demais conjuntos/balcão.
- [x] Habilitar 0x2C..0x2E no campo/catálogo existentes, sem novos proprietários de estado ou simulação.
- [x] Plantio/consumo únicos por ANI/grade; noites regadas/secas; colheita; rebrota de berinjela no estágio 2 e batata-doce no estágio 1, com skip-day original.
- [x] Arte original 12/13/14 no outono; remover cultivos ao entrar no inverno, sem sprite de planta na neve.
- [x] Remessas/pagamento únicos; cenoura verifica adiamento em Outono 9.
- [x] Save/load no crescimento e rebrota, inclusive progresso não zero, com comparação canônica e outro processo.
- [x] Dormir de Outono 30 para Inverno 1, retornar à fazenda, conferir limpeza e carregar save de inverno na sessão e em outro processo.
- [x] Proteger primavera/verão, grama, alimentação, eventos e saves; identificar condição do espinafre sem habilitar sua cadeia incompleta.

Após corrigir a moldura da lista, **103/103 CTests**, zero falhas, **134,40 s**,
Linux Debug com dados locais; **14/14 direcionados**, 10,68 s. Menu/quantidade,
maturidade/rebrota e inverno inspecionados em capturas SDL dummy/software;
preview de outono executado. Windows/cross-build, Release, sanitizers, Android,
janela/áudio manual e instalação limpa não executados.

`src/` e alterações anteriores preservados; save v1 e instalação pelo BIN não
mudaram. Espinafre depende do grupo C, incluindo pimentão; esse desbloqueio
permanece pendente. Relatório, arquivos e reprodução em
[PF06_FALL_CROPS.md](PF06_FALL_CROPS.md). **PF-06 continua aberta.**

### PF-06 — Clima diário e chuva no campo: recorte concluído

- [x] `HmbtnGameState.weather` possui clima atual, previsão e RNG persistente;
  casa, TV, ações, pesca, galinhas, gatilhos e agendas/scripts leem esse domínio.
  As cinco flags meteorológicas são uma projeção para os roteiros.
- [x] Adaptação de `weather_roll_tomorrow`, `weather_step_today` e calendário:
  sorteios sazonais, quatorze datas especiais, aniversários das cinco candidatas
  (incluindo alternativos), véspera de eventos e viradas de estação/ano.
- [x] Observer existente: crescimento com a rega anterior → produção/limpeza →
  troca/previsão → irrigação da chuva. Uma passagem por data, no sono, tick e
  fast-forward. Campo não ganha outro motor; enxada em dia chuvoso deixa terra regada.
- [x] Regador/chuva usam a mesma elegibilidade; preservam grama estabelecida,
  células inativas e inverno. Sol/neve/tempestade/nevasca não chamam a irrigação.
- [x] Tonalidade e partículas com os sprites originais 25/26; atualização cosmética
  no tick, congelada nos menus/interiores. Draw não sorteia nem altera gameplay.
- [x] Save v2 grava RNG; v1 é migrado preservando clima/previsão e iniciando a
  sequência ausente com seed 1, sem novo sorteio na carga. Saves v1 reais de
  outono também carregados no aplicativo. BIN e backends mantidos.
- [x] Linux Debug: configuração/build aprovados; **105/105 CTests**, zero
  falhas, **134,14 s** (63 sem recursos opcionais + 42 com dados).
  Testes de regras/sono/TV/terreno/save/load/viagem/desenho e cinco apresentações;
  capturas inspecionadas. `--preview-weather ATUAL:AMANHA` permite teste dirigido.

Limites: casamento ainda não tem domínio ativo; o algoritmo aceita contexto de
casamento, mas não simula esse sistema. Danos de tempestade/nevasca, estufa,
trovões/tremor de tempestades e integração meteorológica completa de música e
falas permanecem abertos. Os cinco valores são representados; isso não conclui
clima integral nem **PF-06**, que continua aberta. Fontes, arquivos, testes,
diferenças e reprodução em [PF06_WEATHER.md](PF06_WEATHER.md).

### PF-06 — Sementes sazonais de Won e correção da chuva: recorte concluído

- [x] Conversa original 400, comando 81/dispatch 8: inicia grupo D (10) e espera
  a loja terminar. A sessão empresta a loja existente; sem outro vendedor/engine.
- [x] Repolho (500 G/14 dias/250 G), abacaxi (1000 G/20 dias/500 G; rebrota 5)
  e pimentão (150 G/7 dias/40 G; rebrota 2) reutilizam campo/ações/inventário/
  economia/remessas. Sprites sazonais originais 11/16/16 e carga em outro processo.
- [x] Falas/descrições/recibos de Won usam os registros próprios do original;
  menu com nomes, quantidade, cancelamento, falta de fundos/espaço e envio ao
  baú usam a máquina compartilhada. Save v2 existente, sem mudar formato.
- [x] Oito entradas de `shop_seed_menu_setup_d` extraídas/auditadas. O catálogo
  jogável deste recorte contém somente as três culturas acima; flores, condição
  da moondrop (`ESTATE_STATUS_BIT23`) e orangecup/estufa permanecem pendentes.
  Não declarar o estoque inteiro de Won concluído.
- [x] Chuva corrigida conforme `weather.c`, `player_update_position` e
  `main_draw_ots`: paleta azul original recebe a luz depois dos sprites;
  partículas visíveis recebem deslocamento projetado da câmera no tick.
  Nenhuma mudança de paleta artificial ou movimento durante draw.
- [x] Configuração/build Linux Debug; **109/109 CTests**, zero falhas,
  **144,24 s** (64 sem dados opcionais + 45 com dados instalados).
  Pixel da chuva comparado à paleta/iluminação original; movimento real e draw
  sem mutação; três percursos de compra/cultivo/remessa/save/load de Won;
  cancelamento, capacidade/fundos e mensagens próprios protegidos.

Tradução atual é mista: falas/descrições vêm dos arquivos extraídos
`02_messege0.bin`..`06_messege4.bin`; nomes e rótulos do menu são gerados de
`src/main/text/text_bank.c`, não lidos automaticamente do BIN traduzido.
Relatório, arquivos e reprodução em [PF06_WON_SEEDS.md](PF06_WON_SEEDS.md).
PF-06 continua aberta. Windows, Release, sanitizers, janela/áudio manual e
instalação limpa não validados; backends/BIN e alterações anteriores preservados.

### PF-06 — Correção de áudio da chuva e portas: recorte concluído

- [x] Seleção meteorológica original na exploração, manhã, viagens e carga:
  faixa 22 substitui música normal por ambiência da chuva; banco de efeitos preservado.
- [x] Porta tem prioridade sobre arremesso com item na mão, inclusive recusa;
  inventário/poses preservados na passagem. Manter o recuo e tempos do usuário.
- [x] Células originais das rotas acionam portas comuns: abrir, esperar,
  cruzar e fechar com som; folha dupla, pausas, catch-up e retomada protegidos.
- [x] Testes de regras e recursos reais para PCM, transporte, recusa,
  portas de NPCs, persistência e desenho sem avanço. Teste antigo de interiores
  passa a verificar o recuo atual, em lugar da imobilidade anterior.

Detalhes, origem, arquivos e reprodução em [PF06_RAIN_DOORS.md](PF06_RAIN_DOORS.md).
Save v2 permanece compatível; animação transitória de porta de NPC pode reiniciar
na carga, preservando posição/cursor de rota. Não conclui eventos especiais de
portas nem toda a meteorologia. A próxima entrega continua abaixo.

### PF-06 — Câmera, oclusão e desbloqueios do supermercado: recorte concluído

- [x] Aplicar limites da câmera também durante a entrada em portas, preservando
  o recuo e tempos do usuário; validar entrada real além do limite da área 40.
- [x] Item carregado é anexo do jogador na ordenação do mundo: acima do corpo,
  encoberto por cenário/atores à frente. Reutiliza cache/ANI existentes.
- [x] Adaptar grupos A/B/C e limiar original de 101 unidades de cada um dos
  quatro produtos, consultando remessas únicas; nenhuma flag/contagem paralela.
- [x] Publicar morango/abóbora/espinafre na mesa e loja existentes; habilitar
  definições/sprites no motor comum, com compra/cancelamento/baú, crescimento,
  colheita, remessa e rebrota prevista pelas tabelas originais.
- [x] Save v2 preservado; limites 100/101 por produto, independência dos grupos,
  referências após carga e retomada das três culturas em outro processo.
- [x] Configuração/build Linux; seleção final **20/20 CTests**, zero falhas,
  **17,85 s**, e execução direta adicional do caso de câmera no limite.
  Respeitar máximo de 50 execuções por rodada; suíte completa não rodada.

Fontes, diferenças, arquivos e reprodução em
[PF06_CAMERA_SEED_UNLOCKS.md](PF06_CAMERA_SEED_UNLOCKS.md). PF-06 permanece aberta.

### PF-06 — Pink Cat e Toy Flower: recorte concluído em 2026-10-27

- [x] Ampliar o grupo D existente para cinco entradas, filtrando suporte sem
  perder os índices fonte: Pink Cat é a linha 4, Toy Flower é a linha 6.
  Nomes e descrições próprios; mesma conversa, quantidade, pagamento e baú.
- [x] Reutilizar catálogo/campo/ações/diário/inventário existentes: Pink Cat
  custa 200 G e cresce em seis dias regados no verão; Toy Flower custa 500 G
  e cresce em doze dias regados na primavera. Sem rebrota ou renda de remessa.
- [x] Resolver sprites sazonais originais e maturidade pela faixa zero da ANI,
  como já feito para repolho, sem permitir leitura além dos limites do recurso.
- [x] Percursos reais: compra, noite seca, plantio, rega/sono, coleta única,
  mochila/mão, presente para Lillia pela conversa original e mudança de estação.
  Testar fundos/cancelamento/baú/lotação e save/load v2 em outro processo.
- [x] Configuração/build Linux Debug; 18 alvos direcionados validados. A seleção
  de 18 teve 17 aprovações e uma expectativa antiga de morango corrigida;
  `native_crop_daily` passou na repetição isolada. Mais duas execuções diretas
  com capturas. Total conservador: 44 processos, incluindo wrappers e filhos,
  abaixo do limite 50; suíte completa de 118 testes não executada.

Fontes, diferenças, arquivos, reprodução e limites em [PF06_FLOWERS.md](PF06_FLOWERS.md).
**PF-06 permanece aberta.** Naquela entrega, Moondrop, Magic Red e Orangecup ainda não estavam no menu; Magic Red foi integrada abaixo.

### PF-06 — Variantes de Magic Red: recorte concluído em 2026-10-27

- [x] Adaptar `field_event_dispatch`: um `rand_range(25)` por célula aceita;
  resultado zero grava `0x179` (vermelha), demais gravam `0x178` (azul).
  Mesma grade/ação; células inválidas e espera da animação não consomem RNG.
- [x] `HmbtnFarmField.plant_random` é o único dono da sequência de plantio,
  reutilizando a aritmética original já adaptada. Sem RNG por frame/desenho,
  sem tocar nas sequências de moradores, clima, pesca ou galinhas.
- [x] Save nativo v3 grava a sequência; lê v1/v2 com seed inicial 1, preservando
  células, economia e clima. Saves anteriores não tinham sequência de plantio.
- [x] Won oferece seis entradas: Magic Red ocupa a linha visível 4, fonte 5,
  descrição `0x16a`, 600 G. Toy Flower passa à linha visível 5, fonte 6.
  Mesma compra, quantidade, inventário/baú e cancelamento, sem estoque paralelo.
- [x] Ambas crescem no outono em dez manhãs regadas (3+3+2+2), sem rebrota.
  Sprites originais 17/25, faixa zero madura segura. Azul sem renda de remessa;
  vermelha rende 200 G sem um contador de remessas específico no original.
- [x] Testes de regras e dois percursos reais com variantes controladas:
  compra, save antes/depois do plantio e durante crescimento, noites secas,
  colheita, azul na mochila/presente para Lillia, vermelha enviada/paga uma vez,
  inverno e retomada pelo aplicativo em outro processo. Cinco culturas anteriores
  de Won continuam cobertas. Capturas das variantes e do menu inspecionadas.
- [x] Configuração/build Linux Debug; seleção final 18/18 CTests, 7,25 s.
  Os dois novos percursos passaram também em rodada anterior e execução direta
  final com capturas. Foram 20 alvos distintos validados; 44 processos contando
  repetições/wrappers/filhos, dentro do limite 50. Suíte completa de 121 não rodada.

Fontes, esquema, arquivos e reprodução em [PF06_MAGIC_RED.md](PF06_MAGIC_RED.md).
PF-06 permanece aberta. Windows, Release, sanitizers, janela/áudio físicos e
instalação limpa não foram verificados.

### PF-06 — Moondrop e visitas de Karen: recorte concluído em 2026-10-27

- [x] Rastrear os blocos instalados 18/19 e o comando 81/27. A visita 18 é
  dependência necessária: fornece o primeiro pacote; a 19 exige uma Moondrop
  madura e libera Won na chegada, mesmo quando o jogador não conversa depois.
- [x] Adaptar gatilhos originais: papel de solteiro da família atual, afeição de
  Karen (15.000 para o presente), espaço acessível para ferramentas, entrada
  saindo da casa, janela da manhã e condição 7 sobre valor/estágio do campo.
  Visita 18: primavera 1..20, 06:00..11:59; não substituir afeição por amizade.
- [x] Executar os roteiros reais pelo interpretador existente: movimento,
  espera/conversa, falas 1:86/87/88, música 5, publicação da arte do pacote e
  armazenamento pelo inventário comum. Ramo conversado adiciona 2.000 de afeição;
  espera sem conversa não concede esse bônus nem o pacote da visita 18.
- [x] `HmbtnGameState.progress.moondrop_seeds` é a fonte única do desbloqueio,
  distinta dos flags de eventos 269/270. Loja e casa emprestam esse estado;
  comando comum atende visitas/conversas, com rollback de conversa preservado.
- [x] Save v4 acrescenta a progressão; lê v1/v2/v3 preservando seus campos e
  iniciando o desbloqueio em falso. Construtores restauram referências para a
  nova sessão. Persistência antes/depois das visitas, cultivo e compra validada.
- [x] Semente `0x31`, 300 G, primavera, seis manhãs regadas, produto `0x51`,
  sem rebrota ou renda de remessa. Mesmo motor/ações/inventário/presentes;
  sprites 12/40 e faixa madura zero segura. Won conserva seis linhas antes
  e publica sete após o evento, com índices fonte/descrições corretos.
- [x] Percurso real desde o presente até compra com Won e presente para Lillia;
  ambos os ramos de espera, textos/arte, coleta, mochila, saves e retomada em
  outro processo. Regras cobrem noite seca, estação/inverno e menu bloqueado.
- [x] Configuração/build Linux Debug; seleção de **20/20 CTests**, **22,41 s**.
  Após corrigir um modo musical antigo retido entre visitas, apenas o percurso
  afetado foi repetido: **1/1**, **2,50 s**, com verificação da música 5.
  **48 processos executados**, mais uma tentativa antes de o binário estar
  disponível, dentro do máximo 50. Suíte completa de **122** não executada.

Fontes, campos, arquivos, migração, testes e reprodução em
[PF06_MOONDROP.md](PF06_MOONDROP.md). PF-06 permanece aberta; estufa, clima
integral, construções e demais sistemas não são concluídos por esta entrega.

### PF-06 — Carpintaria e primeira ampliação: concluída no recorte

Seleção em 2026-10-27: a auditoria da aquisição da estufa encontrou uma sequência
real de cinco obras em `shop_first_clear_flag`/`shop_silo_offers`. A primeira
obra aproveitável completa foi o galinheiro; a primeira casa foi integrada na entrega seguinte, abaixo.
Não há exigência técnica de refazer toda a arquitetura ou emular PS1.

- [x] Gotz (24), conversa original 374: dispatch 81/8/9 abre o modal compartilhado;
  o script e a agenda original decidem elegibilidade, inclusive mãos ocupadas.
- [x] Madeira: nome/falas originais, descrição, quantidade 1..99, 50 G/unidade,
  cancelamento, fundos/estoque, depósito no mesmo inventário e ledger comum.
- [x] Galinheiro: 5.000 G + 420 madeiras, confirmação original, recusa atômica,
  obra única, resposta enquanto ocupada e quatro manhãs sem repetição.
- [x] `HmbtnSession.construction` guarda apenas prazo; `poultry.upgraded` é o único
  nível. Flags 0x38/0x3A são consultas derivadas para scripts/rotas/gatilhos.
  Status deriva os 20 pontos da melhoria, sem cópia serializada do nível.
- [x] Fazenda/galinheiro usam recursos originais 4..7/116..119, mantendo IDs
  lógicos 0/112; dez vagas/cochos e segunda incubadora pelos sistemas existentes.
- [x] Save v5 preserva obra pendente, melhoria e materiais; restauração e outro
  processo validados. Compatibilidade com v1..v4 foi retirada por decisão do
  usuário; arquivos anteriores são recusados claramente, sem conversão.
- [x] Regras e percurso com recursos instalados: compra/pedido/ocupação, quatro
  noites, save/load durante a obra, mapas/cocho 10/incubadora 2 e retomada do app.

Origem/adaptações, testes e limites em [PF06_CARPENTRY.md](PF06_CARPENTRY.md).
Não são concluídos a cadeia inteira, a estufa, Orangecup nem a PF-06.

### PF-06 — Primeira ampliação da casa: concluída no recorte

Entrega de 2026-10-27, com galinheiro como dependência já satisfeita.

- [x] Gotz/conversa 374 oferece casa por 4.700 G/370 madeiras; descrição,
  confirmação, cancelamento, fundos/materiais e ocupação no modal comum.
- [x] `construction` possui nível da casa, tipo pendente e prazo; não duplica
  `poultry.upgraded`, inventário ou economia. Quatro manhãs e flags 0x38/0x3A/0x3B
  derivados; comando 95/status consultam os mesmos donos.
- [x] Recursos reais 8..11/84..87 por estação, mantendo IDs lógicos 0/80.
  Entradas, despertar, cama/diário/sono, TV, calendário e baú no novo interior.
- [x] Geladeira/armário em `game.inventory`, com navegação original, classes,
  pilhas, mão/mochila, scripts e save; sem estoques paralelos no menu.
- [x] Núcleo da cozinha adaptado de `menu/house.c`, com definições ordenadas
  extraídas do original: seleção, consumo atômico, prato/fiasco, receitas aprendidas,
  consulta/repetição/variação. Receita e quantidade aprendida sem cópia de contador.
- [x] Save v6 atual preserva obra, nível, depósitos, utensílios e receitas;
  roundtrip e retomada pelo aplicativo em outro processo. Não migra v1..v5.
- [x] Linux Debug: configure/build aprovados; seleção final **23/23 CTests,
  18,24 s**. Total de **48 processos de teste** incluindo filhos/tentativas,
  dentro do limite de 50. Windows/Release/sanitizers e suíte inteira não executados.

Fonte, arquivos, percurso reproduzível e limites em
[PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md).
**A cozinha completa não está concluída:** aquisição/entrega de utensílios pela TV,
indicadores completos da ficha e som/animação do preparo continuam na fila PF-06.
Não conceder utensílios gratuitos nem declarar todos os pratos alcançáveis apenas
por extrair as tabelas. Estufa/Orangecup e PF-06 permanecem abertas.

### PF-06 — Obra e cochos do celeiro: concluídos no recorte

Auditoria em 2026-10-27 confirmou cochos/estoque nativos, mas ausência de domínio
bovino/ovino. A capacidade original de animais depende de `livestock_find_free`;
não é declarada jogável somente por trocar o mapa.

- [x] Gotz/conversa 374 oferece a terceira obra por 6.800 G/500 madeiras,
  com descrição 0x136, oferta 0x13E e confirmação 0x5F; mesmas loja/economia/estoque.
- [x] Nível único `construction.barn_level`, tipo pendente `HMBTN_BUILD_BARN`,
  quatro manhãs idempotentes e save/load no prazo. Flag 0x3C, comando 95 e
  status derivados; sem copiar nível em UI/vetor de flags.
- [x] Fazenda 12..15 e celeiro 100..103 mantendo IDs lógicos 0/96; portas,
  texturas, colisões e zonas originais, sem perder campo/depósitos.
- [x] Cochos comuns 10→20, maternidade 1→2 conforme zonas reais. Dez novos
  cochos abastecidos com feno do estoque comum; arte, retirada/devolução,
  portas com item na mão e limpeza diária usando sistemas existentes.
- [x] Save v7 atual, sem migração; valida coerência de níveis/obras e preserva
  depósitos, receitas e cochos. Segunda casa/estufa continuam ocultas.
- [x] Configure/build Linux Debug aprovados. Seleções **8/8 + 20/20 CTests**;
  percurso do celeiro repetido após ajuste apenas de captura, **1/1**. Total
  **49 processos de teste**, incluindo wrappers/filhos/tentativas, abaixo de 50.
- [x] Dependência de compra/presença/capacidade 10/20 e alimentação integrada na
  entrega seguinte, abaixo. Na entrega isolada da obra ainda não havia registros
  bovinos/ovinos; ciclo animal completo e PF-06 continuam abertos.

Fontes, arquivos, reprodução e limites em
[PF06_BARN_EXPANSION.md](PF06_BARN_EXPANSION.md). Windows, Release, sanitizers,
suíte inteira e sessão manual de janela/áudio não executados.

### PF-06 — Criação no celeiro: aquisição e alimentação concluídas no recorte

A dependência descoberta na entrega anterior foi integrada sem refazer obra,
cochos ou backends. O checklist amplo anterior fica atendido no recorte de
aquisição/presença/capacidade/alimentação abaixo; o ciclo animal completo segue
parcial. As tarefas numeradas anteriores foram executadas neste alcance:

1. Adaptar a coleção autoritativa e `livestock_find_free` (dez/vinte vagas,
   compartilhadas pelas espécies), inicialização e nomeação; nada de registros
   independentes na loja, no status ou na cena.
2. Integrar `shop_barn_step` com Barley, preços/recusas/cancelamento originais,
   débito único, colocação e arte/ANI originais. Usar estoque/economia/editor
   existentes, sem conceder animais gratuitamente em partidas normais.
3. Adaptar atualização/alimentação diária de `livestock.c` antes de limpar os
   cochos, com cuidado/saúde e estágios declarados. Cada ação faltante deve
   permanecer identificada; leite/lã, reprodução e eventos têm aceite próprio.
4. Testar compra/nome/cancelamento/lotação, registros e apresentação reais,
   alimentação consumida uma vez, ampliação com animais presentes, transições
   e saves atuais em outro processo. Encerrar o item do celeiro somente com
   capacidade e alimentação efetivamente usadas. Respeitar o limite de 50.


Entrega de 2026-10-27:

- [x] Coleção única `session.livestock`, capacidade 10/20 compartilhada entre
  espécies, com empréstimo de `construction`; sem nível ou contagem duplicados.
- [x] Compra real com Barley: vaca jovem 6.000 G/ovelha 4.000 G, nomes, descrição,
  cancelamento, fundos/lotação, confirmação e débito únicos no modal/editor atual.
- [x] Colocação, tabelas de frames e arte/ANI extraídas do original; escala da
  vaca jovem e desenho com oclusão. Tick controla tempo/movimento, draw é leitura.
- [x] Alimentação no observador diário antes da limpeza, vacas antes de ovelhas;
  crescimento/idade/doença/afeição, conversa com nome e medicina compartilhada.
- [x] Páginas/contagens de status derivadas dos registros; save v8 atual com
  coleção/RNG e empréstimos restaurados. Sem leitura de saves anteriores.
- [x] Capacidade mista 10/20 em regras; compra/editor reais, cinco noites,
  obra com animais presentes, novos cochos consumidos, maturidade e retomada
  pelo app no percurso instalado. Configure/build aprovados; **25/25 CTests,
  18,58 s** na seleção final, **46 processos de teste** no total.

Fontes, arquivos e como experimentar: [PF06_LIVESTOCK.md](PF06_LIVESTOCK.md).
**Não estão concluídos:** venda, leite/lã/escova, reprodução/poções, máquinas,
pastagem/transporte, sequência completa de cues/vozes XA, festivais e funerais.
Mortos ficam reservados no domínio; eventos/remoção precisam ser integrados.
Esses sistemas continuam no backlog PF-06, sem refazer a coleção nem exigir uma
nova migração geral de arquitetura. Windows/Release/sanitizers, suíte inteira
não foram executados; PF-06 permanece aberta.

### PF-06 — Segunda casa: concluída no recorte

Entrega de 2026-10-28. Quarta obra original, sem novo controlador de construção:

- [x] Gotz oferece 10.000 G/750 madeiras pelo modal/roteiro existentes; descrição
  0x137, oferta 0x13F, fundos/materiais/obra ocupada e débito únicos.
- [x] Quatro manhãs, notificação de data idempotente, save/reload durante a obra;
  `construction.house_level` 0..2, flag 0x3D e comando 95 derivados, 80 pontos.
- [x] Recursos sazonais 16..19 da fazenda e 88..91 da casa, portas/colisões e
  objetos originais. Geladeira, armário, cozinha/receitas, TV, calendário,
  opções e diário usam as mesmas interfaces e o inventário central.
- [x] Despertar (902,0,1804) e cama (-2950,y,-2560) das tabelas originais.
  Sono seleciona modelos por LINK/slot, corrigindo também a primeira casa.
- [x] Retirada/devolução da madeira compartilhada; lareira da casa2 no inverno,
  consumo ao completar a animação, fadiga -5 apenas na primeira entrega desde
  acordar, latch por visita e três quadros de textura extraídos do BIN.
- [x] Inventário/depósitos/receitas/animais preservados, codec atual v9 e carga
  pelo app em outro processo; sem compatibilidade com versões anteriores.
- [x] Configure/build aprovados; **23/23 CTests, 69,05 s** na seleção principal,
  **49 processos de teste** no total, incluindo 2/2 verificações dirigidas e o
  percurso repetido após preservar a capacidade nativa de madeira. Capturas de
  ambas as camas conferidas.

Relatório/fontes/arquivos/reprodução: [PF06_SECOND_HOUSE.md](PF06_SECOND_HOUSE.md).
Família/casamento, aquisição de utensílios e cozinha completa continuam pendentes.
Windows, Release, sanitizers e suíte inteira não foram executados. PF-06 segue aberta.

### PF-06 — Estufa: concluída no recorte de construção/cultivo

Entrega de 2026-10-28, após a segunda casa:

- [x] Quinta oferta real de Gotz, 30.000 G/580 madeiras, descrição 0x138/oferta
  0x140. Cancelamento/fundos/materiais/débito único e quatro manhãs com reload.
- [x] `construction.greenhouse` único, flags 0x39/0x3E, comando 95 e 100 pontos
  derivados. Recursos originais da fazenda 20..23 e estufa 124..127, portas e
  resolução de IDs sazonais/lógicos para entrada e restauração.
- [x] Campo próprio 7×7 em `session.greenhouse`, distinto do externo, mesma
  estrutura e motor de ferramentas, plantar/regar/crescer/rebrotar/colher.
  Inverno e mudanças de estação não matam plantas internas; chuva não as rega.
- [x] 49 quads originais, UVs/CLUT/páginas da estufa, pacote de sprites 4 e
  faixas de cultivos extraídas/reutilizadas. Sem partículas/tintura de clima
  no interior, como `weather_fx_frame` original; draw não altera gameplay.
- [x] Perda original de 31/100 em tempestade/nevasca, RNG persistente próprio,
  células limpas sem reiniciar RNG de plantio, mapa/entrada/pontos atualizados.
  Reconstrução usa o mesmo pedido/prazo e mantém os outros sistemas.
- [x] Nabo cultivado e vendido no inverno por ações reais; preservação dos
  campos, inventário, depósitos, receita e animais do celeiro; save v10 e
  retomada do save em outro processo. Sem migradores de versões anteriores.
- [x] Configure/build Linux aprovados; **22/22 CTests, 20,24 s**, mais percurso
  direto final verificando UVs/CLUTs reais. **50 processos de teste no total**,
  incluindo filhos, testes iniciais e tentativas de desenvolvimento.

Relatório, fontes, arquivos e reprodução: [PF06_GREENHOUSE.md](PF06_GREENHOUSE.md).
Orangecup não foi habilitada. Transporte/repatriação de animais na estufa,
clima exterior completo, família e demais sistemas do backlog continuam abertos.
Windows/Release/sanitizers, instalação limpa e suíte inteira não foram executados.
PF-06 permanece em andamento.

### PF-06 — Orangecup: concluída no recorte em 2026-10-28

- [x] Grupo D original de Won agora oferece Orangecup (1.000 G) nas quatro
  estações, com ou sem estufa. São sete entradas sem Moondrop, oito após a
  visita 19. Nomes, descrição 0x16C e painel de quantidade usam o fluxo existente.
- [x] Plantio/rega/crescimento/colheita no mesmo motor da estufa: quatro estágios
  de dois dias regados, sem rebrota, sem morte por estação e sem rega por chuva.
  Pacote 4/sprite 24, faixas 3..6; produto colhido 0x85, vendido por 60 G.
  O item distinto 0x56 continua com preço zero; não foi trocado pelo produto.
- [x] Cancelar, faltar dinheiro/espaço, enviar pacote ao baú, consumir a semente
  uma vez, noite seca, virada do ano, mochila/mão/portas, pagamento único,
  perda/reconstrução da estufa e save/load v10 em outro processo verificados.
- [x] Configuração/build Linux Debug e seleção final **18/18 CTests**, **10,60 s**.
  **49 processos de teste** no total, incluindo referência inicial, tentativas,
  wrappers e filhos. Suíte inteira e Windows não executados.

A auditoria corrigiu a recusa externa presumida no planejamento anterior:
`shop_seed_menu_setup_d` não exige estufa/estação; `field_event_dispatch` não
recusa especificamente Orangecup; `CROP_ORANGE_CUP` tem estação 0. Conservados
plantio externo fora do inverno e crescimento sazonal compartilhado. A fonte
não desenha Orangecup na primavera, usa sprite 18 no verão e 38 no outono.
Essa assimetria foi preservada e documentada, sem inventar uma trava de compra
ou de plantio. Comparação dessa particularidade com execução do original
permanece pendente. O cultivo anual visível na estufa está validado.

Não há novo inventário, catálogo manual, algoritmo de crescimento ou migração
arquitetural. O esquema de save permanece v10; `src/` e backends preservados.
Fontes, arquivos, limites e reprodução: [PF06_ORANGECUP.md](PF06_ORANGECUP.md).
**PF-06 continua aberta.**

### Próxima implementação recomendada — PF-06, ferramentas animais e leite/lã

1. Auditar `menu/shop_dialogues.c:shop_tool_upgrade_menu_setup`,
   `shop_greenhouse_step` e o roteiro de Saibara: integrar a compra real de
   escova, ordenhadeira e tesoura no modal/inventário/economia existentes.
   Não conceder ferramentas gratuitamente em partidas normais; os demais
   serviços do ferreiro continuam com aceite próprio.
2. Adaptar em `port/` os blocos de `animals/livestock.c:livestock_cow_cue`,
   `livestock_sheep_harvest` e cuidado diário, e as fases BRUSH/MILK/CLIP de
   `people/player.c`. Usar exclusivamente `session.livestock`, as ações,
   pedidos de produto, inventário, apresentação e remessa já existentes.
3. Integrar maturidade/saúde/afeição/qualidade, recusa de ação inválida,
   ordenha única por dia, intervalo de tosquia e aparência da ovelha tosquiada
   conforme as fontes. Persistir os efeitos e respeitar a ordem diária atual.
4. Aceite: compra/cancelamento/fundos/espaço e posse compartilhada; vaca jovem
   sem leite, adulta produzindo, repetição impedida, lã/tosquia/recrescimento,
   escovação, produto real/mão/mochila/venda, noites e saves em outro processo.
   Proteger alimentação/medicina, galinhas, obras, estufa e sementes; até 50
   processos de teste por entrega. Reprodução, máquinas e pastagem permanecem
   na fila seguinte, sem declarar o ciclo animal completo prematuramente.
5. Atualizar este roadmap, matriz e arquitetura com os critérios efetivamente
   atendidos; não retomar MA-07..25 como migração geral obrigatória.


A fila continua sendo este arquivo. Não habilitar uma melhoria ainda sem seus
mapas/estado/uso real e não criar aquisição gratuita para saltar dependências.
O custo da tabela original é referência de conteúdo, não limite de hardware PC.

| Ordem | Trabalho | Dependências e critério de conclusão |
|---|---|---|
| 1 — concluída | Carpintaria, madeira e galinheiro: 5.000 G/420 madeiras | Percurso acima, mesma economia/inventário, quatro manhãs, mapas e capacidade reais. |
| 2 — concluída no recorte | Primeira casa: 4.700 G/370 madeiras | Percurso acima: mapas/móveis, geladeira/armário, núcleo da cozinha, quatro manhãs e save v6. Aquisição de utensílios e apresentação completa da cozinha continuam pendentes. |
| 3 — concluída no recorte base | Celeiro e aquisição/alimentação de vacas e ovelhas | Obra/mapas/cochos, compra real/nome/arte, coleção única, capacidade 10/20, alimentação/crescimento/conversa/medicina/status e save v8 validados acima. Produção, reprodução e ciclo animal completo continuam pendentes. |
| 4 — concluída no recorte | Segunda casa: 10.000 G/750 madeiras | Percurso acima: obra, mapas/objetos compartilhados, cama/diário, lareira, estado preservado e save v9. Família e aquisição de utensílios continuam pendentes. |
| 5 — concluída no recorte | Estufa: 30.000 G/580 madeiras | Percurso acima: obra/entrada, campo 7×7 no motor comum, cultivo anual, perda/reconstrução, apresentação e save v10. Transporte de animais e conteúdo restante seguem abertos. |
| 6 — concluída no recorte | Orangecup | Compra com Won, oito noites regadas na estufa, virada do ano, arte, colheita/venda e persistência v10; regras externas/assimetria visual documentadas acima. |
| **7 — próxima** | Ferramentas animais e leite/lã | Compra real de escova/ordenhadeira/tesoura, cuidado, produção/qualidade/intervalos, arte, remessa e persistência no mesmo domínio do celeiro; critérios acima. |

`shop_greenhouse_step`, em `menu/shop_dialogues.c`, trata melhorias de
ferramentas/acessórios; não é o fluxo de construção. `field/greenhouse.c` contém
a grade interna e também alguns helpers de níveis de ferramentas: distinguir as funções.
Continuar adaptando em `port/`, sem editar `src/`, preservar BIN e backends.
Limite de testes: 50 execuções/processos de teste por rodada, incluindo filhos;
selecionar regressões pertinentes. **Não manter migradores de saves antigos**
nesta fase; só a versão atual deve salvar/carregar corretamente.

### Destino das antigas etapas abertas

| IDs anteriores | Encaminhamento atual; não concluídos |
|---|---|
| MA-07/08 | Separação de execução/apresentação entregue em PF-04 para os fluxos atuais; detalhes do plano antigo não são conclusão automática. |
| MA-09 | Flags/RNG no sistema consumidor de PF-02/03/06. Preservar seeds/sequências atuais até uma mudança deliberada; coop não exige unificação agora. |
| MA-10..14 | PF-03, por comandos/roteiros reais, sem bloquear regras independentes. |
| MA-15/16 | PF-01; remessas adicionais no percurso PF-02 e ampliação PF-06. |
| MA-17 | Consumidores existentes em PF-01; demais lojas conforme PF-03/06. |
| MA-18/19/20 | PF-02; completar calendário, clima e culturas em PF-06. |
| MA-21..24 | PF-06, conforme sistema; regressões dos comportamentos atuais protegidas em todas as entregas pertinentes. |
| MA-25 | Revisão contínua da matriz/roadmap e consolidação PF-07; deixa de ser um bloqueio para planejar funcionalidades. |

## Critério comum de conclusão das entregas PF

- Investigar a função original antes de escrever regra nova; adaptar em `port/`
  com origem rastreável. Se o código nativo já reaproveita corretamente o
  algoritmo, preservá-lo e completar apenas as lacunas comprovadas.
- Integrar aos consumidores reais, com estado autoritativo e uma implementação
  de produção por regra. Preservar backends, instalação BIN e `src/` intacto.
- Separar dependências de formato/hardware de regras de conteúdo. Limites
  originais de gameplay são valores iniciais de referência, configuráveis quando
  necessário; limites de segurança dos leitores continuam obrigatórios.
- Testar o percurso e fronteiras do domínio; rodar configuração, build e CTest
  nativos nos pontos de integração. Registrar plataformas/checks não executados.
  Usar os grupos de testes da matriz conforme o risco, sem novos gates genéricos.
- Atualizar roadmap, matriz e arquitetura. Documentar dependências restantes e
  o destino dos adapters temporários; nenhuma funcionalidade é concluída somente
  por reorganizar arquivos. Não implementar rede/editor de mods preventivamente.

## PF-01 — Entrega validada em 2026-10-25

- [x] Adaptado `estate_gold_change` em `game/economy.c`: saldo, retorno,
  insuficiência/reset e ledger, sem duplicações financeiras em lojas/remessas/
  recompensa de evento. Teto original é default configurável por sessão.
- [x] `game/inventory.c` reúne consulta/remoção, armazenamento ordenado de
  ferramentas, compactação, troca/ciclo e colocação na mão. Estado continua
  em `HmbtnGameState`; nenhuma cópia ativa nova. Conversas/medicina, casa/menu
  e coleta/campo/ovos compartilham as operações pertinentes.
- [x] Algoritmos nativos já equivalentes foram conservados; operandos de script,
  notificações e poses ficam separados das operações de estado. Wrappers de
  eventos delegam às mesmas regras, sem outro interpretador.
- [x] Corrigido o ciclo rápido para respeitar a capacidade acessível sem puxar
  itens de células inativas; sentinelas e contagem por registro conservadas.
- [x] Configuração e build `native-debug` aprovados; **81/81 CTests**, zero falhas,
  **51,08 s**, incluindo 28 percursos com recursos locais e 53 testes independentes
  desses dados. Dois testes novos: `native_economy_rules` e `native_inventory_rules`.
  Os nove testes selecionados de regras e consumidores também passaram.

Fontes, interfaces, diferenças, arquivos e pendências em
[PF01_SHARED_RULES.md](PF01_SHARED_RULES.md). `src/`, backends, fluxo BIN e
alterações locais anteriores preservados. Saves em disco continuam ausentes;
inicialização e persistência em memória entre UI/áreas/manhã protegidas pelos
percursos existentes. Nenhum módulo `src/` passou a ser compilado diretamente:
o reaproveitamento desta entrega é por adaptação rastreável dos algoritmos.
Capacidades 8/64, catálogo de 380 itens e layout visual atual não foram ampliados.
Windows/cross-build, Release, Android, sanitizers, instalação limpa e inspeção
manual de janela/áudio não foram executados. PF-02 não foi iniciada.

## PF-02 — Entrega validada em 2026-10-25

- [x] Nabo comprável por 120 G na mesa central de sementes do supermercado, na primavera,
  com Jeff presente; menu base de primavera com nomes, seleção e quantidade
  de 1 a 99, conforme o original. Pilhas/lote inteiro no baú usam `inventory.c`;
  débito usa `economy.c`. Cancelamento/lotação/insuficiência cobertos.
- [x] Banco/ANI e grade original de nove alvos no plantio; dias regados separados
  da definição imutável da cultura. Nabo cresce após quatro dias regados e entra
  no carry/inventário/remessa comuns; preço original de 60 G e pagamento único.
- [x] `daily.c` possui observer, avanço de minutos e preparação de manhã sem SDL;
  a adaptação de relógio existente foi conservada. Campo cresce antes da produção
  de galinhas/limpeza dos cochos; duplicata da secagem na cena removida. Datas e
  manhãs repetidas não reaplicam os efeitos; fast-forward entrega cada dia.
- [x] Sprites originais de broto/maturidade desenhados sem alterar estado de
  crescimento. Dados sobrevivem à casa/fazenda/sono. Saves continuam ausentes.
- [x] Testes novos `native_crop_daily` (sem SDL) e `native_crop_playthrough`
  (recursos reais): ciclo completo, dias secos, estação/inverno/ano, rebrota do
  pepino/skip-day, pausa, múltiplos dias, produção antes da limpeza e efeitos únicos.
- [x] Configuração e build `native-debug` aprovados; **84/84 CTests**, zero falhas,
  **51,58 s**, sendo 55 independentes dos dados opcionais e 29 percursos
  com `data/`. Capturas locais da mesa de sementes, broto e nabo maduro inspecionadas.

Fontes, adaptações, lista de arquivos e limites em
[PF02_CROP_CYCLE.md](PF02_CROP_CYCLE.md). Preservados `src/`, infraestrutura
nativa, instalação pelo BIN e alterações locais anteriores. Não houve novo
consumo de RNG para simular clima/ervas nem compilação direta de módulos
originais. A compra cobre o catálogo base de primavera (grama/nabo/batata/pepino)
com quantidade; cultivo completo das demais culturas, desbloqueios, outras
estações, lojas completas, clima e estufa continuam em PF-06; grama/feno foram integrados no recorte acima. O runtime completo
update/render ainda dependia de PF-04, concluída posteriormente. Windows/cross-build, Release, Android,
sanitizers, instalação limpa e sessão manual de janela/áudio reais não executados.
Na entrega PF-02, PF-03 ainda não havia sido iniciada; sua conclusão está
registrada acima.

Correção da compra em 2026-10-25: substituído o gatilho incorreto de entrega
(zona 36) pelo grupo correto da mesa (zona 37 + registro/chave originais).
Sprites de pacotes sazonais desenhados com os recursos/posições originais;
interação não altera a orientação de Jeff. Testes ampliados rejeitam balcão e
outros grupos e percorrem a compra correta. Configuração/build Linux e **83/83
CTests** aprovados, zero falhas, **51,22 s**, após a correção e integração dos
sprites da mesa. Detalhes no relatório PF-02.


Correção do menu/falas em 2026-10-26: removidos o comprar/não comprar e a
abertura pela descrição do nabo. Fluxo adaptado de `shop_menu_open_choose`,
`shop_menu_choose_next` e `shop_buy_stored_item`: lista original somente com
nomes → item → quantidade/total → compra/retorno, com IDs/bancos originais,
informações por Triângulo e cancelamentos. Novo `native_seed_shop` e percurso
com BIN ampliado para três pacotes, recusas e aviso de baú (3:0x207).
Configuração/build Linux aprovados; **84/84 CTests**, zero falhas, **51,58 s**
(55 sem dados opcionais + 29 com `data/`). Preservação dos dois pacotes após
plantio/áreas/noites também passou no percurso isolado com a asserção adicional.
Capturas de lista, falas, quantidade e compra inspecionadas via SDL software.
Windows e sessão manual não verificados. Detalhes e limites no relatório PF-02.
Na conclusão dessa correção, PF-03 era a próxima entrega. Ela foi concluída
posteriormente, assim como PF-04; a fila atual está no recorte PF-06 acima.

## Entregas MA já concluídas — histórico preservado

Os registros abaixo descrevem cada entrega na ocasião. Menções a “próxima MA”
e dependências antigas são históricas; a fila ativa é a tabela PF acima.

### MA-00 — Planejamento e referência inicial

- [x] Registrar estratégia, arquitetura real, matriz, ordem de migração e testes.
- [x] Executar configuração, build e testes nativos existentes no Linux disponível.

Aceite: somente documentação alterada nesta etapa; resultados e limitações
registrados em [Referência inicial](#referência-inicial). Isso não conclui
nenhuma migração de código.

### MA-01 — Entrega validada em 2026-10-25

- [x] `HmbtnGameState.inventory` possui equipamento, mochila de ferramentas/produtos,
  mão, baú, nível/capacidade e quatro suprimentos; a casa só empresta um ponteiro.
- [x] Inicializar/recriar a interface não inicializa o inventário; novos jogos
  recebem os mesmos valores anteriores. Slots, menus, ações, lojas e scripts
  acessam os registros compartilhados, sem espelho sincronizado.
- [x] Consultas de gatilho e descrições não copiam/substituem o equipamento.
  Rollback das conversas preserva seu escopo anterior e usa checkpoint explícito.
- [x] Novos testes de propriedade única, interfaces/ações/scripts, duas instâncias,
  carregamento de áreas, manhã e limites de save/load passaram junto da suíte.
- [x] Configuração e build `native-debug` aprovados; **66/66 CTests**, zero falhas,
  43,48 s, incluindo os 64 anteriores e os dois testes novos.

Detalhes, APIs, arquivos e limites em [INVENTORY_STATE.md](INVENTORY_STATE.md).
A suíte usou os recursos locais instalados em `data/` (22 percursos opcionais e
44 testes independentes de `HMBTN_TEST_GAME_DATA`). Não houve alteração em
`src/`, na instalação pelo BIN ou nas regras financeiras/calendário/RNG.
Na entrega MA-01, MA-02 ainda não havia sido iniciada. Saves continuam ausentes,
sem leitura/escrita simulada.
ASan/UBSan foram tentados em `build/native-inventory-asan`, mas a configuração
não passou do teste do compilador: runtimes `libasan.so.8.0.0` e
`libubsan.so.1.0.0` ausentes (arquivos estáticos também indisponíveis).
Windows real/cross-build, Release, Android e inspeção manual não foram executados.

### MA-02 — Entrega validada em 2026-10-25

- [x] `HmbtnGameState.economy` é dono da carteira, 25 contadores de remessas,
  quatro pares de receita/despesa e todo o estado da remessa pendente/coleta.
- [x] Removidos os campos financeiros de `HmbtnFarmMenu.stats` e a remessa
  separada da cena. Menu consulta referência const; lojas, scripts e remessas
  alteram a mesma economia, sem cópias/sincronizações.
- [x] Saldo inicial de 500 G instalado somente na criação do estado; recriar UI
  ou carregar área não altera carteira/contas/remessas. Preservados algoritmos,
  preços, limites, datas especiais e ordem de efeitos.
- [x] Testes de duas interfaces/lojas, script, consultas gráficas, coleta única,
  áreas, manhã, limites de save/load e instâncias independentes aprovados.
- [x] Configuração e build `native-debug` aprovados; **68/68 CTests**, zero falhas,
  44,17 s (66 anteriores + `native_economy_state` e `native_economy_playthrough`).

Detalhes, APIs, lista de arquivos e limites em [ECONOMY_STATE.md](ECONOMY_STATE.md).
A suíte usou os recursos locais em `data/`: 23 percursos opcionais e 45 testes
independentes de `HMBTN_TEST_GAME_DATA`. `src/`, backends e instalação pelo BIN
foram preservados. Saves continuam ausentes. Windows, Release, Android,
ASan/UBSan e inspeção manual não foram executados nesta entrega; a tentativa de
sanitizers de MA-01 permanece registrada acima. Na entrega MA-02, MA-03 ainda
não havia sido iniciada.
A unificação dos algoritmos financeiros com funções originais continua em MA-16.

### MA-03 — Entrega validada em 2026-10-25

- [x] `HmbtnGameState.clock` possui o relógio atual e frames; removidos o relógio
  separado da cena, os seis campos de calendário da casa e `sync_clock`.
- [x] Casa, TV, HUD, calendário, ações, loja e scripts consultam o mesmo relógio.
  A loja empresta uma referência const; sua pausa modal foi preservada.
- [x] `HmbtnGameState.registration` possui nomes e aniversário. `HmbtnNewGame`
  conserva somente edição/mensagens e referência ao cadastro; UI, conversas,
  textos e menus consultam o registro central. O título entrega somente valores
  de cadastro à abertura, sem transportar ponteiros ou estado de editor.
- [x] Recriar interfaces preserva data/cadastro; defaults, 8 glifos/terminador,
  aniversário zero-based, pausas, avanço, sono e ordem dos callbacks conservados.
- [x] Configuração e build `native-debug` aprovados; **70/70 CTests**, zero falhas,
  44,07 s (68 anteriores + `native_calendar_state` e `native_calendar_playthrough`).
  Testes cobrem duas interfaces, variáveis de script, nomes/TV/HUD/calendário,
  áreas, pausa, manhã/virada de ano, limites de save/load e instâncias isoladas.

Detalhes, APIs e lista dos arquivos em [CALENDAR_STATE.md](CALENDAR_STATE.md).
Foram usados recursos locais em `data/`: 24 percursos opcionais e 46 testes
independentes de `HMBTN_TEST_GAME_DATA`. `src/`, instalação pelo BIN e backends
foram preservados; nenhum algoritmo original adicional foi portado nesta etapa.
Windows, Release, Android, sanitizers e inspeção manual não foram executados.
Saves em disco continuam ausentes. Na entrega MA-03, MA-04 ainda não havia sido
iniciada; algoritmos de avanço originais mais diretos e pipeline diário continuam
em MA-18/19.

### MA-04a — Ciclo de vida e registros, validado em 2026-10-25

- [x] `port/game/session.*` cria/destrói `HmbtnSession` sem SDL ou I/O. A sessão
  possui `HmbtnGameState`, `HmbtnFarmField`, `HmbtnVillage` e `HmbtnPoultry`.
- [x] Removido o armazenamento desses registros da cena. Casa/menu/lojas/ações/
  scripts usam referências à sessão; recriar interfaces ou carregar áreas
  conserva inventário, economia, cadastro, células, moradores e animais.
- [x] Agendas emprestadas pelo host têm associação/retirada explícita; apenas a
  primeira associação inicializa a vila, no mesmo handoff anterior. Reassociar
  o recurso conserva registros, flags, contadores, agenda decodificada e RNG.
- [x] Testes sem SDL de duas sessões e troca de UI, recursos reassociados, terreno,
  moradores e animais; integração com áreas reais e galinha reservada/transportada.
- [x] Configuração/build `native-debug` aprovados; **72/72 CTests**, zero falhas,
  44,16 s (70 anteriores + `native_session_state` e `native_session_playthrough`).
  A referência dos 70 anteriores após a migração também passou, em 44,20 s.

Detalhes, limites e arquivos em [SESSION_STATE.md](SESSION_STATE.md). A suíte
usou `data/`: 25 percursos opcionais e 47 testes independentes de
`HMBTN_TEST_GAME_DATA`. Windows, Release, Android, sanitizers e inspeção manual
não foram executados. Saves em disco continuam ausentes; `src/`, backends e
instalação pelo BIN foram preservados.

**Estado ao encerrar 04a:** MA-04 continuava aberta, com execução e atores de
evento na cena. A entrega 04b abaixo resolve essa propriedade; callbacks e
coordenação continuam previstos em MA-05..08. A regra de um recorte por entrega
foi preservada.

### MA-04b — Execução e identidade de eventos, validado em 2026-10-25

- [x] `HmbtnSession` possui execução de abertura/visitas/conversas/eventos de
  animais, seus cursores, slots, atores, mensagens transitórias e gatilhos.
  A casa também pertence à sessão, pois conversas emprestam seus contadores.
- [x] Bytecode validado é copiado para armazenamento da sessão, mantendo o
  interpretador existente; falha de validação preserva a execução anterior.
- [x] Vínculos explícitos selecionam uma transformação ativa por morador;
  colisão, interação, zonas e desenho usam a mesma consulta. Rotas e sua
  animação ficam suspensas durante o roteiro e retomam a agenda ao liberar
  os vínculos. IDs locais da infância não substituem moradores homônimos.
- [x] Rebind de recursos/UI conserva evento ativo e seus atores. Modelo e
  callback/contexto do host são empréstimos retirados por unbind; bytecode,
  jogador e referências internas continuam válidos. Arte/animações ficam no host.
- [x] Dois novos testes: `native_session_events` sem SDL e
  `native_session_events_playthrough` com recursos reais; os percursos existentes
  de visitas, torta, ingredientes, primeira manhã, conversas e funeral passaram.
- [x] Configuração/build `native-debug` aprovados; **74/74 CTests**, zero falhas,
  44,07 s (72 anteriores + 2 novos). Referência anterior à edição: 72/72, 44,34 s.
  Incluídos 26 percursos opcionais com `data/` e 48 testes independentes desse recurso.

**MA-04 concluída (04a/b).** Não há execução de evento ou estado de ator
espelhados na cena; o registro de rota fica dormente sob override, sem sincronização
por frame. Checkpoints transacionais de conversa e estados de apresentação
continuam com seus papéis existentes. Saves em disco continuam ausentes; não
houve migração de formato. `src/`, backends e instalação pelo BIN foram preservados.

Callbacks específicos, coordenação dos ticks e dependência entre draw e update
continuam para MA-05..08; o jogo completo ainda não roda sem apresentação.
Ao encerrar MA-04b, a próxima entrega prevista era MA-05a. A solicitação
seguinte autorizou MA-05 inteira; sua conclusão está registrada abaixo. APIs, limites e arquivos em
[SESSION_EVENTS.md](SESSION_EVENTS.md). Windows, Release, Android, sanitizers,
instalação limpa e inspeção manual de janela/áudio não foram executados.

### MA-05 — Contratos mínimos de serviços, validado em 2026-10-25

Entrega integral de 05a e 05b, conforme solicitação do usuário:

- [x] Entrada lógica `HmbtnLogicalInput` sem SDL, produzida pelo adapter existente;
  pressões são consumidas uma vez e botões mantidos conservam seu comportamento.
- [x] `HmbtnSession.services` identifica pedidos/resultados com tickets de 64 bits,
  estados idle/pending/done/failed/cancelled e aquisição única pelo provedor.
  Polling não resubmete efeitos; respostas terminais só saem por ack explícito.
- [x] Recursos lógicos de evento/gatilho/ação/texto usam os leitores delimitados
  existentes. Resultados possuem cópia própria, limite de 1 MiB e liberação
  explícita; não publicam buffers parciais. Recursos/áudio recebidos ainda
  seguram o cursor até aplicação e ack pelo consumidor.
- [x] Mensagens de eventos e conversas, espera de animação de NPC, conclusão de
  ação do jogador, carregamento/início de música e início/término de XA usam
  pedidos identificados. Espera, erro e cancelamento não concluem comandos.
- [x] Backends SDL3/UI/ANI/áudio existentes foram integrados. Unbind republica
  intenções pendentes com novos tickets e invalida respostas do adapter anterior;
  novo evento/destruição limpa resultados e buffers.
- [x] Três testes novos: protocolo sem SDL, sessão com provedores controlados e
  percurso com recursos/áudio reais. Testam respostas imediatas/atrasadas, erro,
  cancelamento, polling/resposta repetidos, recursos, IDs, buffers e efeitos
  únicos em inventário/economia. O teste de entrada lógica também foi ampliado.
- [x] Configuração e build `native-debug` aprovados; **77/77 CTests**, zero falhas,
  **44,51 s**: 50 independentes dos dados opcionais e 27 percursos com `data/`.

**MA-05 concluída.** Ao encerrar aquela entrega, a próxima implementação era
MA-06. A solicitação seguinte autorizou a tarefa inteira, concluída abaixo. A coordenação continua no adapter da cena e `draw`
ainda pode atualizar a simulação, conforme MA-06..08. Não foram adicionados
threads, um scheduler genérico ou outro interpretador; chamadas de desenho,
recursos de apresentação e efeitos curtos fora dos consumidores migrados seguem
nos backends existentes até suas etapas. Saves em disco continuam ausentes.

Arquivos, ownership, limites de integração e origem estão em
[SERVICE_CONTRACTS.md](SERVICE_CONTRACTS.md). Windows/cross-build, Release,
Android, sanitizers, instalação limpa e inspeção manual não foram executados.
`src/`, infraestrutura/recursos existentes e fluxo de instalação pelo BIN foram
preservados, inclusive as alterações locais de MA-04b presentes no início.

### MA-06 — Tick de exploração livre, validado em 2026-10-25

Entrega integral de 06a, 06b e 06c:

- [x] `game/exploration.*` expõe `hmbtn_session_exploration_tick`, sem SDL,
  arquivo, renderer, voz host ou relógio de parede. Recebe entrada lógica,
  seção/terreno e ANI decodificados. A cadência continua agendada pelo host.
- [x] Estado de controle, ferramentas, pesca, carry/feed/gather, loja/menu e
  contadores de exploração pertence à sessão. A cena não mantém espelhos;
  referências de inventário/economia/relógio e registros permanecem únicas.
- [x] Movimento/colisão, ferramentas, coleta/arremesso/transporte, alimentação,
  pesca, interação de casa, menus e pausa usam a lógica existente extraída.
  Texto/expansão/parsing e efeitos de gameplay ficam no domínio; arte e áudio
  são efeitos de apresentação. Sem apresentação, o tick continua funcionando.
- [x] Relógio, notificações de remessa/vila/galinhas, fadiga, agendas e avanço
  das animações necessárias ao gameplay estão na sessão. Ground e ANI são
  consultados/avançados sem desenhar, sem forçar sinais de animação concluída.
- [x] O coordenador atual chama as mesmas fases extraídas nos pontos anteriores.
  Portas, viagens, sono e conversas/roteiros continuam no adapter para MA-07;
  o tick integral de exploração recusa transições/roteiros pendentes. Um frame
  emprestado não é reutilizado após trocar uma área. Não há outro fluxo de regras.
- [x] `native_exploration` roda sem SDL, somente libc/libm, com chão/ANI sintéticos
  validados e serviços controlados; cobre movimento/colisão, pausa, relógio,
  ferramentas, coleta, voo, feed, galinha, pesca e remessa única às 17h58.
- [x] `native_exploration_playthrough` usa três sessões com recursos reais,
  os mesmos 180 ticks e zero/uma/duas apresentações por tick (~0/30/60 fps).
  Ferramenta completa sem desenho; posição, ANI, campo, inventário/economia,
  calendário, moradores e pausa coincidem ao fim de cada tick.
- [x] Configuração/build `native-debug` aprovados; **79/79 CTests**, zero falhas,
  **50,88 s**: 51 independentes de dados opcionais e 28 percursos com `data/`.
  Os 77 anteriores também passaram após a extração, em 44,49 s.

**MA-06 concluída integralmente. Na ocasião, a próxima prevista era MA-07 —
tick de roteiros e transições**, ainda não iniciada. Essa fila foi substituída
pelas entregas PF na revisão de direção acima. A cena ainda agenda as fases durante seu fluxo
atual e `draw` pode dirigir esse coordenador. A separação global update/render,
título e migração dos percursos para API pública permanecem para MA-07/08.
A preparação de recursos livres mantém o host síncrono existente: ausência,
pendência ou erro não são sucesso. Não foi criado outro scheduler ou runtime.

Regras, RNGs atuais, persistência em memória, limites de saves e fluxo BIN foram
preservados. `src/` não foi alterado. Windows/cross-build, Release, Android,
sanitizers, inspeção manual e instalação limpa não foram executados. APIs,
empréstimos, origem e arquivos em [EXPLORATION_TICK.md](EXPLORATION_TICK.md).

## Referência inicial

Execução em **2026-10-25**, no Linux x86-64 disponível, sobre a árvore de trabalho
com alterações locais preexistentes. Não é uma referência de commit limpo e
nenhuma migração de gameplay foi aplicada nesta etapa.

| Comando | Resultado observado |
|---|---|
| `cmake --preset native-debug` | Sucesso; CMake configurou e gerou `build/native-debug`. |
| `cmake --build --preset native-debug` | Sucesso; runtime e executáveis de teste compilados. |
| `ctest --preset native-debug` | **64/64 aprovados**, zero falhas; 43,95 segundos. |

Condições importantes para reproduzir a cobertura:

- Cache existente com `HMBTN_TEST_GAME_DATA` apontando para `data/` local e
  `FETCHCONTENT_SOURCE_DIR_SDL3` para `build/dependencies/sdl3-source`.
- SDL3 3.4.18 local, compilador `/usr/bin/cc`, configuração Debug. Não houve
  necessidade de baixar dependências. Recursos proprietários não foram adicionados.
- Os **21 percursos opcionais** com dados reais foram executados, além dos 43
  testes independentes desse parâmetro. Sem `HMBTN_TEST_GAME_DATA`, CTest registra
  uma suíte menor; ausência desses percursos não deve ser relatada como aprovação.
- Os percursos usam vídeo/renderização/áudio dummy/software conforme o CMake.
  Não houve validação manual de janela, som ou instalador nesta execução.
- A configuração avisou sobre libdecor ausente e não encontrou alguns componentes
  opcionais, como libusb/sndio; isso não impediu o build e a suíte observados.
- Windows real, cross-build Windows, Release, sanitizers e Android **não foram
  executados nesta referência**. A CI existente define Linux/Windows Debug/Release,
  mas seu resultado não foi presumido. Build PS1/SHA-1 não faz parte da referência.

O log local do CTest fica em `build/native-debug/Testing/Temporary/LastTest.log`;
outputs de build/testes são artefatos locais, não documentação de recursos do jogo.

Para uma configuração nova com recursos já instalados, informar também
`-DHMBTN_TEST_GAME_DATA=/caminho/dos/dados` ao configurar o preset. Se SDL3 não
estiver instalada ou no cache, usar o mecanismo existente de dependência local
ou download; registrar a opção realmente usada.

## Regressões a proteger por entrega

- Título/cadastro → abertura/infância → controle na casa → passeio Sim/Não.
- Casa ↔ fazenda ↔ vila/interiores, portas/escadas, nomes e inventário preservados.
- Ferramentas/coleta → mochila → lançamento/remessa → pagamento/contas.
- Loja → ração → cocho → sono → ovos; compra, nomeação, incubação, transporte,
  venda, medicina, funeral e estado das galinhas.
- Rotas, conversa, presentes/aniversários, afinidade, Greg/vara e pesca.
- Visitas de Thomas, torta e entrega de ingredientes usando scripts instalados.
- Bootstrap/entrada/gamepad, formatos/truncamento, leitura/importação de recursos,
  áudio/XA e renderização sintética.

Os CTests e arquivos correspondentes estão em `port/CMakeLists.txt` e
`port/tests/`; a matriz seleciona os pertinentes a cada recorte. Reexecutar a
suíte completa nos pontos de integração. Após alterações de fronteira, validar
Linux/Windows na infraestrutura disponível e registrar separadamente a execução
real de janela/instalador. Sanitizers de memória/UB em Linux são recomendados nas
extrações de estado/ponteiros; não trocar de toolchain ou afirmar essa cobertura
sem executá-los.
