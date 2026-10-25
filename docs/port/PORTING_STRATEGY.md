# Estratégia do porte nativo

Decisão de projeto: 2026-10-25. Alvos atuais: Linux e Windows; Android é futuro.
Este documento define a direção; o [ROADMAP.md](ROADMAP.md) é a fonte principal
da ordem, dependências e critérios das próximas tarefas. A
[arquitetura atual](ARCHITECTURE.md) descreve o código existente; a
[matriz de migração](PORTING_MATRIX.md) relaciona suas funções às fontes originais.
A arquitetura abaixo é o **alvo**; MA-01..03 centralizaram inventário, economia, calendário e cadastro.
O restante segue planejado; entregas em [INVENTORY_STATE.md](INVENTORY_STATE.md),
[ECONOMY_STATE.md](ECONOMY_STATE.md), [CALENDAR_STATE.md](CALENDAR_STATE.md) e
[SESSION_STATE.md](SESSION_STATE.md) (MA-04a).

## Objetivo e diretrizes permanentes

Concluir um porte nativo de Harvest Moon: Back to Nature, aproveitando o máximo
possível do C decompilado como fonte de algoritmos, regras, estruturas e
comportamentos. A prioridade é preservar o comportamento do jogo e completar
sua progressão em PC.

1. Reutilizar diretamente a lógica C original quando tecnicamente viável.
2. Adaptar funções existentes para remover dependências PS1 antes de considerar
   uma reimplementação. Preservar decisões, ordem de efeitos e limites numéricos.
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

Distribuição proposta, a ser introduzida conforme as tarefas MA:

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
e de field/village/poultry (MA-04a). Execução/identidade de eventos é MA-04b;
contratos e tick independente da apresentação ainda são MA-05..08. Não criar
todas as interfaces de uma vez nem uma camada genérica sem consumidor real.

## Estado, estruturas e propriedade

`FarmState` e seus subtipos são o mapa conceitual inicial, sem copiar padding,
endereços fixos ou layouts binários. Preservar identidade dos atores, flags,
capacidades, sentinelas e regras de atualização. Separar:

- **Estado do jogo:** cadastro, inventário, carteira, contas, data/clima, campo,
  animais, moradores, flags e progressão. Deve sobreviver a menus e trocas de área.
- **Execução da sessão:** cursores de scripts, ações em andamento, filas,
  transições e pausa. Sua persistência futura será decidida no desenho dos saves.
- **Apresentação:** texturas, atlas, pixels, canais de áudio, cursor visual,
  rolagem e caches. Pode ser recriada sem reinicializar os dados do jogo.

Frames de animação que determinam o efeito de uma ação pertencem à simulação
ou a um contrato explícito de conclusão; ocultar o desenho não pode impedir a
colheita ou alterar o tempo de um evento. Caches e leituras em const não devem
alterar o estado autoritativo. Menus produzem comandos para a mesma regra usada
por scripts, em vez de aplicarem cópias de regras financeiras ou de inventário.

A primeira mudança será de propriedade, preservando algoritmos e valores
existentes. Durante a transição, adapters podem emprestar referências ao estado
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
host existem apenas em estruturas de runtime; persistência futura terá formato
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
recriação de UI e registros entre áreas. Os demais recortes ainda serão cobertos
nas etapas correspondentes. A referência inicial e os resultados por entrega estão
no roadmap.
