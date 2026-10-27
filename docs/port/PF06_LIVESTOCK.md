# PF-06 — Aquisição e alimentação de vacas e ovelhas

Entrega de 2026-10-27. A sequência permanece no [ROADMAP.md](ROADMAP.md).
Completa o recorte de aquisição/presença/capacidade/alimentação exigido para o
celeiro; **não conclui todos os sistemas bovinos/ovinos nem PF-06**.

## Comportamento entregue

Barley, na sua loja e horário existentes, vende vaca jovem por **6.000 G** e
ovelha por **4.000 G**. Preços vêm do catálogo original. O mesmo modal de loja e
editor de nomes já usados pelas galinhas tratam descrição, nomeação, confirmação,
cancelamento, fundos e lotação. A disponibilidade é conferida novamente antes
de criar o animal e debitar; cancelar não reserva animal nem cobra dinheiro.

Vacas e ovelhas ocupam uma coleção compartilhada: dez vagas no celeiro inicial,
vinte depois da obra. Não há vinte vagas por espécie. Animais comprados são
colocados nas posições originais das fileiras do celeiro lógico 96, com nome,
data, crescimento e afeição iniciais originais. A coleção permanece a mesma
entre áreas, obra, manhãs e saves; renderer, status e loja apenas a consultam.

O ciclo diário visita vacas antes de ovelhas, como os dois passes originais.
Cada animal recebe uma porção disponível antes da limpeza dos cochos. Conversa
dá ganho diário único, alimentação/falta de alimento alteram afeição, e idade,
crescimento e doença seguem as regras adaptadas. A vaca comprada começa com
14 dias de crescimento e fica adulta aos 28; a ovelha comprada já tem o estágio
adulto. Medicina usa a mesma animação/dose/fadiga do jogador existente e passa
a tratar também gado; uso perdido ainda consome uma dose, como antes.

Cross com mãos livres abre as falas originais de saúde/afeição, com o nome
real inserido. As páginas de vacas e ovelhas do status exibem os registros,
idade/data e corações; contagens são derivadas. A arte vem do pacote ANI/BIN
original: os mesmos leitores e renderer de sprites desenham os animais com
oclusão de cenário. A vaca jovem usa escala original 0xCC8; a adulta, 4096.
Os sprites existentes conservam escala padrão e seus caminhos de desenho.

## Fontes e reaproveitamento

`F` = `src/overlay/11_GF_FARM`; `P` = `port/game`.

