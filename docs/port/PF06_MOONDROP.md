# PF-06 — Moondrop e visitas de Karen

Entrega de 2026-10-27. Linux/Windows continuam os alvos atuais; Android posterior.
Adaptações em `port/`, sem alterar `src/`, backends SDL3, recursos proprietários
ou instalação pelo BIN do usuário. PF-06 permanece aberta: o jogo completo
continua parcial e o bootstrap de diagnóstico não executa o jogo.

## Cadeia original e dependência encontrada

Habilitar a cultura e a linha de Won não seria suficiente para um percurso
jogável. O comando que libera a loja está no evento **19**, que exige uma
Moondrop madura; o primeiro pacote vem do evento **18**. Essa dependência foi
integrada nesta entrega usando os blocos instalados, sem novo interpretador.

| Fonte original | Comportamento e integração |
|---|---|
| `src/overlay/11_GF_FARM/event/event_script.h`: `FarmEventEntry`; tabela instalada da fazenda | Eventos 18/19: entrar saindo da casa (lugar anterior 2), 06:00..11:59, papel de solteiro; flags de conclusão separados. Evento 18 usa primavera 1..20, afeição de Karen >=15.000 e espaço acessível de ferramentas. |
| `event/event_commands.c`: `event_wedding_role_match`, `event_bond_match`, `event_calendar_match` | Adaptar papel da família atual e usar afeição para candidatas, amizade para os demais; consultar espaços vazios de equipamento/mochila pelo inventário comum. |
| `farm_init.c`: `farm_event_available`, condição 7; `field/field.c`: `field_find_free_tile(flags=2)` | Existência de célula com valor `0x31` e estágio maduro 3, ignorando chão. Estufa ausente não é simulada. A busca booleana não precisa sortear um ponto inicial. |
| Bloco instalado 18 em `A_FILE/12_event_s.bin` | Karen chega, espera, mostra o pacote e armazena uma unidade pelo mesmo inventário; conversa concede +2.000 de afeição. Se ignorada até o prazo, não concede pacote nem bônus. |
| Entrada `0x0831` do bloco 18; `event_cmd_get_item_id`, `event_cmd_publish_offset`, `event_cmd_item_enable`, `event_item_store` | ANI original do pacote, publicação ao lado de Karen, ocultação e armazenamento. Prop transitório não é outra cópia do inventário. |
| Bloco 19; `event_cmd_dispatch`, comando 27 / `EVENT_CMD_SET_STATUS_BIT23` | Libera Moondrop ao iniciar a visita, antes da conversa. Ignorar a visita mantém a liberação, sem o bônus de afeição da conversa. |
| `shop_seed_menu_setup_d` em `menu/shop_dialogues.c` | A linha fonte 3, descrição `0x168`, depende do antigo bit 23 de `estate.statusFlags`. Mesmo fluxo de Won, compra/quantidade/baú. |
| `src/main/game/farm_tables.c`: `CROP_MOON_DROP`, semente `0x31`, produto `0x51` | 300 G; primavera; etapas 2+2+2 manhãs regadas, sem rebrota ou preço de venda. Mesmo catálogo/motor diário, ações, coleta, mochila e presentes. |
| `field_cell_look_spring/summer`, `graphics/anim.c`: `anim_draw_sprt_parts` | Sprites 12/40. ANI viva declara cinco faixas; faixa madura zero explícita preserva a aparência sem repetir a leitura original fora dos limites. |

As condições acima foram lidas das funções C e tabelas instaladas. A cadeia
não depende de uma data ou amizade inventada a partir de guias externos.
Os dois scripts usam Karen, identidade 3, seus movimentos/ANI/texturas, falas
**1:86/87/88** e música **5**, por meio dos serviços nativos existentes.
O modo musical de uma visita anterior não deve suprimir a carga do áudio
atual: o início dessas visitas compara com a música realmente carregada.

Os bônus de conversa continuam sujeitos aos ajustes normais de relacionamento
entre dias; o teste não presume que a afeição permaneça fixa durante o cultivo.
Os flags de conclusão são 269/270 (`251 + evento`), distintos da liberação.

## Estado autoritativo, interfaces e persistência

