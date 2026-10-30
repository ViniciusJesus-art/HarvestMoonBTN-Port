# PF-06 — Reprodução bovina/ovina e entrada da hospedaria

Entrega de 2026-10-29, conforme a fila do [ROADMAP.md](ROADMAP.md).
Preserva os ajustes locais do usuário em `runtime.c`: recuo, animação do
personagem, aceleração 8, ângulo 0x420 e tempos de travessia das portas.

## Hospedaria do Doug

A zona externa da área 48 aponta para 204..207 conforme a estação, evento 34,
flags `0x1000`, modelo 0 e folha gêmea 1 (`SCENE_ENTRY_TWIN=0x400`). O runtime
tratava toda saída `0x1000` como passagem imediata. Agora verifica também a
presença das folhas vinculadas, anima ambas em sentidos opostos e só então
executa a entrada. A mesma distinção vale para pedidos de portas das rotas.
Não houve alteração nos tempos ou movimentos ajustados pelo usuário.

O percurso de fidelidade verifica a entrada nas quatro estações, ângulos das
duas folhas, recuo de 16×30, item carregado e chegada ao interior. Continua
verificando 18 travessias internas, recusas, descarte/coleta, chuva e cursores.

## Reprodução entregue

Barley oferece C.M. Potion e S.M. Potion por **3.000 G**, na ordem das linhas
originais. Descrição e resultado vêm dos textos 2:0x65/0x66 e 2:0x84. Cada
linha some enquanto a respectiva poção estiver no equipamento, mochila ou
caixa de ferramentas. Compra verifica saldo e espaço antes de inserir e cobrar.

Square usa a animação original da linha 26 e consome uma dose ao terminar,
inclusive quando não há alvo válido. A ação existente de medicina foi ampliada;
não há outro inventário ou motor de ações. Vacas devem ser adultas, saudáveis
e não prenhes; ovelhas também precisam de sete dias desde a tosquia. As duas
espécies compartilham capacidade de 10/20 animais e **uma maternidade**, ou
duas depois da ampliação do celeiro.

`livestock.births[2]` guarda índices de mãe/filhote na coleção existente.
`animals[child].reserved` ocupa uma vaga sem tornar o registro um animal visível,
contável, alimentável ou vendável. Compras e outra gestação respeitam a reserva.
O contador começa em **1** e, ao alcançar **21**, termina a gestação e marca o
parto pendente: são vinte atualizações diárias depois do uso, como no original.
O observador de datas existente impede repetição na mesma data. Alimentação,
saúde, crescimento e afeição continuam no mesmo ciclo.

Ordenha e tosquia recusam gestação. A lista original de venda exclui vacas
prenhes; o filtro de ovelhas permite sua venda. A adaptação libera a reserva
associada quando essa mãe é vendida, evitando índices órfãos no domínio nativo.
Esta limpeza é uma decisão de integridade do port; não uma alegação de que o
BIN executa a mesma sequência de limpeza.

O evento instalado **299** atende as maternidades 0/1 além das incubadoras 2/3
já integradas. Reutiliza o interpretador, comandos de área/fade, mensagens,
fontes de nomes maternos 0x4016/0x4017 e editor sem cancelamento. Bezerro/cordeiro
nascem no registro reservado, com crescimento zero, afeição 10, data atual e
posição 200 unidades atrás da mãe em Z. Repetir preparação não duplica nem
sobrescreve o filhote. Ao terminar os nomes, libera as maternidades e aplica
as posições normais das fileiras. A área do celeiro corresponde ao alvo -3 do
roteiro, sem criar uma sequência de parto paralela.

As falas de gestação usam os textos originais 0:0x6C3/0x6C2, com nome e dias
restantes. O contador inserido usa os glifos numéricos 0x40..0x49 do original.
A vaca prenhe anda a 2; a ovelha conserva a velocidade 6. Movimento/cues gerais
dos animais continuam parciais, conforme o recorte de aquisição anterior.

## Fontes e conferência dos recursos extraídos

| Referência original | Adaptação em `port/` |
|---|---|
| `animals/livestock.c`: `livestock_start_pregnancy/_b`, `livestock_overnight_update/_alt`, `livestock_copy_placement/_b` | `game/livestock.c/.h`: gestação, reserva, prazo, recém-nascido e regras por espécie |
| `animals/animals.c`: `farm_mode_slot_find`, `livestock_find_free`, `farm_items_apply_kind`, `farm_message_request` | Mesma coleção, maternidades 1/2, lugares originais e mensagens com dias restantes |
| `menu/shop_dialogues.c`: `shop_fodder_menu_setup`, `shop_barn_step`, `shop_livestock_find` | `game/feed_shop.c/.h`, `graphics/feed_shop_ui.c`, rótulos extraídos pelo CMake |
| `people/player.c`: `PLAYER_MODE_USE_POTION`, `player_load_tool_anim`, `player_apply_tool_target` | `game/poultry.c`, `game/exploration.c`: ação comum, linha 26 e dose única |
| `event/event_commands.c`: `animal_naming_begin/slot_ready/step/end`; `text.c`: `text_insert_source` | `game/runtime.c`, `game/farm_intro.c`: evento 299, maternidades e nomes |
| `world/scene.c`: carregamento de slot 1 no celeiro; `farm_load.c`, `farm_data.c` | `graphics/livestock_art.c`: ANI compartilhado e ambos os bancos originais |

