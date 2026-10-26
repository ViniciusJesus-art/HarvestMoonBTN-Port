# Tick de exploração livre — MA-06 integral

Entrega em 2026-10-25. MA-06a/b/c foram executadas juntas, conforme solicitação.
Ordem e aceite continuam no [ROADMAP.md](ROADMAP.md); estratégia em
[PORTING_STRATEGY.md](PORTING_STRATEGY.md), funções/dependências em
[PORTING_MATRIX.md](PORTING_MATRIX.md). A próxima tarefa é MA-07, ainda não iniciada.

## Implementação e entrada pública

`port/game/exploration.*` extrai a coordenação livre de `scene_preview.c` para
funções de domínio que usam as regras nativas já existentes:

| Função | Responsabilidade |
|---|---|
| `hmbtn_session_exploration_tick` | Um tick lógico integral de exploração livre: controles → mundo → animação → câmera lógica. |
| `hmbtn_session_exploration_controls` | Prioridade de ações, movimento/colisão, ferramentas, coleta, arremesso/transporte, alimentação, pesca, lojas, interações, menus e pausa. |
| `hmbtn_session_exploration_world` | Relógio, notificações, fadiga, agendas/rotas, galinhas, estatísticas e iluminação derivada, na ordem anterior. |
| `hmbtn_session_exploration_animate` | Avanço da ANI do jogador conforme pausa, carga de ferramenta e estado de ação. |
| `hmbtn_session_clock_events` | Observador anterior de remessas/vila/galinhas e limpeza diária já existente. Também usado pelos consumidores legados de fast-forward/sono. |
| `hmbtn_actor_animation_tick` | Extração do avanço e sinal ENDED/JUMPED anteriormente na cena. |

A chamada pública recebe `HmbtnSession`, `HmbtnLogicalInput` e
`HmbtnExplorationFrame`, com dados decodificados. O host fornece a cadência
(~29,97 ticks/s); a sessão não mede tempo de parede. Ground, colisão, frames e
conclusão de ações funcionam sem apresentação. Não há SDL, abertura de arquivo,
renderer, dispositivo/handle de áudio ou callback de desenho no módulo novo.

As fases públicas são a mesma implementação usada pelo coordenador atual nos
pontos de intercalação anteriores. Não ficou outra versão das regras na cena.
O host não chama o tick integral e as fases novamente para o mesmo tick. O
runtime geral ainda agenda a coordenação de `opening_tick` dentro do fluxo atual
de draw, até MA-07/08. A entrada integral permite atualizar o recorte livre sem
chamar esse fluxo ou desenhar, como demonstrado pelos testes.

O tick integral requer o handoff do mundo concluído e exploração livre. Recusa
abertura/visitas/conversas/eventos de animais ativos, itens de roteiro pendentes,
viagens/portas/sono e fades de pernoite pendentes. Não executa nem finge concluir
esses sistemas. A exploração pode iniciar uma dessas interações; sua continuação
é responsabilidade do coordenador legado até a MA-07.

## Estado único

Os seguintes membros saíram do armazenamento inline da cena e pertencem a
`HmbtnSession`:

| Estado | Propriedade e uso |
|---|---|
| `control`, `farm_action`, `fishing` | Movimento, colisão, ações/stamina/fadiga/experiência, estado atual da pesca e seu RNG existente. |
| `carry`, `feed`, `gather` | Mão/voos associados, alimentação/cochos e registros de coleta; usam o inventário e economia centrais. |
| `feed_shop`, `menu` | Fase de interação/pausa e estatísticas não financeiras existentes. Emprestam a mesma economia, inventário, relógio e galinhas. |
| `area`, `completion_reported`, `quick_*`, `refused_zone` | Identidade lógica da área, handoff e contadores de exploração já existentes. |
| `sleep`, `travel_*`, `door_*`, `overnight_tick`, `morning_fade` | Metadados existentes necessários aos bloqueios/pausas. A execução dos fluxos continua na cena, para MA-07. |
| `daylight`, `field_effects` | Estados derivados/transitórios atualizados na sequência existente; não são novas regras nem persistência de terreno. |

O construtor da sessão inicializa ferramentas/menu e suas referências, no lugar
das chamadas anteriores da cena. Casa, inventário/economia/calendário/cadastro,
registros de campo/vila/galinhas e execução dos eventos mantêm seus proprietários
das entregas anteriores. Não há cópias de gameplay sincronizadas entre cena e
sessão. A sessão continua não copiável/movível, pois os consumidores emprestam
endereços de seus membros.

