# Status do porte nativo

**Atualizado em:** 2026-10-30
**Escopo atual:** Linux e Windows; Android é futuro.  
**Prioridade:** port funcional com reaproveitamento do C original, conforme a fila PF do [ROADMAP.md](ROADMAP.md).
**Concluídas:** MA-01..06 e PF-01..05 — base central, regras compartilhadas, cultivo, evento, atualização independente do desenho e saves nativos estáveis.

**PF-06 em andamento:** batata, grama/feno, culturas base de verão/outono, clima diário/chuva, sementes sazonais de Won, desbloqueios do supermercado, Pink Cat/Toy Flower, variantes de Magic Red e Moondrop/visitas de Karen, carpintaria, ampliação do galinheiro e primeira casa (depósitos/núcleo da cozinha) concluídos nos respectivos recortes; obra/mapas/cochos do celeiro também entregues; aquisição/capacidade/alimentação bovina/ovina, segunda casa/lareira, estufa, Orangecup, ferramentas animais e leite/lã integrados; demais sistemas originais ainda parciais/ausentes.
**Próxima implementação:** PF-06 — lago da fazenda e ração de peixes.
Restaurante de Kai entregue em [PF06_KAI_RESTAURANT.md](PF06_KAI_RESTAURANT.md).
Refeições da hospedaria entregues em [PF06_INN_MEALS.md](PF06_INN_MEALS.md).
Vinícola entregue em [PF06_WINERY.md](PF06_WINERY.md).
Clínica e remédios entregues em [PF06_CLINIC.md](PF06_CLINIC.md).
Consumo direto entregue em [PF06_FOOD_USE.md](PF06_FOOD_USE.md).
Embrulhos entregues no recorte de [PF06_WRAPPING.md](PF06_WRAPPING.md).
Alimentos entregues no recorte de [PF06_GROCERIES.md](PF06_GROCERIES.md):
compra dos cinco produtos, mão/mochila/geladeira e curry com ingredientes comprados.
Cesta de remessas entregue no recorte de [PF06_BASKET.md](PF06_BASKET.md).
As duas ampliações de mochila foram entregues em [PF06_RUCKSACK.md](PF06_RUCKSACK.md).
Utensílios/apresentação da cozinha e acessórios de Saibara também entregues nos
recortes do [roadmap](ROADMAP.md).
Melhorias de ferramentas de Saibara entregues no recorte: minério/XP, preço,
confirmações, três manhãs, retirada com guarda de espaço e carga/alcance/efeitos
reais das cinco ferramentas. [Relatório e validação](PF06_TOOL_UPGRADES.md).
**Saves:** adaptar o necessário à funcionalidade; compatibilidade entre versões
não é requisito nem proibição. PF-06 e cozinha completa seguem abertas.
MA-07..25 foram substituídas como fila, não concluídas. Refatorações necessárias
acompanham as entregas funcionais; PF-04 separou atualização/desenho nos fluxos atuais.

## Entrega atual — restaurante de Kai

Conversa/dispatch e agendas originais de verão, cinco itens, preços/descrições,
cancelamento e saldo insuficiente. Fluxo compartilhado com Doug: débito único,
comida ANI 5/bebida banco 17 e atributos ao término. Água gratuita tem mensagem
final própria. Percurso confere almoço/retorno, domingo sem loja e três
checkpoints reabertos em processos novos. Codec existente suficiente;
`runtime.c` e portas do usuário preservados integralmente neste recorte.
[Auditoria, validação e limites](PF06_KAI_RESTAURANT.md). Debug 16/16 e Release
4/4 selecionados; Windows/Wine unidade/percurso/três retomadas aprovados.
46 processos incluindo wrappers/produtores/filhos/lançadores, sem suíte inteira.
Próximo: lago da fazenda e ração de peixes. Outros eventos de Kai, família,
hospedagem/festivais e PF-06 continuam abertos. Bootstrap não executa o jogo;
runtime parcial. Wine não substitui validação em Windows físico.

## Referência anterior — refeições da hospedaria

O balcão original de Doug abre seis opções diurnas/quatro noturnas conforme
horário. Débito único, mensagens originais e consumo automático por ANI;
comida usa ANI 5, bebida usa banco 17 e restaura o registro completo da mão.
Água gratuita, limites de atributos e preços originais. Percurso com erva
realmente coletada, dez serviços, mudança 17:59/18:00, saída pela porta e
checkpoints em processos novos. Codec existente suficiente, portas preservadas.
[Auditoria, validação e limites](PF06_INN_MEALS.md). Debug 15/15 e Release 4/4
selecionados; Windows/Wine unidade/percurso/três retomadas aprovados.
46 processos incluindo wrappers/produtores/filhos/lançadores, sem suíte inteira.
Próximo: restaurante de Kai. Hospedagem, eventos/família/festivais e restante
PF-06 continuam abertos. Bootstrap não executa o jogo; runtime parcial.

## Referência anterior — vinícola

Manna abre a compra de vinho/suco pelo bytecode original, com horários,
preços, cancelamento, saldo insuficiente, débito/entrega únicos e ANI real.
Geladeira e consumo usam os domínios existentes: vinho altera fadiga/felicidade,
suco comprado recupera 5 de stamina; suco preparado usa a qualidade.
Percurso compra três bebidas, guarda/retira/bebe, coleta uvas e usa o suco
comprado numa receita real de qualidade 23, com checkpoints em processos novos.
[Auditoria, validação e limites](PF06_WINERY.md). Codec existente suficiente;
portas do usuário preservadas. Debug 15/15 e Release 4/4 selecionados;
Windows/Wine unidade/percurso/três retomadas aprovados. 46 processos incluindo
tentativas/wrappers/filhos/lançadores, sem suíte inteira.
Próximo: refeições da hospedaria.
Vindima/Cliff, festivais e demais progressões continuam abertos.
O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.

## Referência anterior — clínica e remédios