`HmbtnGameState.progress.moondrop_seeds` é a única fonte de verdade da
liberação. `HmbtnFeedShop.progress` e `HmbtnHouseInteraction.progress` emprestam
essa estrutura. A loja preserva o empréstimo ao reiniciar sua máquina.
`hmbtn_event_progress_command` em `game/event_variables.c` adapta o comando
81/27 e é compartilhado pelas visitas e conversas. O checkpoint de rollback
já existente nas conversas passa a incluir a progressão, sem virar um estado
alternativo consultado pelo jogo.

O suporte técnico à cultura é independente de sua disponibilidade comercial.
Antes da visita 19, Won oferece seis linhas (fonte `0/1/2/4/5/6`); depois,
oferece sete (fonte `0..6`). Cada entrada conserva nome e descrição próprios.
Orangecup (`0x35`) continua fora do suporte/menu, aguardando a estufa.

O gatilho empresta o campo em `HmbtnFarmTriggers.field`. Nenhuma cópia de células,
flag derivada de maturidade ou sincronização manual foi acrescentada. A busca
apenas verifica valor/estágio; não avança o RNG de plantio. A família nativa
atual continua solteira; papéis de casamento completos ficam fora deste recorte.

`HmbtnFarmVisit.prop` guarda somente ID, posição e visibilidade do pacote em
cena. Sua ANI é decodificada da entrada original do evento, e ele é ordenado
junto aos objetos do mundo pelo renderer existente. Não concede recursos antes
do comando de armazenamento e não é persistido fora de uma visita.

O escritor passa a **save v4**: um booleano canônico u32 little-endian ao final
de AUXI, após os RNGs das versões 2 e 3. O leitor aceita **v1/v2/v3/v4**.
Versões anteriores recebem liberação falsa, preservando os campos que possuíam;
não se deduz o desbloqueio a partir de outro flag. Continuam válidas as migrações
anteriores do clima e do RNG de plantio. Construtores vinculam casa/loja ao
estado da nova sessão. Eventos/diálogos ativos continuam impedindo salvamento.

## Arquivos modificados nesta entrega

| Grupo | Arquivos em `port/` |
|---|---|
| Progressão e empréstimos | `game/game_state.h`, `game/house_interaction.c/.h`, `game/session.c`, `game/feed_shop.c/.h` |
| Comando comum e rollback | `game/event_variables.c/.h`, `game/village_talk.c/.h` |
| Visitas/gatilhos/coordenação | `game/farm_visit.c/.h`, `game/farm_intro.c/.h`, `game/runtime.c`, `game/exploration.c` |
| Cultivo e arte | `game/farm_field.c/.h`, `graphics/estate_art.c`, `graphics/scene_preview.c` |
| Saves | `game/save.c/.h` |
| Testes e build | `CMakeLists.txt`, `tests/moondrop_playthrough.c` (novo), `tests/farm_visit_test.c`, `tests/won_seed_test.c`, `tests/flower_test.c`, `tests/save_test.c`, `tests/crop_daily_test.c`, `tests/magic_red_test.c` |

Os dois últimos testes apenas atualizam o exemplo de cultura ainda não suportada
para `0x35`; Moondrop já é suportada. O teste compartilhado de flores ganhou os
vetores de Moondrop. Documentação atualizada: este relatório, `ROADMAP.md`,
`ARCHITECTURE.md`, `PORTING_MATRIX.md`, `PORTING_STRATEGY.md`, `ROADMAP_STATUS.md`
e `ROADMAP_PENDENTE_CODEX.md`. Arquivos originais e demais alterações preservados.

## Validação e limites

`cmake --preset native-debug` e `cmake --build --preset native-debug` passaram
em Linux Debug, com SDL3 local e dados instalados. Produção continua compilada
com os checks existentes; avisos const antigos de `world_map_playthrough.c`
não foram alterados nesta entrega.

A seleção de regressão passou **20/20 CTests, zero falhas, 22,41 s**:

