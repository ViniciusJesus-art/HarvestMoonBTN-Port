# Estratégia do porte nativo

Decisão de projeto: 2026-10-25. Alvos atuais: Linux e Windows; Android é futuro.
Este documento define a direção; o [ROADMAP.md](ROADMAP.md) é a fonte principal
da ordem, dependências e critérios das próximas tarefas. A
[arquitetura atual](ARCHITECTURE.md) descreve o código existente; a
[matriz de migração](PORTING_MATRIX.md) relaciona suas funções às fontes originais.
A arquitetura abaixo é o **alvo**; MA-01..03 centralizaram inventário, economia, calendário e cadastro.
O restante segue planejado; entregas em [INVENTORY_STATE.md](INVENTORY_STATE.md),
[ECONOMY_STATE.md](ECONOMY_STATE.md), [CALENDAR_STATE.md](CALENDAR_STATE.md) e
[SESSION_STATE.md](SESSION_STATE.md) (MA-04a), [SESSION_EVENTS.md](SESSION_EVENTS.md) (MA-04b)
[SERVICE_CONTRACTS.md](SERVICE_CONTRACTS.md) (MA-05 integral) e
[EXPLORATION_TICK.md](EXPLORATION_TICK.md) (MA-06 integral). PF-01 integrou
regras compartilhadas adaptadas de inventário/dinheiro; fontes e limites em
[PF01_SHARED_RULES.md](PF01_SHARED_RULES.md). PF-02 integrou o primeiro cultivo
completo e processamento diário em [PF02_CROP_CYCLE.md](PF02_CROP_CYCLE.md).
PF-03 compartilhou comandos e integrou o convite de Duke em
[PF03_REUSABLE_EVENTS.md](PF03_REUSABLE_EVENTS.md). PF-04 consolidou ticks e
apresentação separados em [PF04_UPDATE_PRESENTATION.md](PF04_UPDATE_PRESENTATION.md).
PF-05 entregou saves nativos para exploração estável, com codec versionado e
retomada transacional em [PF05_NATIVE_SAVES.md](PF05_NATIVE_SAVES.md).
PF-06 entregou a batata reaproveitando o mesmo motor e catálogo, com duas
mudanças de integração em produção: filtro de suporte e sprite original.
Fontes/testes em [PF06_POTATO.md](PF06_POTATO.md). Grama/feno também foram integrados
no mesmo campo/estoque, com regras originais de crescimento, corte e rebrota:
[PF06_GRASS.md](PF06_GRASS.md). Tomate/milho/cebola e o menu base de verão
foram integrados sem outro motor ou estado em [PF06_SUMMER_CROPS.md](PF06_SUMMER_CROPS.md).
Berinjela/cenoura/batata-doce e o menu base de outono usam os mesmos sistemas em
[PF06_FALL_CROPS.md](PF06_FALL_CROPS.md), incluindo passagem ao inverno e saves.
Clima diário/chuva, sementes sazonais de Won e desbloqueios do supermercado
foram integrados. Pink Cat e Toy Flower também reutilizam os sistemas existentes,
conforme [PF06_FLOWERS.md](PF06_FLOWERS.md). Magic Red e Moondrop também
foram integradas: [PF06_MAGIC_RED.md](PF06_MAGIC_RED.md) e
[PF06_MOONDROP.md](PF06_MOONDROP.md). A carpintaria de Gotz e a ampliação do
galinheiro são a primeira dependência entregue da cadeia da estufa:
[PF06_CARPENTRY.md](PF06_CARPENTRY.md). A primeira casa, geladeira/armário e núcleo
da cozinha foram integrados em [PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md).
Obra/mapas/cochos do celeiro foram integrados em
[PF06_BARN_EXPANSION.md](PF06_BARN_EXPANSION.md). Aquisição/presença/alimentação bovina e ovina foram integradas em
[PF06_LIVESTOCK.md](PF06_LIVESTOCK.md). A segunda casa, posições de sono/retorno
e lareira de inverno foram integradas em [PF06_SECOND_HOUSE.md](PF06_SECOND_HOUSE.md).
A estufa de cultivo foi integrada em [PF06_GREENHOUSE.md](PF06_GREENHOUSE.md),
com campo distinto, regras comuns e perda/reconstrução. Orangecup foi habilitada
nesses mesmos sistemas em [PF06_ORANGECUP.md](PF06_ORANGECUP.md). Ferramentas animais, escovação, ordenha e tosquia foram integradas em
[PF06_ANIMAL_TOOLS.md](PF06_ANIMAL_TOOLS.md). Próximo: venda de vacas e ovelhas,
conforme o roadmap;
venda/reprodução/ciclo animal completo permanecem pendentes.
Aquisição de utensílios/cozinha completa e PF-06 permanecem abertas.