O atendimento abre pelo script original do médico. Exame de 10 G com fades,
mensagem sobre fundo preto e diagnóstico de stamina/fadiga; sem cura/tempo
adicional, conforme o original. Quatro medicamentos com preços/descrições/ANI,
marcos XL de 51 ervas remetidas e consumo pelo fluxo existente de bebida.
Percurso instalado entra pela porta, recusa/examina, cruza os marcos com a 51ª
erva coletada/remetida, compra, guarda na geladeira, retira/bebe e retoma.
[Auditoria, validação e limites](PF06_CLINIC.md). Codec suficiente e animações
de portas do usuário preservadas. Debug 11/11 e Release 4/4 selecionados;
Windows/Wine unidade/percurso/três retomadas aprovados. 42 processos contando
tentativas/wrappers/filhos/lançadores, sem suíte inteira.
Próximo: vinícola; internação/colapso,
outras lojas e festivais continuam abertos. Bootstrap não executa o jogo;
runtime parcial.

## Referência anterior — consumo de alimentos e bebidas

Square consome comida/bebida carregada pelas flags e pelo callback originais.
ANI/som distintos para comer e beber, ocultação da figura, qualidade como byte
assinado, efeitos/remoção únicos no fim e limites de stamina/fadiga/felicidade.
Curry/pão/arroz comprados, uvas realmente coletadas e suco preparado/bebido;
relógio continua onde o mapa permite. Codec v19 acrescenta felicidade junto
aos atributos existentes; eventos conservam sua restauração temporária.

Debug 13/13 selecionados e regressões de chuva/portas 2/2, Release 2/2;
Windows/Wine unidade/produtor/percurso/três retomadas aprovados. Foram 48
processos incluindo tentativas/wrappers/filhos/lançadores, sem suíte inteira.
[Auditoria, validação e limites](PF06_FOOD_USE.md). As animações de portas do
usuário foram preservadas. Embrulhos, compras e cesta permanecem entregues.
Naquela entrega, a próxima etapa era a clínica, agora entregue acima. Outras lojas, recebedores/eventos ainda não
portados e festivais permanecem abertos.
O bootstrap `--diagnostics` não executa o jogo; o runtime permanece parcial.

## Situação atual

O projeto possui runtime nativo C11/SDL3, recursos originais instalados pelo BIN
do usuário, renderização e áudio próprios, título/cadastro e trechos jogáveis
de abertura, casa, fazenda e vila. Relógio, economia, inventário, galinhas,
moradores, presentes, afinidade, pesca e alguns eventos funcionam parcialmente.
Nabo, batata, tomate, milho, cebola, berinjela, cenoura e batata-doce têm percurso
completo de compra de sementes até venda. Grama tem
compra/plantio/crescimento/corte/rebrota e abastece o estoque usado pelo celeiro.
**Saves nativos retomam exploração estável desde PF-05. O jogo completo ainda não executa no host.** O bootstrap `--diagnostics` não
executa o jogo.

Nenhum módulo C de `src/` é compilado diretamente no runtime PC. Há adaptações
em `port/`, tabelas extraídas do original e interpretação dos scripts instalados.
A [arquitetura atual](ARCHITECTURE.md) descreve os limites; a
[estratégia](PORTING_STRATEGY.md) define o aumento do reaproveitamento do C original.
Compilar PS1, manter MIPS/ABI ou reproduzir SHA-1 não são critérios do produto PC.

## Marcos e prioridades

| Marco | Situação | Próximo encaminhamento |
|---|---|---|
| Base MA | MA-01..06 concluídas; plano restante substituído | Preservar estado/sessão/serviços/tick; PF-01..05 concluídas; seguir PF-06 (Utensílios da cozinha a seguir (melhorias de ferramentas, máquinas de Saibara, mineração/aquisição de minérios, pastagem/transporte/sino, funerais e reprodução entregues; venda com Barley, ferramentas animais/leite/lã, Orangecup/estufa/segunda casa entregues); primeira casa/depósitos/núcleo da cozinha e carpintaria/galinheiro, Moondrop, Magic Red, flores simples, desbloqueios do supermercado, sementes sazonais de Won, clima diário/chuva, culturas base de verão/outono, batata e grama/feno entregues), sem uma nova fase geral de reorganização. |
| M1 — Aplicação nativa | Funcional no recorte atual | Preservar entrada/janela; validar Windows real. |
| M2 — Recursos e apresentação | Funcional parcialmente | Preservar leitores/backends; ampliar fidelidade com comparação reproduzível. |
| M3 — Título/cadastro | Funcional parcialmente | Tick/apresentação separados na PF-04; preservar fluxos e rastrear algoritmos/inicialização. |
| M4 — Trecho jogável | Parcial e extenso | Proteger abertura, controle, portas, mapa e eventos na migração. |
| M5 — Sistemas e progressão | Parcial | Reaproveitar regras em PF-01; cultivo/ciclo diário PF-02, eventos PF-03, saves PF-05 e expansão da cobertura PF-06. |
| M6 — Instalação/distribuição | Parcial | Preservar BIN/importação/pacotes; validar instalação limpa e Windows real. |

Os antigos 219 itens concluídos e 62 pendentes eram contagens de entradas de
checklists sobrepostos. São registros históricos, não contagem atual de sistemas
nem denominador de progresso da migração. O [backlog complementar](ROADMAP_PENDENTE_CODEX.md)
reconcilia esses temas; a ordem executável e o aceite estão no roadmap principal.

## Última referência — PF-06, funerais bovinos/ovinos

Evento instalado 172 ampliado para vacas e ovelhas: seis ramos por espécie e
causa, mensagens/listas completas, atores/música/igreja originais, tempo e
penalidade única. Limpeza libera mortos e reservas associadas; conserva vivos,
filhos nascidos, outro parto e incubadoras. Funeral seguido de nascimento e
eclosão pelo evento 299 validado. Nenhuma mudança de codec foi necessária.

Linux Debug: 14/14 selecionados; Release: 3/3; build MinGW Release e três testes
no Wine, com repetição do percurso após conferir o bônus original do mel. Até
47 processos, sem suíte inteira. Alterações locais preservadas.
[Fontes C/BIN, arquivos e limites](PF06_ANIMAL_FUNERALS.md).

## Referência histórica — PF-06, reprodução bovina/ovina