| Código/tabela original | Implementação nativa | Dependências e estratégia |
|---|---|---|
| `F/animals/animals.c`: `livestock_find_free`, `place_row_spot`, `farm_row_spots` | `P/livestock.c/.h`, `P/session.c/.h` | Coleção autoritativa; capacidade lê empréstimo de `construction.barn_level`. Posições extraídas do C original no configure. Preserva fileiras de cinco e sentidos alternados. |
| `F/menu/shop_dialogues.c`: `shop_fodder_menu_setup`, `shop_barn_step`; `src/main/game/farm_tables.c`: `ITEM_YOUNG_COW`/`ITEM_SHEEP` | `P/feed_shop.c/.h`, `port/graphics/feed_shop_ui.c`, `port/CMakeLists.txt` | Estende o modal/editor atuais; mensagens 2:0x67/0x68, 0x72/0x73/0x74 e pergunta 0x6B. Nomes originais extraídos, sem loja/economia/inventário paralelos. Linhas de venda/poções/sino ainda não integradas permanecem ocultas. |
| `F/animals/livestock.c`: `livestock_record_init`/`_b`, `livestock_growth_kind`/`_b`, `livestock_overnight_update`/`_alt` | `P/livestock.c`, `P/daily.c` | Adapta inicialização, alimentação, ordem por espécie, afeição, doença, crescimento e envelhecimento. Reutiliza RNG original explícito, com estado local salvo; não exige sequência global PS1. Constantes extraídas do original. Pastagem/reprodução ficam fora do recorte. |
| `F/animals/livestock.c`: `livestock_talk`/`_b`, `livestock_cure_sickness`/`_b`; `F/animals/animals.c`: `farm_message_request` | `P/livestock.c`, `P/exploration.c`, `P/poultry.c/.h` | Mensagens originais 0:0x6BD..0x6C1, fonte do nome e flag diária. Medicina generaliza a ação existente e mantém wrapper para consumidores anteriores; não cria outra regra de consumo/fadiga. |
| `F/animals/livestock.c`: tabelas de frames, `livestock_pick_state`/`_b`, `livestock_walk_cue`/`livestock_walk_start` | `P/livestock.c`, `P/exploration.c` | Frames/poses e decisões básicas por espécie reaproveitados; sondas de chão e bloqueio host substituem callbacks PS1. Temporização e sequência completa de cues ainda parciais, sem declarar equivalência integral do deslocamento. |
| `F/farm_load.c`: `farm_load_resource`, `farm_slot_file_offsets`; `F/farm_data.c`: `farm_slot_toc_0` | `port/graphics/livestock_art.c/.h`, `port/cmake/generate_farm_tables.py` | Extrai TOC e faixa de arquivo, carrega ANI e banco por offsets verificados. Não converte endereço serializado em ponteiro host. Mesmo pacote serve aos estágios; não há sprites inventados. |
| `F/animals/livestock.c`: `LIVESTOCK_YOUNG_SCALE`; desenho ANI existente | `port/graphics/asset_decode.c/.h`, `model_render.c/.h`, `new_game_ui.c` | Escala explícita no sprite nativo; wrapper antigo conserva escala 4096. Testes sintéticos verificam redução, transparência e limites, além das regressões de composição/oclusão. |
| `F/menu/status_pages.c`: `livestock_count_cattle`, `livestock_count_sheep`, `status_draw_cow_row`, `status_draw_sheep_row` | `P/farm_menu.c/.h`, `port/graphics/farm_menu_ui.c` | Contagens/listas consultam os animais. Removidos dois contadores persistentes vazios de gado, sem passar a atualizá-los manualmente. Mantido contador legado de galinhas para consumidores existentes. |

Nada em `src/`, `sdk/`, `disc/`, `tools/` ou CMake PS1 foi alterado. Nenhum módulo
C original passa a ser compilado diretamente no runtime: algoritmos são adaptados
em `port/`, tabelas extraídas e recursos originais interpretados. O alvo continua
Linux/Windows, sem emulação/ABI MIPS/SHA-1 como requisito. Capacidades e preços
são conteúdo inicial do jogo, não uma restrição permanente para expansões PC.

## Estado, apresentação e save

`HmbtnSession.livestock` possui os registros e seu RNG; cada registro tem espécie,
nome, posição/ANI, crescimento, idade, afeição, saúde e flags diárias. Estágio,
pack, escala, contagens e capacidade são consultas derivadas. A estrutura empresta
o controlador de construções; não duplica o nível do celeiro. `feed_shop` e `menu`
emprestam a coleção. Nenhum animal pertence à UI ou ao cache de recursos.

O observador diário existente executa criação antes de limpar os cochos e usa
a mesma chave de data que evita repetir updates. O estoque de feno continua no
inventário central; o alimento colocado continua em `feed.barn`. Desenhar não
move/amadurece/alimenta os animais. Animação e movimento avançam no tick mesmo
que não haja apresentação.

Save **v8 aceita somente v8**, adiciona seção LSTK com campos explícitos de
animais/RNG e retira os dois contadores vazios de gado de PLAY. Valida ocupação
dentro da capacidade, estágio/saúde/data/afeição e posições/ANI limitadas. O
construtor refaz os vínculos à coleção e à construção; não serializa ponteiros,
atlas nem bytes de structs. Publicação transacional do arquivo continua igual.
Saves v7/anteriores são recusados, conforme a decisão de não manter migradores.

## Validação

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado. Avisos preexistentes de
  `const` na fixture de mapa apareceram na recompilação; produção passou com
  `-Wall -Wextra -Wpedantic -Werror`.
- Primeira seleção de regras/save/loja/menu: **4/4 CTests** aprovados.
- Seleção final: **25/25 CTests**, **18,58 s**, incluindo regras novas, decoder/
  renderer, sessão/inventário/eventos, construção/casa/celeiro, cochos, galinhas,
  medicina/status, lojas, portas/interiores e saves com retomada em outro processo.