Handles de voz (inclusive rega), renderer, atlas, texturas, recursos, geometria,
cache da TV e demais dados exclusivos de apresentação continuam no host.
A troca de área conserva o estado e apenas reassocia dados/cache, como antes.
Saves em disco continuam ausentes: não houve mudança/migração de formato de save.

## Recursos, apresentação e empréstimos

`HmbtnExplorationFrame` contém empréstimos delimitados da seção/terreno, ANI e
serviços. Vale somente durante a chamada. O host conserva a origem dos recursos;
endereços de 32 bits dos BINs continuam sendo decodificados pelos leitores,
sem conversão para ponteiros host ou uso de `long` como inteiro PS1.

- ANI é recurso de simulação necessário à conclusão de ação; seu provedor
  retorna dados decodificados, e o domínio avança os frames. Ausência é erro,
  em vez de marcar animação concluída artificialmente.
- Preparação de banco de ação, texto e recursos de moradores usa o host síncrono
  atual, apoiado nos leitores/serviços de MA-05. Pendência/erro retorna falha
  explícita nesse caminho, sem declarar o recurso instalado. A suspensão
  retomável dos roteiros continua no protocolo MA-05 existente.
- Expansão e parsing dos textos ficam no domínio. A nova
  `hmbtn_ui_message_art` aplica apenas arte, e a função combinada anterior da UI
  continua disponível para os consumidores legados.
- Sons, textura do campo, arte de mensagens/TV, estéreo e diagnósticos são efeitos
  de apresentação. Podem ser omitidos por um host headless; não determinam
  duração, conclusão, inventário, dinheiro ou avanço de relógio.
- O banco preparado pelo host fica vivo até deixar de ser usado. O domínio não
  guarda handles nem recursos gráficos proprietários dentro da sessão.

O caminho livre conserva a preparação síncrona existente. O tick não é uma
transação com rollback: retorna false no primeiro erro; o adapter atual para
sua execução. Não há retry automático de um tick parcialmente executado.
Não foram criados loader/scheduler genéricos nem outro interpretador.

Portas, viagem, sono e entrada/execução de conversas/visitas são fronteiras MA-07
explicitamente delegadas aos caminhos existentes da cena. Sem o adapter,
alcançar uma fronteira necessária é erro, não sucesso simulado. O tick integral
recusa transições já pendentes antes de tocar o mundo. No coordenador legado,
a associação `exploration_frame` é reconstruída antes da fase seguinte após uma
troca de área; não se reutilizam ponteiros à seção antiga liberada.

## Algoritmos, ordem e rastreabilidade

A extração parte da árvore local anterior à MA-06 (`d3c0f38` mais as entregas
MA-04b/05 ainda não commitadas). `scene_preview.c` forneceu os blocos existentes
de controles, mundo, observador e ANI; os blocos de porta/viagem/sono permanecem
nesse arquivo como adapter temporário. A origem está anotada no header novo.

Referências originais que continuam orientando os módulos, sem alterações:

- `src/overlay/11_GF_FARM/farm_main.c` e `farm_frame.c`: coordenação do mundo.
- `src/overlay/11_GF_FARM/people/player.c`: controle/ações/conclusão por ANI.
- `src/overlay/11_GF_FARM/field/field.c`, `field/fishing.c`: campo e pesca.
- `src/overlay/11_GF_FARM/world/clock.c`: relógio e callbacks.
- `src/main/graphics/anim.c`: avanço/sinais de animação.

Não foram trocados algoritmos, regras de inventário/economia/calendário,
sequências RNG, prioridades de interação ou cadência. Mantêm-se as pausas
existentes; por exemplo, o relógio não é congelado por toda e qualquer ação,
somente pelas mesmas condições anteriores. A liquidação permanece às **17h58**.
O observador também foi movido por dependência do relógio; isso não conclui
MA-19 nem amplia as regras diárias. Nenhum módulo original passou a ser compilado
diretamente. O reaproveitamento mais direto do C continua nas etapas da matriz.

## Validação e aceite

Linux x86-64, Debug, SDL3 3.4.18 local e `HMBTN_TEST_GAME_DATA=data/`:

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado, com os checks de compilação existentes.
- Os 77 testes anteriores passaram após a extração: 44,49 s.
- Suíte final: **79/79 CTests**, zero falhas, **50,88 s**; 51 independentes dos
  dados opcionais e 28 percursos com recursos locais.