## Objetivo e diretrizes permanentes

Concluir um porte nativo de Harvest Moon: Back to Nature, aproveitando o máximo
possível do C decompilado como fonte de algoritmos, regras, estruturas e
comportamentos. A prioridade é completar sua progressão em PC com o comportamento original
como referência e permitir expansão posterior de conteúdo. A base nativa atual
é aproveitada; refatorações entram com necessidades concretas de implementação.

1. Reutilizar diretamente a lógica C original quando tecnicamente viável.
2. Adaptar funções existentes para remover dependências PS1 antes de considerar
   uma reimplementação. Preservar decisões e ordem de efeitos; distinguir regras do jogo de limites
   técnicos do PS1. Limites de gameplay são valores iniciais, não tetos permanentes
   para a expansão do port.
3. Reimplementar somente quando a adaptação for inviável ou prejudicar a
   arquitetura; registrar a razão concreta, a origem e os testes equivalentes.
4. Preservar SDL3, renderização, áudio, leitores/decodificadores, instalação e
   empacotamento existentes. Seu uso não depende de emular um PS1.
5. Pesquisar primeiro a implementação em `src/`; não criar um sistema paralelo
   quando já houver lógica aproveitável.
6. Centralizar dados compartilhados e regras. Cada dado mutável tem um único
   proprietário; menus e renderizadores não são donos do inventário ou dinheiro.
7. Separar sessão, gameplay e apresentação. Desenhar ou abrir uma interface não
   deve, por si só, avançar o mundo ou aplicar transações.
8. Migrar gradualmente, mantendo os percursos jogáveis e substituindo um recorte
   por vez. Separar mudanças de organização de mudanças de comportamento.
9. Manter `src/` intacto. Adaptações, tipos e interfaces host ficam em `port/`,
   com rastreabilidade de arquivo e função originais.
10. Preservar a instalação em que o usuário fornece seu BIN original. Distribuir
    somente o runtime e ferramentas permitidas, sem recursos proprietários.

## Saves durante o desenvolvimento

Decisão do usuário em 2026-10-27: **não exigir compatibilidade entre saves de
versões diferentes nesta fase**. O codec aceita somente sua versão atual; uma
mudança de esquema pode exigir jogo novo. Rejeitar versões antigas/futuras com
motivo claro, sem tentar inferir/migrar dados ou apagar arquivos do usuário.
Continuam obrigatórios: integridade dos saves atuais, retomada sem efeitos
repetidos, validação de dados e publicação transacional que preserve o arquivo
anterior em caso de falha. Migradores entre versões não são pré-requisito de uma
entrega. Compatibilidade para uma versão publicada poderá ser definida depois.
Os relatórios anteriores descrevem a compatibilidade existente naquelas entregas;
não são compromissos de manutenção. A construção introduziu v5; casa/depósitos/cozinha introduziram o formato v6, que
não lê versões anteriores. O celeiro acrescentou nível em v7; a coleção animal e RNG foram introduzidos em v8. O formato vigente é v11, com
ordenha/cuidados diários dos animais, sem leitura de versões anteriores.

## Escopo PC e legado PS1

Compilar para PS1, manter ABI/MIPS, reproduzir executáveis/overlays ou obter o
SHA-1 original não são requisitos nem critérios de aceite deste porte. Os
arquivos legados em `src/`, `sdk/`, `disc/`, `tools/` e CMake permanecem como
referência; removê-los não faz parte desta migração. O build nativo já seleciona
`port/` antes do toolchain PS1. Usar `build/native-*` para host.

