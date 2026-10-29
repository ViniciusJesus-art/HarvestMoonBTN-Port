# Pendências complementares ao roadmap

Atualizado em 2026-10-27. **A fila principal está no [ROADMAP.md](ROADMAP.md).**
MA-01..06 e PF-01..05 foram concluídas. PF-06 entregou batata, grama/feno e culturas base de verão/outono;
clima diário/chuva também está integrado; sementes sazonais de Won também integradas; desbloqueios do supermercado e flores simples também integrados; Magic Red e suas variantes também integradas; Moondrop e visitas de Karen também integradas; carpintaria/galinheiro e primeira casa/depósitos/núcleo da cozinha também entregues; obra/cochos do celeiro também entregues; aquisição/capacidade/alimentação bovina/ovina também entregues; segunda casa/estufa/Orangecup, ferramentas animais e venda com Barley também entregues; a próxima entrega é **PF-06 — reprodução bovina/ovina**. Correções de portas internas, chuva em portas/eventos, coleta nas bordas e descarte estão em [PF06_WORLD_FIDELITY.md](PF06_WORLD_FIDELITY.md). Nabo/batata/tomate/milho/cebola/berinjela/cenoura/batata-doce têm compra
até venda; grama tem corte/rebrota, estoque e cochos compartilhados.
A cadeia MA-07..25 foi substituída por entregas funcionais; a tabela de
correspondência está no roadmap. IDs MA nas classificações abaixo indicam o
escopo técnico anterior, não gates ativos. Funcionalidades podem começar com as
dependências reais de seu domínio, sem terminar toda a migração arquitetural.

As antigas 62 entradas eram consolidações históricas com sobreposições e limites
de etapas que depois avançaram: por exemplo, incubação, medicamento, transporte,
pesca, conversas e alguns interiores já têm implementações parciais. Não usar a
contagem antiga como número de funcionalidades ainda ausentes. Seus registros
permanecem em [ROADMAP_HISTORICO.md](ROADMAP_HISTORICO.md) e
[ROADMAP_CONCLUIDOS.md](ROADMAP_CONCLUIDOS.md). A classificação abaixo reconcilia
esses temas sem apagar o histórico nem declarar paridade completa.

## Regras para as próximas implementações

Seguir [PORTING_STRATEGY.md](PORTING_STRATEGY.md): reaproveitar C original,
adaptar em `port/`, manter `src/` intacto e justificar reimplementações. Preservar
SDL3/backends e o fluxo em que o usuário fornece seu BIN. Linux/Windows são os
alvos atuais; Não exigir compilação PS1, ABI/MIPS ou SHA-1.
Continuam necessárias leitura LE, endereços serializados de 32 bits e validação
dos recursos, sem `long` como inteiro PS1 ou ponteiros host vindos do disco.

Não ampliar estado/regra paralelos. Adaptar as dependências estritamente necessárias ao domínio
na mesma entrega funcional. Trabalho funcional novo deve ganhar tarefa pequena no roadmap,
com função de origem, dependências, testes e critério de conclusão.

## Correspondência do backlog anterior

| Tema anterior | Estado e encaminhamento |
|---|---|
| Inventário, ferramentas, mochila e menus | Já parciais; inventário e finanças centrais MA-01/02 concluídos; tick livre MA-06 concluído; apresentação MA-08, regras MA-15/16. Melhorias e ações ainda ausentes ficam para a próxima fila funcional. |
| Endereços fixos, estruturas host, inicialização e módulos nativos | Propriedade MA-01..04, contratos MA-05 e tick livre MA-06 concluídos; seguir MA-07/08, sem portar o `main.c` PS1 inteiro ou simular overlays MIPS. |
| Comandos, portas, diálogos, eventos e scripts completos | Os roteiros já cobertos são proteção; compartilhar os comandos necessários ao evento escolhido em PF-03, sem exigir toda a antiga MA-10..14. |
| Relógio, sono, calendário, cochos e eventos diários | Preservar recorte funcional; calendário/cadastro central MA-03 concluído, regras MA-18/19 e integração MA-20/21. |
| Plantações, colheita, grama e produção | Nabo completo em PF-02; batata, grama/feno e culturas base de verão/outono em PF-06; clima diário/chuva integrado; repolho/abacaxi/pimentão de Won integrados; demais culturas/clima completo/estufa em PF-06, sem esperar a antiga MA-20. |
| Galinhas, pintinhos, ovos, venda, medicina e transporte | Já parciais, não recomeçar do zero. Migrar regras em MA-21; concursos e espécies ausentes são trabalho posterior. |
| Interiores, agendas, presença e rotas | Já parciais; registros sob sessão em MA-04a; identidade de atores MA-04b concluída; separar execução/apresentação MA-07/08, seleção/movimento MA-22. |
| Conversas, presentes e afinidade | Já parciais; comandos comuns MA-10..14 e ganho/decay MA-23. Família/casamento não implementados por essa migração. |
| Caixa de venda, lojas e economia | Já parciais; propriedade central MA-02 concluída; regras MA-15..18. Cesta, cavalo, caixas adicionais e outras lojas dependem desses serviços e dos sistemas próprios. |
| Pesca e entrega da vara | Já parciais; preservar testes/roteiros e reconciliar MA-24, sem segundo sistema de pesca. |
| Geometria, áudio, formatos e apresentação | Backends preservados MA-05..08; diferenças de fidelidade continuam registradas, sem reescrever a infraestrutura como pré-requisito. |
| Instalação pelo BIN e pacotes | Fluxo já implementado parcialmente; preservar regressões de importação. Não refazer o instalador nem distribuir recursos proprietários. |

## Áreas abertas, detalhadas continuamente no roadmap