Compra/uso de poções de 3.000 G, maternidades 1/2, reserva de registro, contador
1→21 e nascimento/nomeação pelo evento 299 integrados. Mesmo inventário,
coleção/ciclo diário e editor; guardas de produção/venda e banco de texturas do
celeiro originais. Save v12 com validação das relações e retomada em novo app.
Porta externa do Doug anima as duas folhas nas quatro estações, preservando
recuo/ritmo/tempos ajustados pelo usuário e as 18 travessias internas.

Linux Debug: 12/12 selecionados; Release: 2/2 mais percursos diretos; Windows
MinGW Release compilado e três testes aprovados no Wine. Limite de até 49
processos respeitado. Fontes/BIN, decisões e limites em
[PF06_LIVESTOCK_BREEDING.md](PF06_LIVESTOCK_BREEDING.md).

## Referência histórica — PF-06, fidelidade do mundo e venda com Barley

**Revisão 2026-10-29:** caminhada real pelas portas internas, mensagem correta
de Barley, cursores alinhados e devolução de pedra/galho ao plantio corrigidos.
Fontes/binário, testes e limites em [PF06_INTERACTION_FIXES.md](PF06_INTERACTION_FIXES.md).

Portas internas com destino `-2` abrem/fecham pelo modelo original, com recusas
preservadas. Chuva e luz avançam em portas/eventos independentemente do relógio;
coleta consulta o ponto à frente também com os pés fora da grade. Descarte fora da grade mostra
a segunda figura antes de sumir; pedras/galhos retornam ao plantio livre. Agendas de Karen/Ann
comparadas aos dados originais para cinco climas.

Barley oferece venda de vacas/ovelhas adultas, com quatro linhas por página,
preços originais, confirmação/cancelamento, pagamento único e vaga reutilizável.
Coleção/economia/cochos/save v11 compartilhados; gestação continua pendente.
Linux Debug: 21/21 selecionados; novos percursos dirigidos e unidade Release
aprovados. Build Windows MinGW Release e percurso de fidelidade no Wine aprovados. Detalhes e limites em
[PF06_WORLD_FIDELITY.md](PF06_WORLD_FIDELITY.md).

## Referência histórica — PF-06, ferramentas animais e leite/lã

Saibara vende escova/ordenhadeira/tesoura, com posse/capacidade compartilhadas.
Escovação, ordenha diária/qualidade, tosquia/retorno da lã, arte/ANI, remessa e
persistência v11 integradas no mesmo domínio do celeiro. Não é conclusão da
reprodução, venda, máquinas, pastagem, festivais ou ciclo animal completo.

Linux Debug: 16 casos selecionados aprovados, após corrigir uma expectativa
antiga no teste de itens sobre a geladeira. Release: 3/3 aprovados. Windows
MinGW Release compilou; unidade, percurso e retomada em app novo passaram no
Wine. Orçamento conservador de até 50 processos respeitado, sem suíte inteira.
MSVC/nova CI e Windows real não executados. A CI automática inclui a nova
unidade, nove testes por configuração (36 na matriz Linux/Windows Debug/Release).

[Fontes, arquivos, limites e reprodução](PF06_ANIMAL_TOOLS.md). Próxima: venda
de vacas e ovelhas, conforme o roadmap. Alterações existentes nos READMEs preservadas.

## Referência histórica — PF-06, Orangecup

Configure/build Linux Debug aprovados; seleção final **18/18 CTests**, **10,60 s**,
**49 processos de teste** no total incluindo referência/tentativas/wrappers/filhos.
Compra real com Won (1.000 G), quatro estágios/oito noites regadas na estufa,
virada do ano, arte original, produto 0x85/60 G, mão/mochila/portas, pagamento
único, perda/reconstrução e save v10/retomada em outro processo verificados.
A fonte não contém a recusa externa presumida no plano: preservada sua regra
sazonal e explicitada a falta de sprite de primavera. Comparação dessa
particularidade com execução original, Windows real, suíte inteira, Release,
sanitizers e sessão manual de janela/áudio não executados.

[Relatório, arquivos e reprodução](PF06_ORANGECUP.md). Próxima: ferramentas
animais e produção de leite/lã, conforme o roadmap. PF-06 segue aberta.

## Referência histórica — PF-06, aquisição/alimentação de vacas e ovelhas

Configure/build Linux Debug aprovados. Regras iniciais **4/4** e seleção final
**25/25 CTests**, **18,58 s**, com **46 processos de teste** incluindo wrappers,
filhos e dois percursos diretos. Limite de 50 respeitado; suíte inteira não rodada.

Compra/nomeação/cancelamento e fundos com Barley, coleção comum 10/20, posições/
ANI originais, crescimento, alimentação diária antes da limpeza, fala/medicina,
status derivado e save v8. Obra com animais presentes, novos cochos consumidos e
retomada do app aprovados. Produção/venda/reprodução/pastagem/funerais e sequência
integral de cues/vozes continuam pendentes. Windows/Release/sanitizers/Android,
instalação limpa e sessão manual de janela/áudio não executados.

[Relatório, arquivos e reprodução](PF06_LIVESTOCK.md). Próxima: segunda casa;
PF-06 continua aberta.

## Referência histórica — PF-06, obra/cochos do celeiro

Configure/build Linux Debug aprovados. Seleções **8/8 + 20/20 CTests** passaram;
repetição final do novo percurso **1/1**, após ajustar apenas a captura. Total
**49 processos de teste**, incluindo wrappers/filhos/tentativas. Não executada a
suíte inteira. Windows/Release/sanitizers/Android, instalação limpa e sessão
manual de janela/áudio não executados.

Gotz, 6.800 G/500 madeiras, quatro manhãs, nível único, flags/scripts/status,
recursos 12..15/100..103, dez novos cochos abastecidos, portas com feno, quatro
estações e save v7 com retomada pelo app aprovados. Vacas/ovelhas ainda ausentes;
não declarar capacidade/consumo animal apenas pela obra. Próximo: aquisição,
presença e alimentação reais antes de concluir o item do celeiro.
[Relatório e arquivos](PF06_BARN_EXPANSION.md). PF-06 continua aberta.

## Referência histórica — PF-06, primeira casa

Configure/build Linux Debug aprovados. Seleção final **23/23 CTests aprovados,
18,24 s**; total **48 processos de teste** incluindo wrappers, filhos e tentativas
corrigidas, dentro do limite de 50. Não foi executada a suíte inteira.