Comparações de imagem, áudio e comportamento com o jogo original são evidência
útil de fidelidade, sem exigir reconstrução PS1. Não confundir SHA-1 de um
executável com SHA-256 de integridade dos recursos instalados: o manifesto e as
checagens de formato/intervalo continuam necessários ao fluxo BIN.

As diretrizes desta decisão substituem, para o planejamento futuro, as metas
anteriores de manutenção/validação do build PS1. Registros históricos continuam
válidos como registros do que foi feito, não como requisitos atuais. Nenhuma
alteração do build legado ou das instruções em `AGENTS.md` foi feita nesta etapa.

## Extensão sem herdar restrições de hardware

Novas plantas, itens, áreas e eventos devem usar os mesmos sistemas do conteúdo
original. Separar definições de conteúdo (identidade, parâmetros e recursos) do
estado das instâncias (crescimento, posição e progresso). IDs/contagens fixos do
BIN são tratados na importação/adaptação, sem virar tetos universais do runtime.
Não é necessário converter todos os arrays em alocações dinâmicas agora; remover
restrições no domínio que estiver sendo ampliado, sem prometer suporte a novos
conteúdos antes de integrar seus leitores e consumidores.

Reaproveitar C significa conservar algoritmos úteis, não reproduzir alocação de
memória, overlays, dependências MIPS, endereços ou orçamento de atores do PS1.
Câmera, resolução, desempenho e interfaces PC podem evoluir sobre os backends
existentes, sem exigir emulação de limitações visuais. Formatos originais ainda
precisam de leitura correta, e capacidades que afetam regras do jogo exigem testes
quando forem alteradas deliberadamente.

Coop é possibilidade futura. Manter estado e comandos independentes do desenho
facilita essa evolução; não introduzir rede, replicação, rollback ou determinismo
global como pré-requisito do port de um jogador. Linux/Windows vêm primeiro,
Android e expansões recebem suas próprias entregas depois.

## Critério para escolher como portar uma função

| Opção | Quando usar | Registro obrigatório |
|---|---|---|
| Compilar C de origem sem alterações | A unidade e seus headers podem ser usados no host sem SDK, assembly ou ABI PS1. | Fonte, funções, dependências host e teste de resultado. |
| Adaptar em `port/` | Algoritmo aproveitável, mas com estado global, desenho, I/O, ponteiros serializados ou dependências PSY-Q. | Funções de origem, alterações necessárias e diferenças deliberadas. |
| Manter backend nativo existente | Serviço host ou backend de gráficos/áudio/recursos já funcional. | Contrato consumido pelo gameplay e seus limites. |
| Reimplementar | Há impedimento técnico demonstrado para as opções anteriores. | Impedimento, alternativas avaliadas e evidência de equivalência. |

Compilar um arquivo inteiro não é condição para reaproveitar seu algoritmo.
Extrair uma função para uma adaptação rastreável em `port/` é preferível a
replicar sua regra em vários sistemas. Includes gerados reutilizam **dados**,
não contam como compilação das funções originais. Não usar inclusão textual de
grandes `.c`, renomeações de globais por macros ou stubs PSY-Q para esconder
dependências ainda não resolvidas.

Em cada adaptação, registrar junto ao código: caminho e símbolo de origem,
revisão de referência obtida na implementação, motivo da adaptação, diferenças
de semântica e testes associados. Atualizar a linha correspondente da matriz.
Os candidatos da matriz ainda precisam dessa verificação de compilabilidade;
nenhum é declarado portável sem adaptação apenas por ser C.

## Arquitetura desejada

```text
Aplicação / SDL3 / entrada / relógio host
                 |
          atualização da sessão
                 |
     estado central + regras do jogo
       |         |           |
     eventos   economia   mundo/atores/calendário
                 |
      pedidos e resultados de serviços
                 |
 arquivos/recursos   áudio   apresentação SDL3
                            ^
                    visão de leitura da sessão
```