Estas são áreas abertas, não tarefas de implementação suficientemente pequenas.
Cada uma deve ganhar escopo, fontes e critérios no roadmap ao ser selecionada,
sem aguardar a antiga MA-25. Saves já têm entrega PF-05; demais sistemas PF-06,
distribuição PF-07. Conteúdos novos e coop são futuros e usam a mesma base,
sem exigir infraestrutura de rede agora.

| Área | Pré-requisitos e conclusão a definir por tarefa |
|---|---|
| Clima completo e demais callbacks diários | Pipeline MA-18/19, campo/animais; previsão/troca diária e irrigação entregues; faltam danos de tempestades, callbacks ausentes e meteorologia integral a partir do original. |
| Demais culturas, melhorias e ações/ferramentas | MA-15/16/20 e sessão sem desenho; regras originais, produtos e transições completas por recorte. |
| Vacas, ovelhas, cachorro, cavalo, ajudantes e concursos | Vacas/ovelhas: coleção/compra/nome/arte/capacidade/alimentação/crescimento/conversa/medicina/status/saves entregues no recorte base. Produção/venda/reprodução/pastagem/funerais pendentes; pets/ajudantes/concursos continuam separados. |
| Família, casamento e progressão de história | Comandos, flags, calendário e relacionamentos comuns; não usar valores iniciais fixos para simular progresso. |
| Festivais, minijogos, minas e serviços restantes | Inventário de módulos originais e contratos nativos; selecionar um roteiro/sistema e seu percurso verificável. |
| Saves host versionados | Estado central estável, sem dump de structs/ponteiros; testar ida/volta e corrupção da versão atual em Linux/Windows. Compatibilidade entre versões de desenvolvimento não é exigida; v8 rejeita versões diferentes claramente. Importação de save PS1 é decisão separada, não requisito implícito de compatibilidade binária. |
| Traduções com alterações de código | Identificar dados/scripts versus regras alteradas; definir compatibilidade sem assumir que substituir recursos basta. |
| Fidelidade gráfica, áudio e RNG | Comparações reproduzíveis de imagem/ritmo/afinação/volumes; registrar decisões de interpolação/reverb/sequência RNG sem exigir rebuild PS1. |
| Distribuição Linux/Windows | Validar instalação limpa, atualização, dados ausentes/corrompidos, cancelamento e janela/áudio em Windows real; executar workflow de pacotes com autorização adequada para publicação quando aplicável. |


Não há novo total agregado de pendências: tarefas MA, áreas futuras e registros
históricos têm granularidades diferentes. O próximo passo e o aceite de cada
entrega são os do roadmap, não o checklist de uma etapa antiga.


Atualização de 2026-10-26: desbloqueios do supermercado e correções de câmera/item
carregado integrados em [PF06_CAMERA_SEED_UNLOCKS.md](PF06_CAMERA_SEED_UNLOCKS.md).
A fila principal seleciona flores simples de Won a seguir. Referências antigas
a esses desbloqueios como pendentes são históricas; não iniciar implementação
duplicada. A validação por rodada respeita o limite de 50 execuções do usuário.


Atualização de 2026-10-27: Pink Cat e Toy Flower concluídas no mesmo motor de
Won/cultivo/inventário/presentes, com saves v2 preservados. A próxima entrega é
Magic Red e suas variantes; Moondrop/estufa seguem pendentes. Esta nota substitui
a seleção histórica acima; critérios e ordem estão somente no [ROADMAP.md](ROADMAP.md).


Atualização posterior de 2026-10-27: Magic Red concluída, com sorteio original
por célula e save v3 que lê v1/v2. Próxima: Moondrop e condição de progressão;
Orangecup depende da estufa. Esta nota atualiza a seleção histórica anterior;
seguir somente a ordem e os critérios do [ROADMAP.md](ROADMAP.md).


Atualização posterior de 2026-10-27: Moondrop e as visitas 18/19 de Karen
concluídas, com save v4 que lê v1..v3. A próxima entrega é estufa/Orangecup,
começando pelo percurso original de aquisição. Esta nota substitui a seleção
histórica anterior; ordem e aceite continuam somente no [ROADMAP.md](ROADMAP.md).


Atualização posterior de 2026-10-27: primeira casa, geladeira/armário e núcleo da
cozinha integrados com save v6 atual. Aquisição de utensílios pela TV e detalhes
de apresentação do preparo/ficha continuam pendentes; não repetir os algoritmos
já adaptados em `cooking.c`. Próxima entrega: celeiro, com auditoria de espécies,
capacidade e cochos, conforme o [ROADMAP.md](ROADMAP.md). Fonte/resultado:
[PF06_HOUSE_EXPANSION.md](PF06_HOUSE_EXPANSION.md).


Atualização posterior de 2026-10-27: obra do celeiro, mapas ampliados e cochos
adicionais integrados, save v7 atual. A criação bovina/ovina ainda não existe no
runtime nativo; aquisição/presença/alimentação reais são a próxima dependência
antes de concluir o item do celeiro/avançar para casa2. Não refazer obra/feno/loja
nem simular vagas/animais. Seguir critérios do [ROADMAP.md](ROADMAP.md) e fontes
em [PF06_BARN_EXPANSION.md](PF06_BARN_EXPANSION.md).


Atualização posterior de 2026-10-27: aquisição/presença/alimentação bovina e ovina
integradas em [PF06_LIVESTOCK.md](PF06_LIVESTOCK.md), save v8 atual. O requisito de
uso real do celeiro foi atendido no recorte base; próximos critérios de construção
são da segunda casa no [ROADMAP.md](ROADMAP.md). Venda/produção/reprodução,
pastagem/cues completas/vozes e funeral ainda precisam de integração, usando a
mesma coleção e ações. Não declarar criação integral nem duplicar seus estados.
