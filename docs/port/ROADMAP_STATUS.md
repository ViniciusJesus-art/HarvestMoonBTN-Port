# Status do porte nativo

**Atualizado em:** 2026-10-25  
**Escopo atual:** Linux e Windows; Android é futuro.  
**Prioridade:** migração arquitetural MA do [ROADMAP.md](ROADMAP.md).  
**Concluídas:** MA-01..03 — inventário, economia, calendário e cadastro centrais.  
**Em andamento:** MA-04 — 04a concluída; 04b aberta.  
**Próxima implementação:** MA-04b — execução e identidade dos atores de evento.

## Situação atual

O projeto possui runtime nativo C11/SDL3, recursos originais instalados pelo BIN
do usuário, renderização e áudio próprios, título/cadastro e trechos jogáveis
de abertura, casa, fazenda e vila. Relógio, economia, inventário, galinhas,
moradores, presentes, afinidade, pesca e alguns eventos funcionam parcialmente.
**O jogo completo ainda não executa no host.** O bootstrap `--diagnostics` não
executa o jogo.

Nenhum módulo C de `src/` é compilado diretamente no runtime PC. Há adaptações
em `port/`, tabelas extraídas do original e interpretação dos scripts instalados.
A [arquitetura atual](ARCHITECTURE.md) descreve os limites; a
[estratégia](PORTING_STRATEGY.md) define o aumento do reaproveitamento do C original.
Compilar PS1, manter MIPS/ABI ou reproduzir SHA-1 não são critérios do produto PC.

## Marcos e prioridades

| Marco | Situação | Próximo encaminhamento |
|---|---|---|
| MA — Migração arquitetural | Em andamento; MA-01..03 e recorte 04a concluídos; MA-04b e demais etapas abertas | Estado → sessão/apresentação → comandos → regras compartilhadas → ciclo diário → moradores. |
| M1 — Aplicação nativa | Funcional no recorte atual | Preservar entrada/janela; validar Windows real. |
| M2 — Recursos e apresentação | Funcional parcialmente | Preservar leitores/backends; ampliar fidelidade com comparação reproduzível. |
| M3 — Título/cadastro | Funcional parcialmente | Preservar fluxos; separar tick/apresentação e rastrear algoritmos/inicialização. |
| M4 — Trecho jogável | Parcial e extenso | Proteger abertura, controle, portas, mapa e eventos na migração. |
| M5 — Sistemas e progressão | Parcial | Migrar regras; depois completar cultivos, clima, espécies, história e saves. |
| M6 — Instalação/distribuição | Parcial | Preservar BIN/importação/pacotes; validar instalação limpa e Windows real. |

Os antigos 219 itens concluídos e 62 pendentes eram contagens de entradas de
checklists sobrepostos. São registros históricos, não contagem atual de sistemas
nem denominador de progresso da migração. O [backlog complementar](ROADMAP_PENDENTE_CODEX.md)
reconcilia esses temas; a ordem executável e o aceite estão no roadmap principal.

## Referência executada nesta revisão

Linux x86-64, Debug, SDL3 3.4.18 local, árvore com alterações preexistentes:

- `cmake --preset native-debug`: sucesso.
- `cmake --build --preset native-debug`: sucesso.
- `ctest --preset native-debug`: **72/72 aprovados**, zero falhas, 44,16 s (70 anteriores + 2 novos da MA-04a).

A suíte incluiu os 25 percursos opcionais com `HMBTN_TEST_GAME_DATA` configurado
para os recursos locais. Sem esse parâmetro a cobertura é menor. Condições,
avisos e reprodução estão na [referência inicial do roadmap](ROADMAP.md#referência-inicial).

Não foram executados nesta revisão: Windows real/cross-build, Release,
Android, inspeção manual de janela/áudio ou instalação limpa. ASan/UBSan não foram
repetidos nesta entrega; a tentativa de MA-01 não chegou a executar testes
por ausência dos runtimes compartilhados e arquivos estáticos.
CI configurada não equivale a execução comprovada. Os resultados não demonstram
jogo completo ou fidelidade integral ao original.

## Entrega MA-01 e limites

`HmbtnGameState.inventory` passou a possuir equipamento, mochila, mão, baú,
capacidade e suprimentos. Casa/menus/ações/scripts compartilham seus registros;
recriar a UI e carregar áreas não os reinicializa. Há testes de integração,
rollback, consultas sem mutação, duas instâncias e manhã/limites de save/load.
Detalhes e arquivos em [INVENTORY_STATE.md](INVENTORY_STATE.md).

Regras e backends existentes foram preservados. Checkpoints transacionais e
itens em ações/voo não são inventários ativos paralelos. Ainda faltam regras
originais mais diretas (MA-15), sessão independente de gráficos (MA-04..08) e
outros estados. A entrega MA-01 não alterou finanças/calendário/RNG ou `src/`.

## Entrega MA-02 e limites

`HmbtnGameState.economy` é a origem única de saldo, contas, contadores e estado
de remessas. O menu consulta uma referência const; lojas, eventos e coleta
usam o mesmo estado. Abrir/recriar UI, transições de área e sono preservam os
registros; a inicialização financeira pertence somente ao novo jogo.
Detalhes e arquivos em [ECONOMY_STATE.md](ECONOMY_STATE.md).

Foram preservados preços, limites, algoritmos e backends. Ainda há aritmética
financeira nos consumidores, prevista para consolidação com o original em MA-16;
não há mais carteira/contas/remessa ativas paralelas. Na entrega MA-02, calendário/cadastro
ainda aguardavam migração. Os contadores não financeiros do menu e os demais
registros permanecem com os proprietários atuais. Saves continuam ausentes.

## Entrega MA-03 e limites

`HmbtnGameState.clock` é o relógio único; casa e loja emprestam referências,
sem `sync_clock`. `HmbtnGameState.registration` possui nomes e aniversário;
`HmbtnNewGame` empresta esses dados e conserva somente a edição transitória.
Recriar UI, mudar de área e dormir preservam o cadastro e aplicam somente o
avanço temporal já existente. O título entrega valores, sem ponteiros de editor.

Os novos testes verificam leituras de script, TV, renderização de nomes/horário/
calendário, pausa, virada de ano e limites de save/load; os testes anteriores
continuam aprovados. [CALENDAR_STATE.md](CALENDAR_STATE.md) registra APIs e arquivos.
Não foram alterados algoritmos de relógio/cadastro, backends ou `src/`.
Clima, coordenação de sessão e demais registros continuam nas etapas previstas.
Na entrega MA-03, MA-04 ainda não havia sido iniciada.

## Entrega MA-04a e limites

`HmbtnSession` possui game/field/village/poultry, com criação e destruição sem
SDL. A cena possui uma referência à sessão, mantendo carregamento e apresentação.
UI, lojas e scripts acessam os mesmos registros; não há cópias sincronizadas dos
módulos migrados. Agendas são emprestadas pelo host e podem ser reassociadas sem
reinicializar moradores. A inicialização original e algoritmos foram preservados.

Foram testadas duas sessões, recriação de interfaces, vida útil dos empréstimos,
transições com recursos e transporte de galinha sem duplicar o registro. A lista
de arquivos e APIs está em [SESSION_STATE.md](SESSION_STATE.md).

A MA-04 inteira continua aberta: execução e atores de evento permanecem na cena
para o próximo recorte **MA-04b**. Tick, serviços e separação gráfica continuam
em MA-05..08. O ciclo de vida sem SDL não significa gameplay completo sem vídeo.