Gotz cobra 4.700 G/370 madeiras; quatro manhãs; recursos 8..11/84..87,
despertar/cama/móveis, depósitos autoritativos e núcleo da cozinha, receitas e
save v6 atuais. Percurso com dados instalados e retomada pelo app em outro
processo aprovados. Compra de utensílios e apresentação completa da cozinha
continuam pendentes, sem posse gratuita. Windows/Release/sanitizers/Android,
instalação limpa e sessão manual de janela/áudio não executados.

Detalhes e arquivos: [PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md).
Próximo: celeiro, segundo os critérios do roadmap. PF-06 continua aberta.

## Referência histórica — PF-06, culturas base de outono

Linux x86-64, Debug, SDL3 3.4.18 local, árvore com alterações preexistentes:

- `cmake --preset native-debug`: sucesso.
- `cmake --build --preset native-debug`: sucesso.
- `ctest --preset native-debug`: **109/109 aprovados**, zero falhas,
  **144,24 s** (64 sem dados opcionais + 45 com dados instalados).

Quatro novos CTests: regras de Won e três percursos completos, com carga pelo
aplicativo em outro processo. Chuva agora tem teste de pixel com a paleta e
iluminação originais e movimento do personagem/câmera. Capturas de menu, descrição,
quantidade, repolho/abacaxi/pimentão maduros e chuva inspecionadas. A suíte anterior
incluindo save v1/v2 e lojas sazonais continua coberta. Build com `-Werror` em
produção; avisos const antigos da fixture de mapa não foram alterados.

Não executados: Windows/cross-build, Release, sanitizers, Android, sessão manual
de janela/áudio e instalação limpa. BIN preservado; PNG adiado. Formato v2 não
mudou. Detalhes, origem dos textos e limites em [PF06_WON_SEEDS.md](PF06_WON_SEEDS.md).

CI configurada não equivale a execução comprovada. Os resultados não demonstram
jogo completo ou fidelidade integral ao original.

## Histórico das entregas MA

As próximas etapas citadas nos registros abaixo pertencem ao plano anterior.
A correspondência MA → PF e a sequência ativa estão somente no roadmap.

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

Ao encerrar 04a, MA-04 continuava aberta para resolver execução e atores.
Esse limite foi tratado no recorte 04b abaixo. Tick, serviços e separação gráfica
continuam em MA-05..08; ciclo de vida sem SDL não significa gameplay completo sem vídeo.

## Entrega MA-04b e limites

`HmbtnSession` agora possui execução/bytecode e atores de abertura, visitas,
conversas e eventos de animais, gatilhos e casa. Colisão, alvos, zonas e desenho
selecionam a mesma identidade ativa. A rota e sua animação ficam suspensas durante
o override e retomam a agenda ao liberar o vínculo, sem copiar de volta posição.
Dados locais da infância continuam separados das identidades dos moradores.

Os dois testes novos verificam bytes/cursor próprios, waits, rejeição sem perda
da execução anterior, atores e sessões independentes, rebind de recursos e UI
durante evento ativo, destruição de outra sessão e retomada de uma rota real.
Os 72 testes anteriores passaram. Detalhes em [SESSION_EVENTS.md](SESSION_EVENTS.md).
**MA-04 concluída.** Naquela entrega a próxima prevista era MA-05a; a
solicitação seguinte autorizou MA-05 inteira, concluída abaixo.
Callbacks específicos e agendamento continuam na cena até MA-05..08.
Não foram alterados o interpretador, regras de inventário/calendário/economia,
formato de save (ausente), instalação ou código em `src/`.

## Entrega MA-05 integral e limites

05a/b concluídas juntas, conforme solicitação. `HmbtnSession.services` possui
pedidos/resultados com tickets, estados explícitos, aquisição única, cancelamento,
rebind com invalidação e buffers próprios. Entrada lógica neutra, recursos
lógicos, mensagens de eventos/conversas, animações/ações e música/XA usam os
backends existentes; espera/erro não concluem comandos. A aplicação de um
resultado requer ack antes de liberar a instrução que aguarda recursos/áudio.

Três testes novos verificam respostas imediatas/atrasadas, erro, cancelamento,
polling duplicado, ownership e efeito único em inventário/economia. Foram
exercitados leitores, arquivos de ações/textos e áudio/XA reais. Os 74 anteriores
continuam aprovados. Detalhes e arquivos em [SERVICE_CONTRACTS.md](SERVICE_CONTRACTS.md).
Na entrega MA-05 a próxima etapa era MA-06; ela foi executada integralmente
na solicitação seguinte. A separação completa de update/render continua para MA-07/08. Preservados `src/`,
instalação pelo BIN, algoritmos e alterações locais anteriores.

## Entrega MA-06 integral e limites

06a/b/c concluídas juntas. `game/exploration.*` possui o tick público de
exploração e as mesmas fases de controle, mundo e animação usadas pelo adapter
atual. Estado de controle/ações/carry/feed/gather/pesca/menus pertence à sessão.
Movimento, ações e relógio não dependem de desenho; recursos decodificados são
emprestados e a apresentação pode ser omitida. O teste sem SDL confirmou libc/
libm como únicas bibliotecas dinâmicas; o percurso com dados reais compara
três sessões a zero/uma/duas apresentações por tick, com conclusão de ferramenta,
pausa e dados equivalentes. Os 77 testes anteriores continuam aprovados.

**Situação ao encerrar MA-06:** MA-07 ainda não iniciada. Na revisão de direção,
essa fila foi substituída pelas entregas PF; PF-01 foi concluída, próxima PF-02. Portas, viagens, sono e roteiros continuam
no adapter; um tick público de exploração recusa esses estados pendentes. A cena
ainda chama as fases na sequência anterior durante seu coordenador, que pode
ser dirigido por draw, até MA-07/08. Nenhuma promessa de jogo inteiro headless,
save em disco ou paridade completa. Detalhes e arquivos em
[EXPLORATION_TICK.md](EXPLORATION_TICK.md).

## Entrega PF-01 e limites