Os caminhos de referência acima são relativos a `src/overlay/11_GF_FARM`,
exceto `src/main/text/text.c`. Constantes, posições e TOCs continuam extraídas
no configure por `port/cmake/generate_farm_tables.py`.

Foi conferido o overlay instalado `data/A_FILE/11_GF_FARM.bin` por desmontagem
MIPS somente para leitura, base 0x800b7e20. A rotina bovina em **0x800df72c**
verifica espécie 0x162, gestação/doença, chama a busca da maternidade e do registro
livre e grava gestação 1; a ovina em **0x800e12d8** também verifica lã ≥7.
Em **0x800ded34..0x800ded64**, a atualização incrementa, compara com 21, zera e
marca nome pendente. A tabela de ANI e a escolha dos packs 4/8 também foram
conferidas. Não são endereços convertidos em ponteiros host.

O overlay instalado tem SHA-1 `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`;
o build PS1 de referência é `664eee540550138d6e3717bb7e1593aeb8fe9d38`.
O disco local possui modificações já registradas no manifesto: essa conferência
não declara igualdade do disco com a referência, nem substitui a validação PS1.

No arquivo `00_gf.bin`, slot 0 ocupa `0x03810000..0x03833000` e slot 1 ocupa
`0x03833000..0x03845800`. Os cabeçalhos ANI são iguais; o banco de texturas de
slot 1 contém as páginas/paletas das prenhes, ausentes no banco de slot 0.
O port agora carrega ambos no atlas próprio de animais. Não sintetiza imagens
nem altera os recursos instalados. O evento 299 foi lido de `12_event_s.bin`,
com desvios para as quatro maternidades/incubadoras e seus textos reais.

## Persistência e limites

Save **v12**, sem migradores: LSTK grava explicitamente contador de gestação,
reserva e pares mãe/filhote. Valida índices, capacidade, exclusividade, relação
entre contador/parto pendente e registros reivindicados. Recursos e ponteiros
não entram no arquivo. Gestação e parto pendente são salváveis; a execução do
parto/nomeação continua protegida pela regra existente que impede salvar eventos.
Saves v11/anteriores são recusados, conforme a política atual do roadmap.

O recorte inclui nascimento/nomeação, sem completar PF-06. Funerais bovinos/
ovinos, tratamento de reservas após a remoção de uma mãe morta, pastagem,
concursos, máquinas, vozes/cues integrais e o restante da progressão seguem
pendentes. Animais mortos conservam seu registro; não há remoção silenciosa.
O bootstrap diagnóstico continua sem executar o jogo; este trabalho pertence
ao runtime jogável parcial. Windows real/MSVC, suíte integral e SHA-1 PS1 não
foram executados nesta rodada.

## Validação

- Linux Debug: configure/build aprovados; seleção final **12/12 CTests**, **21,91 s**.
  Inclui reprodução, save, ferramentas, loja, galinhas, compra/venda, noites reais,
  funerais/incubação de galinhas, chuva e fidelidade das portas.
- O percurso novo compra as poções, usa ANI real, atravessa vinte noites com
  alimento e executa as duas maternidades do evento 299. Salva durante gestação
  e após nomes; cada snapshot também abre em um processo novo do aplicativo.
- Linux Release: configure/build, **2/2 CTests**, **0,44 s**, e dois percursos
  diretos aprovados. O último verifica pixels não vazios dos quatro packs nas
  quatro direções e captura prenhes, filhotes e loja com câmera/fade adequados.
- Windows MinGW Release: build aprovado; unidade de reprodução, percurso de
  fidelidade e ciclo completo de reprodução aprovados no Wine.
- **46 processos de testes/wrappers/aplicativos**, incluindo tentativas e
  repetições; contando também os três lançadores Wine, **49**. Limite de 50
  respeitado. Não foi executada a suíte inteira.

Reprodução dirigida (com recursos locais instalados):

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(livestock_breeding|livestock_breeding_playthrough|world_fidelity_playthrough)$' --output-on-failure
```

Somente `port/` e `docs/port/` foram alterados. Código original, SDK, disco,
ferramentas e fluxo CMake PS1 foram preservados. Nenhum recurso proprietário
ou artefato de teste foi acrescentado ao Git.