O gameplay não inclui SDL, não abre arquivos, não conhece texturas/vozes host e
não chama renderizadores. Ele recebe entrada lógica, dados decodificados e
resultados de operações solicitadas. A sessão possui estado e coordena o tick;
o host fornece os serviços. Pedidos pendentes preservam seu identificador e
cursor até a conclusão; um carregamento ou uma animação não pode cobrar, dar
um item ou avançar um script duas vezes.

Distribuição proposta, evoluída conforme as entregas funcionais PF do roadmap:

| Local proposto | Responsabilidade |
|---|---|
| `port/game/game_state.*` | Estado autoritativo de inventário, finanças, calendário, progressão e registros do mundo. |
| `port/game/session.*` | Ciclo de vida, tick, transições, pausa e coordenação dos módulos existentes. |
| `port/game/` e submódulos futuros | Algoritmos originais portados, comandos, regras e consultas de mundo. |
| Contratos em `port/game/` | Entrada lógica, visão de leitura, pedidos de recursos/áudio/apresentação e resultados. |
| `port/graphics/` | Recursos gráficos, caches, composição, renderização e estado visual transitório. |
| `port/audio/`, `filesystem/`, `platform/`, `input/`, `installer/` | Backends existentes e adaptação dos contratos para serviços host. |

`port/game/game_state.*` já possui inventário (MA-01), economia (MA-02),
calendário e cadastro (MA-03). `session.*` já possui o ciclo de vida desse estado
e de field/village/poultry (MA-04a) e execução/identidade de eventos (MA-04b);
contratos mínimos dos consumidores atuais foram introduzidos em MA-05. MA-06
extraiu controle/mundo/animação livres para `exploration.*`. PF-04 consolidou
roteiros/portas/sono em `runtime.*`, separou tick/draw da cena e do título/cadastro
e retirou os adapters provisórios de coordenação dos fluxos PF-02/03.
MA-07..25 foram substituídas como fila, sem execução ou conclusão presumidas. Não criar todas as
interfaces de uma vez nem uma camada genérica sem consumidor real.

## Estado, estruturas e propriedade

`FarmState` e seus subtipos são o mapa conceitual inicial, sem copiar padding,
endereços fixos ou layouts binários. Preservar identidade dos atores, flags, sentinelas e regras de atualização.
Capacidades iniciais seguem o jogo base; representações devem permitir ampliá-las
sem duplicar sistemas ou transportar layout binário. Separar:

- **Estado do jogo:** cadastro, inventário, carteira, contas, data/clima, campo,
  animais, moradores, flags e progressão. Deve sobreviver a menus e trocas de área.
- **Execução da sessão:** cursores de scripts, ações em andamento, filas,
  transições e pausa. PF-05 bloqueia salvar durante essas execuções; somente
  estados estáveis são persistidos. Ampliar essa política explicitamente caso
  haja necessidade futura de retomar scripts/ações em andamento.
- **Apresentação:** texturas, atlas, pixels, canais de áudio, cursor visual,
  rolagem e caches. Pode ser recriada sem reinicializar os dados do jogo.

Frames de animação que determinam o efeito de uma ação pertencem à simulação
ou a um contrato explícito de conclusão; ocultar o desenho não pode impedir a
colheita ou alterar o tempo de um evento. Caches e leituras em const não devem
alterar o estado autoritativo. Menus produzem comandos para a mesma regra usada
por scripts, em vez de aplicarem cópias de regras financeiras ou de inventário.

A mudança inicial de propriedade já foi realizada em MA-01..06, preservando
algoritmos e valores existentes. As próximas entregas priorizam regras originais
e percursos funcionais; não reiniciar a centralização nem exigir o antigo plano
MA inteiro antes de portar uma função. Durante a transição, adapters podem emprestar referências ao estado
central. Não manter cópias mutáveis sincronizadas em dois sentidos. Remover o
armazenamento antigo no mesmo recorte em que o novo assume a propriedade.

## Eventos e serviços compartilhados

Manter a leitura validada de `event_script.c` e construir um único runtime de
comandos a partir de `event_commands.c`, com acesso tipado ao estado central.
Conversas, abertura, visitas e eventos de animais fornecem contexto e recursos;
não devem possuir versões diferentes de aritmética, flags e inventário.