`economy.c` adapta `estate_gold_change` para todos os débitos/créditos atuais.
`inventory.c` conserva/reúne os algoritmos de consulta/remoção, armazenamento,
compactação e troca/ciclo; conversas e medicina compartilham armazenamento,
coleta/campo/ovos compartilham colocação na mão. Sem novo estado paralelo.
O ciclo rápido respeita a capacidade e preserva registros inativos. Teto de
saldo pode ser configurado pela sessão; os valores base continuam os mesmos.

O reaproveitamento é por adaptação em `port/`, sem compilar módulos originais.
Catálogo, arrays e layout visual ainda têm os tamanhos atuais, não há novos
conteúdos ou coop habilitados. Fontes, diferenças e lista completa de arquivos
em [PF01_SHARED_RULES.md](PF01_SHARED_RULES.md). Próximo percurso: cultivo completo
PF-02, não iniciado nesta entrega.

## Entrega PF-02 e limites

Nabo tem ciclo completo: compra do pacote por 120 G na mesa de sementes do supermercado original,
plantio por ANI/grade original, rega e quatro dias regados, broto/maturidade com
sprites originais, colheita/carry e venda por 60 G. Regras e estado são os mesmos
usados por menus/remessas/eventos. `daily.c` centraliza o observer e preparação
de manhã; o campo não é seco outra vez pela cena. Alimentação/ovos/limpeza e
contadores atuais foram preservados, com testes de datas e efeitos únicos.

`native_crop_daily` cobre também dias secos, estação/inverno/ano, rebrota do
pepino e nove alvos. `native_seed_shop` cobre seleção/quantidade, falas/bancos,
cancelamentos e compra sem duplicação/cobrança indevida. O percurso com dados
usa ANI real e portas/cama/caixa, sem forçar conclusão de animação. Capturas locais
foram inspecionadas. Naquela entrega não foram declarados saves, clima completo,
grama, estufa, outras culturas ou lojas completas. Detalhes em [PF02_CROP_CYCLE.md](PF02_CROP_CYCLE.md).
Na entrega PF-02, a próxima etapa era PF-03. Ela foi concluída posteriormente,
conforme o registro PF-03 abaixo; PF-04 também foi concluída e a fila atual está no recorte PF-06 do roadmap.

Correção da PF-02: compra agora seleciona o grupo de primavera na mesa central
pela zona 37 e pela posição/chave original do registro, com sprites sazonais.
A zona 36 de entrega não abre a loja; os demais grupos não vendem nabo por engano.
Os testes da compra foram corrigidos e ampliados; fontes e limites no relatório PF-02.

Validação da correção da mesa: configuração/build Linux aprovados e **83/83
CTest**, zero falhas, **51,22 s**. Captura dos sacos na mesa antes da compra
inspecionada; Windows/Release/sanitizers e sessão manual continuam não executados.


Correção do menu/falas — 2026-10-26: lista somente com os quatro nomes base
de primavera, seleção do item e quantidade de 1 a 99. Falas e retratos usam
os bancos/IDs de `shop_result_request`/`shop_buy_stored_item`; descrição somente
por Triângulo. Lotes usam a regra compartilhada de empilhamento/envio inteiro
ao baú. O percurso com BIN compra três pacotes por 360 G e preserva dois após
plantar um; verifica cancelamento, recusas e mensagem 3:0x207. Morango,
outras estações e cultivo completo das demais espécies continuam pendentes.

Validação atual: configuração/build `native-debug` aprovados; **84/84 CTests**,
zero falhas, **51,58 s** (55 independentes dos dados opcionais + 29 percursos
com `data/`). A asserção adicional de preservação dos dois pacotes após todas
as noites/transições passou no percurso isolado após a suíte. Capturas da
abertura, lista, descrição, quantidade, compra e aviso de baú inspecionadas com
SDL dummy/software. Windows/Release/sanitizers, instalação limpa e sessão
manual com janela/áudio reais não executados.


## Entrega PF-03 — 2026-10-26

Evento 140, convite de Duke para a vindima, jogável com gatilho original no
ano 1/outono/dia 14/06h–12h e os dois ramos. Bytecode, falas 1:418..421,
ANI/texturas e predicados originais; aceitar define a flag 205 e conserva as
duas alterações de amizade do roteiro. Identidade ativa compartilhada, controles
restaurados e ausência de repetição ao trocar de área.

`event_variables.c` centraliza get/update, aritmética, clamps e comparações dos
comandos 13..22 usados por conversas/visitas/slots da abertura. Testes novos:
`native_event_variables` e `native_vineyard_invitation_playthrough`. Configuração,
build e **86/86 testes aprovados**, zero falhas, **52,10 s** (56 sem os dados
opcionais + 30 percursos com `data/`). Capturas inspecionadas via SDL software.
As verificações de Windows/Release/sanitizers e janela/áudio manual não foram
executadas. Detalhes em [PF03_REUSABLE_EVENTS.md](PF03_REUSABLE_EVENTS.md).
Vindima/Cliff, demais comandos/eventos e sistemas de variáveis ausentes continuam
em PF-06; saves em PF-05. Naquela entrega, coordenação global/testes privados
aguardavam PF-04, concluída posteriormente conforme o registro abaixo.


## Entrega PF-04 — 2026-10-26

`game/runtime.c` concentra o tick público de roteiros, conversas, portas/viagens,
sono/manhã e fases livres. Backends/recursos continuam na cena, sem cópias novas
de inventário ou progressão. Callbacks persistentes recebem a sessão; empréstimos
de recursos são válidos durante o tick e renovados após cargas.

`core/app.c` agenda entrada/ticks e apresentação separadamente. Título/cadastro
possuem tick/draw distintos; sorteios, animações e timers não avançam ao desenhar.
Percursos existentes usam APIs públicas, sem inclusão de `.c` de produção.
Os três novos testes cobrem o acumulador e comparações de 0/1/2 draws por tick,
Duke nos dois ramos, portas, sono/daily/remessa, cadastro/abertura e falha de área.