- `native_exploration`: chama a API integral sem SDL, usando chão/ANI sintéticos
  realmente decodificados. Verifica movimento/colisão, bypass ao correr, pausa
  e retenção de minuto parcial, conclusão de enxada/rega por ANI, custo único,
  coleta/voo, coleta silvestre única, estoque/cocho, identidade de galinha na mão,
  pesca/banco de ação, remessa única às 17h58, limites da API e recurso ausente.
  Não força `animation_done`. `ldd` confirmou apenas libc/libm como bibliotecas
  dinâmicas desse executável, sem SDL/vídeo/dispositivo de áudio.
- `native_exploration_playthrough`: três sessões com os mesmos 180 ticks e
  recursos reais, zero/uma/duas apresentações por tick (equivalentes a ~0/30/60
  apresentações/s na cadência host). A enxada conclui sem desenho; posição,
  sprite/frame/ticks, campo, inventário, economia, calendário, moradores/RNG,
  fadiga/stamina e pausa coincidem após cada tick. As chamadas de desenho usam
  elapsed zero para não agendar simulação adicional.
- Os 77 anteriores protegem abertura/infância, primeira manhã Sim/Não, visitas,
  conversas/presentes/rollback, funeral/nomeação, sono, transições, menus,
  alimentação, pesca, remessas, inventário, economia e calendário.

06a/b/c estão concluídas. MA-07 não foi iniciada. A prova de independência de
desenho refere-se à exploração livre; não demonstra jogo completo headless,
renderização geral imutável, título separado ou migração completa dos testes.
Os percursos com dados reais ainda incluem `scene_preview.c` para construir o
adapter; a remoção desse acesso privado continua em MA-08c.

Não executados: Windows/cross-build, Release, Android, sanitizers, inspeção
manual de janela/áudio ou instalação limpa. Sanitizers não foram tentados de
novo; runtimes estavam ausentes no ambiente anteriormente registrado.
O bootstrap `--diagnostics` continua sem executar o jogo. BIN e recursos
proprietários não foram adicionados ao Git; instalação pelo BIN foi preservada.

## Arquivos desta entrega

Implementação/build:

- `port/CMakeLists.txt`
- `port/game/exploration.c`
- `port/game/exploration.h`
- `port/game/session.c`
- `port/game/session.h`
- `port/graphics/new_game_ui.c`
- `port/graphics/new_game_ui.h`
- `port/graphics/scene_preview.c`

Testes novos:

- `port/tests/exploration_playthrough.c`
- `port/tests/exploration_test.c`

Percursos existentes adaptados aos membros da sessão:

- `port/tests/calendar_playthrough.c`
- `port/tests/economy_playthrough.c`
- `port/tests/farm_feed_playthrough.c`
- `port/tests/farm_interiors_playthrough.c`
- `port/tests/farm_visit_playthrough.c`
- `port/tests/feed_shop_playthrough.c`
- `port/tests/first_morning_playthrough.c`
- `port/tests/fishing_playthrough.c`
- `port/tests/greg_rod_playthrough.c`
- `port/tests/ingredient_delivery_playthrough.c`
- `port/tests/inventory_playthrough.c`
- `port/tests/pie_quest_playthrough.c`
- `port/tests/poultry_helpers.h`
- `port/tests/poultry_lifecycle_playthrough.c`
- `port/tests/poultry_medicine_playthrough.c`
- `port/tests/poultry_playthrough.c`
- `port/tests/poultry_status_playthrough.c`
- `port/tests/poultry_transport_playthrough.c`
- `port/tests/session_events_playthrough.c`
- `port/tests/session_playthrough.c`
- `port/tests/shipping_playthrough.c`
- `port/tests/shop_shortage_playthrough.c`
- `port/tests/village_gifts_playthrough.c`
- `port/tests/village_playthrough.c`
- `port/tests/village_talk_playthrough.c`
- `port/tests/world_clock_playthrough.c`
- `port/tests/world_map_playthrough.c`

Documentação:

- `docs/port/ARCHITECTURE.md`
- `docs/port/EXPLORATION_TICK.md`
- `docs/port/PORTING_MATRIX.md`
- `docs/port/PORTING_STRATEGY.md`
- `docs/port/ROADMAP.md`
- `docs/port/ROADMAP_PENDENTE_CODEX.md`
- `docs/port/ROADMAP_STATUS.md`

As alterações preexistentes de MA-04b/05 foram mantidas. Seus relatórios
`SESSION_STATE.md`, `SESSION_EVENTS.md` e `SERVICE_CONTRACTS.md`, não rastreados
no início desta tarefa, permaneceram byte a byte iguais. Os percursos existentes
mantêm suas asserções; mudaram os caminhos dos membros de cena para sessão.
Nenhum arquivo fora de `port/` e `docs/port/` foi alterado nesta entrega.
