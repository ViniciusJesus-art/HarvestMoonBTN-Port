# PF-05 — Saves nativos e retomada

Política definida em 2026-10-26, antes da implementação. Próxima fila e aceite
continuam no [ROADMAP.md](ROADMAP.md). Alvos Linux/Windows; Android depois.

**Atualização de política — 2026-10-27:** o usuário dispensou compatibilidade
entre saves durante o desenvolvimento. O codec atual v10 aceita somente v10;
versões antigas/futuras são recusadas com indicação de jogo novo. Os detalhes
v1 abaixo registram a entrega original. Migradores não são requisito atual;
integridade, publicação transacional e retomada da versão vigente continuam
obrigatórias. Ver [PORTING_STRATEGY.md](PORTING_STRATEGY.md) e
[PF06_LIVESTOCK.md](PF06_LIVESTOCK.md).

## Política de salvamento

O save representa uma sessão em exploração livre, entre ticks, depois da abertura,
com recursos associados e sem evento, diálogo, menu, ação, salto, arremesso,
transporte de animal, viagem, porta, sono ou fade em andamento. Esses estados
transitórios bloqueiam salvar com motivo explícito; não são descartados em
silêncio nem serializados parcialmente. Objetos normais na mão são inventário
persistente. Nenhum ponteiro, callback, ticket, áudio ou textura vai para o arquivo.

“Dormir e salvar” usa o menu já existente e grava depois da atualização diária e
da conclusão do fade da manhã, antes do próximo tick de gameplay. A solicitação
não é considerada gravada antes da confirmação do host. O carregamento recria
recursos e associações em uma cena candidata; só substitui a sessão atual depois
de validar tudo. Uma falha preserva a sessão em uso e o save anterior.

O título “Continue” e a opção de carregar do diário usam o mesmo carregador.
O slot nativo padrão fica na pasta de preferências do usuário, separado dos
recursos importados. Caminho explícito permite saves independentes e testes de
reinício de processo. Não recriar seleção/formatação/transferência de memory card.

## Origem e diferenças deliberadas

`src/overlay/15_saveload/saveload.c`: `save_pack_farm_state`,
`save_unpack_farm_state`, `save_image_checksum_write/verify` e fluxo de confirmação.
`src/overlay/11_GF_FARM/world/clock_events.c`: `farm_overnight_finish` salva após
processar a noite. `src/overlay/27_GF_TITLE/title.c`: destino Continue do título.

O original copia imagens de estruturas, usa endereços/globais, blocos de memory
card e checksum do formato PS1. Esse transporte não serve ao host. Reaproveitar
a seleção conceitual dos dados e os momentos de salvar/carregar; manter as regras
de inventário, economia, calendário e processamento diário já adaptadas.
O codec/arquivo nativo precisa de implementação própria, com campos explícitos,
versão, tamanhos, IDs lógicos e validação. Não interpretar dumps PS1 como saves PC.

## Dados persistentes e retomada

Cobrir inventário completo/capacidades/suprimentos, economia/remessa/ledger,
calendário/cadastro, células/crescimento do campo, cochos/coleta diária,
galinhas/ovos/incubação/saúde/posições, moradores/agendas/rotas/afinidade/flags/
contadores diários, RNGs atuais, stamina/fadiga/experiência, estatísticas,
configurações, área/posição/direção e marcos de primeira visita. Conservar a chave
idempotente de atualização diária e o estado da remessa para não executar de novo.

Geometria, quads do campo e recursos são reconstruídos; registros de moradores e
flags não passam pela inicialização de novo jogo. O carregamento de recursos não
pode ser tratado como entrada nova na área para disparar eventos ou limpar flags.
Dados de conteúdo usam IDs lógicos estáveis; contagens da versão inicial descrevem
as estruturas nativas existentes, sem transformar o layout PS1 em contrato futuro.
Na política inicial, novas versões migrariam esses campos. Essa exigência foi retirada em 2026-10-27; o desenvolvimento pode exigir jogo novo.

## Implementação e formato v1

- `port/game/save.c/.h`: disponibilidade, codec e validação de domínio, sem SDL
  ou arquivos. O decode aloca uma sessão não movível através do construtor;
  inventário, economia e demais vínculos apontam para seus próprios membros.