Configuração/build aprovados; **89/89 CTests**, zero falhas, **113,53 s**, com
recursos locais. Build final sem avisos e repetição isolada da fixture ajustada:
**1/1 aprovado**, 3,97 s. Critérios da PF-04 atendidos no recorte implementado.
Não executados Windows/cross-build, Release, Android, sanitizadores, instalação
limpa ou verificação manual em janela/áudio reais. Preservados código original,
infraestrutura PS1 e alterações preexistentes fora do escopo. Fontes, arquivos e
limites em [PF04_UPDATE_PRESENTATION.md](PF04_UPDATE_PRESENTATION.md).
Naquela entrega, a próxima era PF-05; sua conclusão está registrada abaixo.


## Entrega PF-05 — 2026-10-26

Codec nativo versionado em `game/save.c`, arquivo em `filesystem/save_file.c`,
restauração em cena candidata e integração de dormir/salvar, Continue e Read diary.
O slot padrão fica nas preferências do usuário, sem recursos proprietários.
`--save-file` seleciona arquivo independente; `--load-save` retoma diretamente.

O estado autoritativo continua na sessão; decode constrói outra sessão sem copiar
structs/ponteiros. Carga reconstitui recursos e mantém flags/rotas/calendário/
remessa, sem disparar entrada de área nem aplicar novamente a atualização diária.
Eventos/ações/menus/transições em andamento bloqueiam salvar; a política foi
registrada antes da implementação. Versões desconhecidas/corrupção são erro;
futuras versões precisam de migração explícita. Não há importação de memory card.

Configuração/build aprovados; **92/92 CTests**, zero falhas, **123,82 s**. Três
novos testes, incluindo processos reais e Continue no loop de `app.c` com arquivo
existente/ausente; cama, campo/cochos/animais, pagamento único e Duke sem repetição.
Critérios atendidos para exploração estável. Windows/cross-build (toolchain
indisponível), Release, Android, sanitizadores, instalação limpa e janela/áudio
reais não executados. Quatro avisos const preexistentes em uma fixture de mapa;
código original e mudanças fora do escopo preservados. Relatório e arquivos em
[PF05_NATIVE_SAVES.md](PF05_NATIVE_SAVES.md). Naquela entrega, a próxima era
PF-06/batata, concluída no registro abaixo; a cobertura geral segue parcial.


## PF-06 — Batata — 2026-10-26

Recorte concluído; PF-06 permanece aberta. O catálogo e o motor adaptado do
original já forneciam as regras. Produção muda apenas o filtro para aceitar
0x23 e o sprite para 8. Loja, plantio, grade, crescimento 3 + 4, secagem, colheita,
inventário/remessa/pagamento e saves reutilizam as implementações existentes.

O mesmo driver de nabo/batata cobre compra de três pacotes por 450 G na mesa,
noite seca, sete dias regados, colheita 0x42 e pagamento único de 80 G. Salva no
estágio 1 com um dia regado, destrói/recria a sessão e continua o cultivo;
outro processo também abre o save. Regras de estação/inverno, grade, consumo
único e regressões de nabo/pepino passaram.

Configuração/build aprovados; **93/93 CTests**, zero falhas, **124,82 s**.
Direcionados **4/4**, 1,43 s. Capturas de quantidade/planta madura inspecionadas;
nenhum recurso adicionado ao Git. Sem Windows/cross-build, Release, Android,
sanitizadores, instalação limpa ou janela/áudio reais. Original e alterações
anteriores preservados. Relatório: [PF06_POTATO.md](PF06_POTATO.md).
Naquela entrega, o próximo recorte era grama/feno, concluído no registro abaixo.


## PF-06 — Grama/feno — 2026-10-26

Recorte concluído; PF-06 permanece aberta. Adaptações de `field_daily_tick`,
`field_tile_harvest` e `field_grown_cell_request` usam o campo atual e produzem
feno em `inventory.supplies[0]`, já usado pelo celeiro. Compra na mesa, ANI/grade,
onze noites sem rega, corte único, rebrota, capacidade 999 e inverno/primavera
foram protegidos. Formas/UV são derivadas na apresentação; efeito de corte usa
ANI original 14/19. Save v1 não mudou, com carga intermediária e outro processo.

Configuração/build aprovados; **95/95 CTests**, zero falhas, **124,59 s**.
Após ampliar fixtures sazonais, **9/9 direcionados**, 2,66 s. Capturas de
crescimento, maturidade, corte/outono e inverno inspecionadas com SDL dummy.
Arquivos e limites em [PF06_GRASS.md](PF06_GRASS.md). Naquela entrega, o próximo
recorte era **culturas base de verão**, concluído abaixo. PNG segue adiado.


## PF-06 — Culturas base de verão — 2026-10-26

Recorte concluído; PF-06 permanece aberta. Menu base do grupo B por chave
0x129/registro 113, saudação 2:0x273 e nomes/descrições extraídos do original.
Tomate, milho e cebola usam o mesmo campo/catálogo/ciclo diário, colheita/remessa,
inventário, economia e save v1. Rebrota/skip-day de tomate/milho e murchamento
no outono protegidos. Nenhuma liberação artificial de abóbora.

Carga sazonal recompõe os bancos da própria área: corrigida a dependência de
pixels deixados pela primavera, encontrada ao destruir a cena e carregar um
save de verão. **99/99 CTests**, zero falhas, **128,11 s**; dez direcionados
**10/10**, 5,90 s, capturas inspecionadas e preview executado. Fontes, arquivos e
limites em [PF06_SUMMER_CROPS.md](PF06_SUMMER_CROPS.md). Naquela entrega, o próximo
recorte era **culturas base de outono**, concluído abaixo.


## PF-06 — Culturas base de outono — 2026-10-26

Recorte concluído; PF-06 permanece aberta. Grupo C base, chave 0x12A/registro
114, saudação 2:0x274 e nomes/descrições extraídos do original. Berinjela,
cenoura e batata-doce usam campo/daily/ações/colheita/remessas e save v1 atuais.
Rebrota nos estágios 2/1, moldura ajustada ao maior nome e ausência de cultivos
no inverno protegidas. Espinafre continua pendente do grupo C/pimentão.

**103/103 CTests**, zero falhas, **134,40 s**, após ajuste visual;
**14/14 direcionados**, 10,68 s. Inclui crescimento/rebrota, sono Outono 30→Inverno 1,
saves comparados e abertura de crescimento e inverno em processos novos.
Relatório: [PF06_FALL_CROPS.md](PF06_FALL_CROPS.md). Próximo recorte:
**clima diário e chuva no campo**; esse recorte foi concluído depois, conforme seção a seguir.