Migrar primeiro slots, comparações, aritmética e variáveis já suportadas; depois
mensagens, atores, transições e gatilhos. Cada família de comandos deve declarar
dependências e conservar resultados/esperas/desvios originais. Comando ainda
ausente produz limite explícito, nunca sucesso simulado. As restrições atuais
por evento só saem após cobertura equivalente dos roteiros que dependem delas.

Flags, calendário, RNG, dinheiro e inventário terão serviços únicos. Ao unificar
RNG, primeiro conservar os estados/sequências atuais por domínio; trocar a
sequência por uma única sequência original é mudança posterior de comportamento,
com semente, ordem de consumo e efeitos documentados. Não exigir reprodução
binária para testar limites e probabilidades do jogo.

## Segurança dos dados e portabilidade

Recursos continuam little-endian, com offsets/endereços serializados de 32 bits.
Usar leitores explícitos, tamanhos fixos e validação de limites; não converter
palavras do disco em ponteiros host nem usar `long` como inteiro PS1. Ponteiros
host existem apenas em estruturas de runtime; PF-05 usa formato nativo
versionado explícito, sem dump de structs. Auditar sinal, overflow, shifts,
alinhamento e bitfields em cada algoritmo adaptado.

Overlays viram módulos/funções nativas, não regiões executáveis ou carregadores
MIPS. Geometria de colisão é dado do gameplay, não dependência do desenho. Os
backends SDL3 existentes continuam prestando serviços; Android será outro host
dessas interfaces quando Linux/Windows estiverem consolidados.

## Migração e prevenção de regressões

Executar as tarefas e os critérios no [roadmap](ROADMAP.md). Cada entrega deve:

1. Identificar fonte e comportamento do recorte, seus consumidores e testes.
2. Registrar resultados antes da troca; separar regressões de lacunas conhecidas.
3. Trocar um único proprietário/algoritmo/família de comandos, com adapter
   temporário de prazo definido pela tarefa dependente.
4. Validar consumidores reais e retirar a implementação duplicada do recorte.
5. Atualizar matriz, arquitetura e roadmap com o que foi migrado e o que falta.

Por orientação do usuário, limitar a validação a **50 execuções de teste por
rodada de trabalho**, contando repetições e processos de teste filhos. Selecionar
os testes afetados e regressões relevantes; não executar automaticamente toda
a suíte quando ela ultrapassar esse limite. Registrar a seleção, resultados e
o que não foi executado. A quantidade de testes cadastrados pode crescer;
o limite trata das execuções por rodada, sem remover cobertura existente.

Manter testes unitários sintéticos no Git e percursos com BIN instalado somente
no ambiente local autorizado. Comparar valores e transições, não bytes de
structs com padding ou ponteiros. Um teste diferencial pode executar a função
original isolada com dependências reais mínimas; quando isso exigir SDK ou
endereços PS1, usar vetores e rastros revisados do algoritmo, registrando a
origem. Não deixar duas implementações de produção permanentes para comparação.

Além dos CTests existentes, as tarefas MA exigem testes de propriedade única,
sessões independentes, pausa, atualização sem vídeo/áudio, conclusão assíncrona
idempotente e renderização sem mutação. MA-01..03 introduziram testes de inventário, economia, calendário e cadastro
compartilhados, persistência em memória e consultas gráficas sem mutação.
MA-04a acrescentou criação/destruição sem SDL, empréstimos de recursos,
recriação de UI e registros entre áreas. MA-04b acrescentou bytecode/cursor
próprios, autoridade dos atores, rebind durante evento ativo, duas sessões
e retomada de rota. MA-05 acrescentou pedidos/resultados identificados,
respostas imediatas/atrasadas/erro/cancelamento, buffers próprios e polling sem
repetir efeitos, com provedores sintéticos e backends reais. MA-06 acrescentou
tick de exploração sem SDL, conclusão de ações por ANI sem desenho, pausa/relógio
e equivalência com zero/uma/duas apresentações por tick nos recursos reais.
Os demais recortes ainda serão cobertos
nas etapas correspondentes. A referência inicial e os resultados por entrega estão
no roadmap.