- `port/filesystem/save_file.c/.h`: caminhos UTF-8, leitura limitada, escrita
  temporária no mesmo diretório, flush/close e publicação por rename com
  substituição. O destino não é truncado antes da publicação. Temporários são
  retirados em falhas normais. Metadados do diretório após queda de energia e
  múltiplos processos escrevendo o mesmo slot simultaneamente não têm garantia
  adicional; usar caminhos distintos para sessões independentes.
- `port/graphics/scene_preview.c`: `hmbtn_scene_saved_create` recria recursos e
  associa agendas sem executar inicialização de novo jogo ou entrada de área.
  Restaurar não limpa flag 0x5E, streak de pesca ou marcos de visitas. O candidato
  valida o primeiro desenho/ANI antes de ser publicado pelo host.
- `port/core/app.c`: processa pedido de salvar entre ticks, depois do fade da
  manhã; sucesso só é anunciado após a escrita. Falha mantém jogo/save anterior
  e informa o erro. O pedido é consumido uma vez; uma nova escolha de dormir e
  salvar permite tentar novamente. Continue/diário trocam a cena somente após
  carga completa; erro no título retorna ao menu e erro no diário conserva a
  sessão aberta. Tempo de I/O/modal não vira avanço acumulado do jogo.
- `port/graphics/title_preview.c/.h`: destino Continue disponível para o host e
  retorno explícito ao menu em falha. `game/title_flow.c` conserva seu algoritmo.
- `port/game/exploration.c`, `runtime.c`, `session.h`: solicitação de carga do
  diário e de save da manhã, sem I/O de arquivos no domínio. O diário usa a
  terceira opção original; troca de animais continua ausente.
- `port/core/main.c` e `port/core/app.h`: `--save-file FILE` seleciona um slot; `--load-save FILE`
  inicia carregando esse slot. As opções de preview existentes continuam sendo
  sessões novas; `--load-save` não pode ser combinado com essas entradas.

Cabeçalho: oito bytes `HMBTNSV\0`, versão u32, tamanho do payload u32, quantidade
de seções u32 e CRC-32 do payload u32. Inteiros são little-endian por definição
do formato nativo, sem relação com ABI PS1. Seções `GAME`, `FILD`, `VILL`, `POUL`,
`PLAY`, `AUXI` têm ID e tamanho explícitos. Todos os escalares são escritos por
campo; bool é 0/1, signed tem conversão definida, nomes mantêm os glyphs originais
em valores numéricos e IDs de itens/moradores/áreas não são offsets de arquivo.
`GAME` identifica o catálogo base por ID 1. Contagens/IDs são verificadas antes
de aceitar registros; a ordem das seis seções pode variar, mas ausência,
duplicação, tamanho divergente ou seção desconhecida são erro em v1.

Versão desconhecida é rejeitada com motivo explícito. Ao adicionar conteúdo ou
alterar o esquema, implementar migração por versão antes de aceitar os novos
arquivos; não reler dados antigos usando o tamanho/layout atual de uma struct.
O contrato v1 descreve as capacidades implementadas hoje, não uma promessa de
compatibilidade com mods ainda inexistentes. Limites de leitura/validação não
são limites de memória ou de execução herdados do PS1.

A retomada conserva também contadores que afetam ações futuras: golpes/alvo de
ferramenta, frames do relógio, RNGs, contador de repouso/rotação dos controles,
recusa de zona e interpolação de luz. Campo/rotas/animais continuam usando os
mesmos algoritmos. Tickets/bytecode de eventos ativos não são serializados,
pois esses estados bloqueiam salvar; flags e contadores de progressão persistem.
Sprites transitórios de poeira e progresso de música/vozes não são retomados.
Não há importação de saves PS1, seleção de memory card ou troca de animais.

## Como testar

Usar o título normal, selecionar New Game e jogar até a casa. Na cama, selecionar
“Make diary entry and then sleep”. Depois da manhã, o host confirma a gravação.
Para retomar, escolher Continue no título ou “Read diary” na cama.
Um arquivo explícito evita compartilhar o slot normal durante testes:

```sh
./build/native-debug/port/hmbtn_native --data data --house-preview --save-file /tmp/minha-fazenda.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/minha-fazenda.hms
./build/native-debug/port/hmbtn_native --data data --save-file /tmp/minha-fazenda.hms
```

O primeiro comando permite usar a cama sem repetir a abertura. O segundo retoma
diretamente; o terceiro abre o título usando esse mesmo slot para Continue.
O diretório pai de um caminho explícito precisa existir. O padrão é `farm.hms`
na pasta retornada por `SDL_GetPrefPath("hmbtn", "HarvestMoonBTN")`, ao lado da
pasta de recursos do usuário e independente de `--data`. Nenhum BIN ou recurso
proprietário é incorporado ao save ou adicionado ao Git.

## Verificação

- `native_save`: ida/volta e bytes estáveis, referências internas independentes,
  remoção dos empréstimos host, preservação de campos/RNGs, pagamentos/diário
  idempotentes, recusa de estados transitórios, versão/catálogo desconhecidos,
  truncamento, checksum, tamanhos, bool inválido, área e seções duplicadas.
- `native_save_files`: arquivos separados, nomes UTF-8, substituição de um slot,
  recusa sem alterar arquivo anterior, erros de diretório/publicação, limpeza
  de temporários, arquivo truncado e gravação/leitura em processos distintos.
- `native_save_playthrough`: recursos instalados; igualdade do estado persistido
  antes/depois e por tick, fazenda/galinheiro/casa, falha de recurso sem trocar
  sessão, cama real/dormir/salvar, campo/ração/ovos/remessa/flags e passagem diária
  sem repetição. Convite original de Duke salvo depois de concluído e carregado
  sem repetir alterações de amizade. Título Continue e diário usam APIs públicas.
  O runner inicia outro processo com `--load-save` e dirige o loop real de
  `app.c` com eventos SDL para testar Continue com arquivo existente e ausente.

Os testes usam pastas temporárias e SDL dummy/software; não escrevem no slot do
usuário. Configuração e build `native-debug` aprovados; suíte completa **92/92**, zero
falhas, **123,82 s** (59 sem dados opcionais e 33 com recursos locais). Após
assegurar que as asserções das novas fixtures continuem ativas em Release, os
testes de save e mapa foram repetidos em Debug: **4/4 aprovados**, 14,87 s.
Um cenário de preview sazonal
fora da fazenda também verifica que restaurar recursos preserva a estação do
campo ainda não visitado. A recompilação mostrou quatro avisos de constância
preexistentes em `port/tests/world_map_playthrough.c`, preservado nesta entrega.
Produção é compilada com `-Wall -Wextra -Wpedantic -Werror`.

Windows/cross-build não foi executado: toolchain MinGW não está disponível.
Também não foram executados Release, Android, sanitizadores, instalação limpa
ou inspeção manual de janela/áudio reais. Não há garantia nova de sincronização
em rede, retomada de scripts ativos ou compatibilidade com saves PS1. O bootstrap diagnóstico continua sem executar o jogo completo.


## Arquivos alterados nesta entrega

- `port/game/save.c/.h` e `port/filesystem/save_file.c/.h` (novos).
- `port/game/session.h`, `exploration.c`, `runtime.c`.
- `port/core/app.c/.h`, `port/core/main.c`.
- `port/graphics/scene_preview.c/.h`, `title_preview.c/.h`.
- `port/CMakeLists.txt`.
- Novos testes: `port/tests/save_fixture.h`, `save_test.c`, `save_file_test.c`,
  `save_file_process_test.py`, `save_playthrough.c`, `save_app_test.c`,
  `save_process_playthrough.py`.
- Documentação: este relatório, `ARCHITECTURE.md`, `PORTING_MATRIX.md`,
  `PORTING_STRATEGY.md`, `ROADMAP.md`, `ROADMAP_STATUS.md`.

Comparação com snapshot anterior à PF-05: nenhuma remoção de arquivo ou alteração
em outras implementações/documentos. `src/`, `sdk/`, `disc/`, `tools/` e CMake
PS1 permanecem intactos. A próxima entrega selecionada está no roadmap (batata);
nenhuma cultura nova foi implementada nesta tarefa.