## Entrega PF-06 — Clima diário e chuva

Previsão/atualização diárias, calendário e aniversários, rega de manhã/enxada,
estado central, consumidores e apresentação básica dos cinco tipos integrados.
`HmbtnGameState.weather` é o proprietário; RNG persistente em save v2, com
migração de v1 sem reroll. **105/105 CTests**, zero falhas, 134,14 s.
Danos de tempestade, família e meteorologia integral continuam pendentes;
PF-06 permanece aberta. Próximo recorte: sementes de Won, conforme o roadmap.


## Entrega PF-06 — Won e apresentação da chuva

Loja pela conversa/comando original; três sementes sazonais e seus ciclos,
rebrotas e remessas integrados à implementação compartilhada. Grupo D completo
continua parcial (3/8). Chuva recebe luz após os sprites e deslocamento projetado
da câmera no tick, com testes de pixel, movimento e draw sem mutação.
**109/109 CTests**, zero falhas, 144,24 s. Próximo recorte: desbloqueios do
supermercado; PF-06 permanece aberta. Nomes de menus são compilados das tabelas C;
falas/descrições são lidas do BIN instalado, portanto tradução ainda é mista.


## Correção PF-06 — Chuva e portas

Áudio da chuva integrado pelo seletor original: faixa 22 substitui a música
normal; volume, retomada, manhã e viagens usam o clima compartilhado. Portas
precedem o arremesso com itens, inclusive nas recusas. Recuo/tempos ajustados
pelo usuário preservados. Moradores acionam portas comuns pelas células de
suas rotas, esperam a abertura, passam e deixam fechar com os efeitos originais.
Save v2 conserva inventário/clima/rotas; a animação transitória da porta pode
reiniciar após carregar. Eventos especiais de portas continuam parciais.

`cmake --preset native-debug`, build nativo e **111/111 CTests** passaram,
zero falhas, **136,90 s**, Linux Debug, SDL3 3.4.18. PCM real validado até o loop
da chuva (~186 s de áudio), percursos instalados e capturas software de portas.
O teste antigo de interiores foi atualizado para verificar o recuo atual.
Windows, Release, sanitizers, dispositivo de áudio/janela interativa e instalação
limpa não foram validados nesta correção. Código original e alterações anteriores
preservados. Detalhes e arquivos em [PF06_RAIN_DOORS.md](PF06_RAIN_DOORS.md).
A próxima entrega permanece **PF-06 — desbloqueios do supermercado** no roadmap.


## PF-06 — Correções visuais e sementes desbloqueáveis

Câmera limitada durante portas e item carregado sujeito à oclusão do mundo,
preservando recuo/tempos do usuário. Morango, abóbora e espinafre liberados pelas
remessas originais (101 de cada produto do grupo), com cultivo/colheita/rebrota
prevista e saves, usando os sistemas existentes. Nenhuma nova flag persistida.

Configuração/build Linux Debug passaram; seleção final **20/20 CTests**, zero
falhas, **17,85 s**. Caso real adicional de câmera no limite e capturas passaram
em execução direta. Foram 48 processos de teste/verificação incluindo wrappers,
repetições e cargas filhas, respeitando o máximo de 50. **Não foi executada a
suíte completa de 115 testes.** Windows, Release, sanitizers, janela/áudio físicos
e instalação limpa não validados nesta rodada. Detalhes, fontes e arquivos em
[PF06_CAMERA_SEED_UNLOCKS.md](PF06_CAMERA_SEED_UNLOCKS.md).
Próximo recorte: flores simples de Won, conforme o roadmap. PF-06 permanece aberta.


## PF-06 — Pink Cat e Toy Flower — 2026-10-27

Recorte concluído com cinco entradas funcionais de Won entre oito originais.
As duas flores usam os motores/estados existentes; preços, estações, estágios e
produtos vêm das tabelas originais. Sem rebrota ou preço de remessa. Foram
validados compra/descrições, plantio/rega/noite seca, colheita, mão/mochila,
presente para Lillia pela agenda/conversa reais, murchamento e saves v2.

Configuração e build Linux Debug passaram. A seleção de 18 CTests teve 17
aprovações e uma expectativa antiga que ainda tratava morango como não suportado.
O teste passou após corrigir essa expectativa e manter Moondrop como caso
negativo; a repetição foi isolada em `native_crop_daily`. Dois percursos diretos
adicionais geraram capturas inspecionadas das flores maduras e do menu.
Os 18 alvos selecionados estão validados; não houve uma nova rodada completa.
Total conservador de 44 processos nesta entrega, contando wrappers/filhos,
dentro do limite 50. A suíte completa agora registra 118 testes e não foi rodada.
Windows, Release, sanitizers, janela/áudio físicos e instalação limpa não foram
verificados. Próximo: Magic Red; PF-06 permanece aberta. Relatório e arquivos em
[PF06_FLOWERS.md](PF06_FLOWERS.md).


## PF-06 — Magic Red — 2026-10-27

Recorte concluído: semente de 600 G em Won, variantes azul/vermelha gravadas no
plantio, dez dias regados no outono, sprites originais e coleta sem rebrota.
Azul usa mochila/presentes; vermelha pode ser enviada por 200 G. Sem motores
paralelos; sorteio por célula usa estado autoritativo do campo e o helper original.
Save atual v3 persiste esse RNG, mantendo leitura v1/v2 sem alterar seus campos.
Moondrop/Orangecup continuam pendentes; PF-06 permanece aberta.

Configuração/build Linux Debug passaram. Seleção final: **18/18 CTests**, zero
falhas, **7,25 s**. Os dois novos percursos passaram também na seleção inicial
com carga pelo aplicativo em outro processo e em execuções diretas finais com
capturas. Total de 20 alvos distintos validados. Um teste novo inicialmente
supunha a reação de Lillia igual à das flores anteriores; foi corrigido para o
valor 0 confirmado em `picture_codes.c` e passou na repetição isolada/final.
Foram **44 processos** contando repetições/wrappers/filhos, abaixo do máximo 50.
Suíte completa de 121 testes, Windows, Release, sanitizers, janela/áudio físicos
e instalação limpa não executados. Próximo: Moondrop e seu desbloqueio por evento,
conforme [ROADMAP.md](ROADMAP.md). Fontes e arquivos em [PF06_MAGIC_RED.md](PF06_MAGIC_RED.md).