```sh
ctest --preset native-debug --output-on-failure -R '^native_(save|flower_rules|won_seed_rules|farm_visit|magic_red_rules|event_variables|crop_daily|event_tool_store|inventory_rules|vineyard_invitation_playthrough|farm_visit_playthrough|moondrop_playthrough|won_[0-6]_playthrough|weather_doors_playthrough)$'
```

A revisão final corrigiu o modo musical antigo retido entre visitas e acrescentou
uma verificação do áudio 5. Build completo passou novamente, e somente o percurso
afetado foi repetido: **`native_moondrop_playthrough`, 1/1, 2,50 s**. Os demais
resultados acima precedem essa alteração restrita à entrada das visitas 18/19.

Cobertura: limiar de afeição, janela/lugar, espaço, flor madura, menu bloqueado/
liberado, operandos inválidos, comando idempotente, compra/cancelamento, presente
inicial, ambos os ramos de espera, texto/arte/música, plantio original, seis noites,
coleta única, mochila e presente real para Lillia. Regras independentes cobrem
noite seca, ausência de renda, murchamento/inverno e migrações v1/v2/v3. O driver
salva/carrega em fases distintas e o wrapper retoma no aplicativo em outro processo.

| Rodada | Entradas | Filhos adicionais | Processos |
|---|---|---|---|
| Regras iniciais | 6 | 0 | 6 |
| Percursos diretos, incluindo repetições | 3 | 0 | 3 |
| Seleção de regressão | 20 | 16 | 36 |
| Repetição final do percurso | 1 | 2 | 3 |
| Total | 30 | 18 | **48** |

Uma tentativa anterior ocorreu antes de o executável de teste estar disponível
(sem iniciar processo do jogo). Contando também essa tentativa, **49**, dentro
do limite **50**. As duas primeiras execuções diretas falharam em expectativas
da fixture: faltava o tick de entrada para disparar a segunda visita e a afeição
esperada ignorava os ajustes diários normais. As expectativas foram corrigidas,
e o percurso completo e a repetição final passaram.

Capturas do pacote original, flor madura e menu de sete linhas foram inspecionadas.
Suíte completa de **122 testes**, Windows, Release, sanitizers, janela/áudio
físicos e instalação limpa **não executados**. Sem alegação de comparação visual
lado a lado com uma execução PS1.

Logs em `/tmp/hmbtn-moondrop-configure.log`, `/tmp/hmbtn-moondrop-final-build.log`,
`/tmp/hmbtn-moondrop-core-tests.log`, `/tmp/hmbtn-moondrop-final-tests.log` e
`/tmp/hmbtn-moondrop-audio-recheck.log`. Capturas em `/tmp/hmbtn-moondrop-shots/`;
nenhum desses recursos foi adicionado ao Git.

## Como experimentar

```sh
./build/native-debug/port/hmbtn_native --data data --area-preview 0 --preview-date 0:3 --preview-time 06:00
```

O preview inicia uma sessão nova, sem conceder afeição ou desbloqueio. Para o
percurso normal, aproxime-se de Karen até 15.000 de afeição e saia da casa na
janela da visita 18, com espaço acessível para ferramentas. Converse com ela,
plante as sementes na primavera e regue por seis manhãs. Deixe ao menos uma flor
madura no campo e saia da casa na janela da visita 19; depois Moondrop aparece
em Won na estalagem por 300 G. O save conserva a liberação, inclusive quando a
flor é colhida ou a estação muda.

Para executar a fixture completa com data/afeição declaradas, usando conversa,
arte e ações reais, em um ambiente que permita janela ou com os drivers dummy:

```sh
mkdir -p /tmp/hmbtn-moondrop-manual
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
./build/native-debug/port/hmbtn_moondrop_playthrough data 0 \
  /tmp/hmbtn-moondrop-manual/check.hms /tmp/hmbtn-moondrop-manual
./build/native-debug/port/hmbtn_native --data data --load-save /tmp/hmbtn-moondrop-manual/check.hms
```

Esse driver de teste exercita também os ramos ignorados em sessões separadas;
o arquivo final continua sendo o da cadeia concluída. Não foi adicionado um
atalho de progressão ao produto. A próxima tarefa é estufa/Orangecup, começando
pela aquisição original, conforme o [roadmap](ROADMAP.md).