- **46 processos de teste** no total, contando wrappers/filhos e duas execuções
  diretas do percurso; limite de 50 respeitado. Suíte inteira não executada.
- Novos testes: `native_livestock` e `native_livestock_playthrough`. Regras cobrem
  vagas mistas 10/20, posições, preços/falhas/cancelamento, medicina/dose, crescimento,
  ordem vaca→ovelha com alimento insuficiente, notificação diária repetida, estado
  inválido recusado e empréstimos restaurados. O percurso compra/nomeia ambos,
  consulta fala/status, alimenta por cinco noites reais, amplia o celeiro com os
  animais presentes e salva/carrega em vários pontos, inclusive no novo app.
- A compra e o editor são reais; dinheiro/feno e obras anteriores são fixtures.
  O pedido do celeiro neste teste usa a API de produção, já exercitada por Gotz
  no teste separado. Um avanço controlado da idade chega ao limiar de 28 dias
  na última noite; não simula livremente a maturidade em uma partida normal.
- Capturas de loja, editor, fala com nome, status, animais jovens/adultos e
  celeiro ampliado inspecionadas. Capturar verifica invariância do gameplay.

Windows/cross-build, Release, sanitizers, Android, instalação limpa e sessão
manual com janela/áudio não foram executados.

## Como experimentar

```sh
ctest --preset native-debug -R '^native_livestock(_playthrough)?$' --output-on-failure
```

Para gerar uma partida de inspeção com os animais comprados pelo percurso:

```sh
./build/native-debug/port/hmbtn_livestock_playthrough data 0 /tmp/livestock-current.hms
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/livestock-current.hms
```

O save termina na casa ampliada; visite o celeiro normalmente. É uma fixture de
teste, não um atalho gratuito no jogo. O BIN continua fornecido pelo usuário;
nenhum recurso/captura proprietário foi incluído no Git.

## Arquivos desta entrega

- Novos: `port/game/livestock.c/.h`, `port/graphics/livestock_art.c/.h`,
  `port/tests/livestock_test.c`, `livestock_playthrough.c`.
- Domínio/integração: `port/game/session.c/.h`, `daily.c`, `exploration.c/.h`,
  `feed_shop.c/.h`, `farm_menu.c/.h`, `poultry.c/.h`, `save.c/.h`.
- Apresentação: `port/graphics/scene_preview.c`, `feed_shop_ui.c`, `farm_menu_ui.c`,
  `asset_decode.c/.h`, `model_render.c/.h`, `new_game_ui.c`.
- Geração/build: `port/CMakeLists.txt`, `port/cmake/generate_farm_tables.py`.
- Regressões estendidas: `port/tests/asset_decode_test.c`, `model_test.c`.
- Documentação: este relatório, roadmap/status/backlog, arquitetura, estratégia,
  matriz e notas de versão nos relatórios de saves e celeiro.

Alterações anteriores da casa/celeiro e demais trabalhos foram preservadas;
`runtime.c` não precisou mudar nesta entrega.

## Limites e sequência

Venda, poções/reprodução, leite/lã/escova, máquinas, pastagem/transporte entre
interior e exterior e festivais não foram integrados. As cues de locomoção são
parciais e as vozes XA dos animais não foram ligadas; os dados de saúde e idade
existem, mas eventos de funeral/remoção ainda faltam. Um animal morto mantém seu
registro reservado e deixa de ser desenhado; não há venda/remoção silenciosa nem
evento falso. Esse ciclo precisa ser completado antes de chamar criação de animais
de sistema completo. Medicina/conversa/alimentação são funcionais neste recorte.

O requisito de capacidade/alimentação que faltava à obra do celeiro foi atendido.
A próxima entrega na cadeia de construção é a **segunda casa**, com auditoria de
mapas, cama/objetos e estado real; casamento/família não serão declarados prontos
pela simples troca de mapa. Os demais cuidados/produção animal continuam no
backlog PF-06, com as fontes acima para adaptação, sem recomeçar a coleção.
