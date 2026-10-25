# Pendências complementares ao roadmap

Atualizado em 2026-10-25. **A fila principal está no [ROADMAP.md](ROADMAP.md).**
MA-01..03 e MA-04a foram concluídas; MA-04 continua aberta. A próxima tarefa
é MA-04b (execução e identidade dos atores de evento). Não selecionar uma funcionalidade antiga deste arquivo para
contornar as dependências da migração arquitetural.

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

Não ampliar estado/regra paralelos. Completar primeiro a migração do domínio
necessário. Trabalho funcional novo deve ganhar tarefa pequena no roadmap,
com função de origem, dependências, testes e critério de conclusão.

## Correspondência do backlog anterior

| Tema anterior | Estado e encaminhamento |
|---|---|
| Inventário, ferramentas, mochila e menus | Já parciais; inventário e finanças centrais MA-01/02 concluídos; separação MA-06/08, regras MA-15/16. Melhorias e ações ainda ausentes ficam para a próxima fila funcional. |
| Endereços fixos, estruturas host, inicialização e módulos nativos | Propriedade inicial MA-01..04a concluída; seguir MA-04b..08, sem portar o `main.c` PS1 inteiro ou simular overlays MIPS. |
| Comandos, portas, diálogos, eventos e scripts completos | Os roteiros já cobertos são proteção; consolidar MA-10..14 antes de ampliar eventos. |
| Relógio, sono, calendário, cochos e eventos diários | Preservar recorte funcional; calendário/cadastro central MA-03 concluído, regras MA-18/19 e integração MA-20/21. |
| Plantações, colheita, grama e produção | Ainda incompletas; primeiro cultivo completo MA-20, demais culturas/clima/estufa em tarefas posteriores rastreadas. |
| Galinhas, pintinhos, ovos, venda, medicina e transporte | Já parciais, não recomeçar do zero. Migrar regras em MA-21; concursos e espécies ausentes são trabalho posterior. |
| Interiores, agendas, presença e rotas | Já parciais; registros sob sessão em MA-04a; consolidar atores/execução MA-04b/07/08, seleção/movimento MA-22. |
| Conversas, presentes e afinidade | Já parciais; comandos comuns MA-10..14 e ganho/decay MA-23. Família/casamento não implementados por essa migração. |
| Caixa de venda, lojas e economia | Já parciais; propriedade central MA-02 concluída; regras MA-15..18. Cesta, cavalo, caixas adicionais e outras lojas dependem desses serviços e dos sistemas próprios. |
| Pesca e entrega da vara | Já parciais; preservar testes/roteiros e reconciliar MA-24, sem segundo sistema de pesca. |
| Geometria, áudio, formatos e apresentação | Backends preservados MA-05..08; diferenças de fidelidade continuam registradas, sem reescrever a infraestrutura como pré-requisito. |
| Instalação pelo BIN e pacotes | Fluxo já implementado parcialmente; preservar regressões de importação. Não refazer o instalador nem distribuir recursos proprietários. |

## Entregas posteriores, a detalhar no roadmap em MA-25

Estas são áreas abertas, não tarefas de implementação suficientemente pequenas.
Cada uma deve ser decomposta e inventariada antes de começar.

| Área | Pré-requisitos e conclusão a definir por tarefa |
|---|---|
| Clima completo e demais callbacks diários | Pipeline MA-18/19, campo/animais; portar previsões, tempestades e consequências a partir do original. |
| Demais culturas, melhorias e ações/ferramentas | MA-15/16/20 e sessão sem desenho; regras originais, produtos e transições completas por recorte. |
| Vacas, ovelhas, cachorro, cavalo, ajudantes e concursos | Estado/identidade e ciclo diário migrados; adaptar cada espécie/sistema original separadamente. |
| Família, casamento e progressão de história | Comandos, flags, calendário e relacionamentos comuns; não usar valores iniciais fixos para simular progresso. |
| Festivais, minijogos, minas e serviços restantes | Inventário de módulos originais e contratos nativos; selecionar um roteiro/sistema e seu percurso verificável. |
| Saves host versionados | Estado central estável, sem dump de structs/ponteiros; definir o que se salva e testar ida/volta, corrupção e versões em Linux/Windows. Importação de save PS1 é decisão separada, não requisito implícito de compatibilidade binária. |
| Traduções com alterações de código | Identificar dados/scripts versus regras alteradas; definir compatibilidade sem assumir que substituir recursos basta. |
| Fidelidade gráfica, áudio e RNG | Comparações reproduzíveis de imagem/ritmo/afinação/volumes; registrar decisões de interpolação/reverb/sequência RNG sem exigir rebuild PS1. |
| Distribuição Linux/Windows | Validar instalação limpa, atualização, dados ausentes/corrompidos, cancelamento e janela/áudio em Windows real; executar workflow de pacotes com autorização adequada para publicação quando aplicável. |


Não há novo total agregado de pendências: tarefas MA, áreas futuras e registros
históricos têm granularidades diferentes. O próximo passo e o aceite de cada
entrega são os do roadmap, não o checklist de uma etapa antiga.