## PF-06 — Moondrop e Karen — 2026-10-27

Recorte concluído, incluindo a dependência descoberta no original: visita 18
fornece o primeiro pacote (afeição 15.000 e espaço acessível); visita 19 exige
flor madura e libera a loja na chegada. Roteiros/arte/falas/música originais,
mesmo interpretador e inventário. Desbloqueio único em `game.progress`,
emprestado por casa/loja; save v4 lê v1/v2/v3 sem inferir progresso de outro flag.
Compra 300 G, cultivo em seis manhãs regadas, coleta/mochila/presente, ambos os
ramos de espera, ausência de repetição e retomada em outro processo validados.

Configuração/build Linux Debug passaram. Seleção de 20/20 CTests em 22,41 s;
após corrigir o modo musical retido entre visitas, repetição do percurso afetado:
1/1 em 2,50 s. Foram 48 processos efetivamente executados, incluindo repetições,
wrappers e filhos; uma tentativa anterior ocorreu antes de o binário estar pronto.
49 tentativas no total, respeitando o máximo 50. Capturas de pacote, flor e menu
inspecionadas. Suíte completa de 122 testes, Windows, Release, sanitizers,
janela/áudio físicos e instalação limpa não executados. PF-06 permanece aberta.
Próximo: estufa/Orangecup, conforme o [roadmap](ROADMAP.md). Detalhes e arquivos
em [PF06_MOONDROP.md](PF06_MOONDROP.md).


## PF-06 — Carpintaria e galinheiro — 2026-10-27

Primeira dependência da cadeia da estufa entregue: loja original de Gotz,
madeira, pedido de 5.000 G/420 madeiras, quatro manhãs e galinheiro ampliado real.
Sessão possui o prazo; `poultry.upgraded` possui o nível; dinheiro/madeira são
os mesmos consumidos por todos os sistemas. Flags e status derivam desse estado.
Save v5 preserva obra/melhoria; versões antigas não são mais migradas, conforme
solicitação do usuário. Primeira casa é a próxima entrega do roadmap.

CMake/configuração e build Linux Debug passaram. Seleção intermediária 13/13;
seleção final **14/16 CTests em 67,42 s**. Os dois testes de portas falharam por
esperarem incremento 5 enquanto a edição atual de `runtime.c` usa 8; a alteração,
feita durante esta rodada, foi preservada. Carpintaria, saves/Continue em outro
processo, Moondrop, sessão/calendário/inventário, conversas, feed e primeira manhã
passaram. **49 processos** incluindo repetições/wrappers/filhos; limite 50.
Capturas de menu, fala de custo e interior inspecionadas. Suíte completa de 124,
Windows, Release, sanitizers, instalação limpa e janela/áudio físicos não
executados. Detalhes e lista de arquivos em [PF06_CARPENTRY.md](PF06_CARPENTRY.md).


## PF-06 — TV Shopping e utensílios — 2026-10-29

Concluído o recorte de aquisição: programa de sábado após ampliação, oferta
assistida, telefone da hospedaria, taxa de ligação, confirmações/cancelamento,
preços do catálogo e nove entregas originais de Zack (oito utensílios e Power
Berry). Gatilhos BIN determinam a entrega, sem contador inventado. Aparelhos
aparecem nas posições das duas ampliações e liberam as opções da cozinha
existente. A faca entregue foi usada para preparar/registrar salada consumindo
o ingrediente; seu preço original de remessa zero foi respeitado.

Debug: configure/build, regressão 8/8 e wrapper 1/1 com retomadas em outros
processos. Release: configure/build e seleção 2/2. Windows x64: build local,
unidade/percurso sob Wine e retomadas pendente/concluída pelo aplicativo
aprovados. Percurso atualizado também repetido em Debug/Release/Wine para a
prioridade do telefone sobre descartar um item na mão. Capturas inspecionadas;
portas do usuário preservadas por comparação do bloco completo inicial.
Até 49 processos nesta rodada, contando tentativas, wrappers, filhos e
lançadores; nenhuma suíte inteira. Windows/janela/áudio físicos, instalação
limpa, sanitizers e verificação PS1 não executados.

Relatório: [PF06_TV_SHOPPING.md](PF06_TV_SHOPPING.md). Próximo: apresentação final
do preparo (ANI/som/tempo) e indicadores da cozinha/ficha. PF-06 permanece
aberta; o runtime é parcial e o bootstrap de diagnóstico não executa o jogo.


## PF-06 — Preparo e indicadores da cozinha — 2026-10-29

Entregue a sequência C/BIN de fechar confirmação, fade, consumo único,
faixa XA 22, aguardar áudio e revelar resultado. O original usa tela preta,
sem ANI de personagem cozinhando nesse fluxo. Painéis de utensílios/temperos,
receitas e faltas, quantidades e navegação foram adaptados; contagem na ficha
continua derivada. Escolhas de guardar resultado melhor/pior respeitam o fim
da mensagem. Nenhuma alteração de codec foi necessária.

Debug: regras 7/7, percurso com aquisição real e retomada em processos separados,
portas/chuva/mundo e regressão final das duas ampliações 2/2. Release: regras/XA
2/2, percurso e retomada aprovados. Windows x64/Wine: build, regras/XA, sete
preparos e retomada aprovados. PCM original não nulo até o fim e capturas dos
painéis/ficha inspecionados. Portas do usuário preservadas por comparação.
45 processos contabilizados incluindo tentativas/wrappers/filhos/lançadores;
sem suíte inteira, sanitizers, instalação limpa ou janela/áudio/Windows físicos.

[Relatório e reprodução](PF06_COOKING_PRESENTATION.md). Próximo: acessórios de
Saibara, usando loja/fila/minérios existentes. PF-06 permanece aberta; runtime
parcial e bootstrap de diagnóstico que não executa o jogo continuam explícitos.
