# Contratos mínimos de serviços — MA-05 integral

Entrega em 2026-10-25. MA-05a e MA-05b foram executadas juntas por solicitação
explícita do usuário. [ROADMAP.md](ROADMAP.md) continua sendo a fonte principal
da próxima tarefa: MA-06, ainda não iniciada. Diretrizes em
[PORTING_STRATEGY.md](PORTING_STRATEGY.md), dependências em
[PORTING_MATRIX.md](PORTING_MATRIX.md), estado anterior em
[SESSION_EVENTS.md](SESSION_EVENTS.md).

## Contrato e propriedade

`port/game/services.*` define o protocolo sem SDL, arquivos, renderer ou dispositivo
de áudio. Cada `HmbtnSession` possui seu `HmbtnServices`, inicializado a zero.
`HmbtnLogicalInput` contém máscaras de botões pressionados/mantidos; o adapter
SDL existente produz esse valor e a aplicação o entrega à cena, preservando o
acúmulo de pressões e a cadência anteriores.

| Canal | Pedidos dos consumidores atuais | Resultado |
|---|---|---|
| RESOURCE | Dados de evento, gatilhos por área sazonal, banco de ação, registro de texto | Bytes próprios e, para ação, tamanho da seção ANI. |
| MESSAGE | Mostrar mensagem de evento ou conversa; banco/registro, emissor e cursor | Escolha ou zero ao fechar. |
| ANIMATION | Esperar a animação selecionada de um ator de roteiro | Conclusão identificada do ANI existente. |
| ACTION | Esperar a ação/animação do jogador no roteiro | Conclusão independente do canal de NPC. |
| AUDIO | Carregar música, iniciá-la, iniciar XA, aguardar término de XA | Conclusão ou erro explícito do backend atual. |

Há um pedido ativo por canal, não uma fila ilimitada. IDs de conteúdo são os
IDs lógicos originais; não são offsets, ponteiros serializados ou comandos MIPS.
O ticket de 64 bits é monotônico e válido somente junto à sessão que o emitiu.
O cursor distingue ocorrências sucessivas de uma mesma mensagem/animação.
Outro pedido conflitante não sobrescreve o atual. Esgotamento de tickets é erro,
sem reciclar identificadores. Não copiar/mover a sessão/protocolo em atividade.

O ciclo é:

1. `hmbtn_service_begin` publica PENDING; repetir o mesmo pedido conserva o ticket.
2. `hmbtn_service_claim` entrega-o uma vez ao provedor. O provedor pode responder
   imediatamente ou depois. Polling não reenvia carregamento, som ou animação.
3. O provedor completa com ticket, resultado ou erro. Somente o primeiro resultado
   é aceito. Repetidos, antigos, cancelados ou já reconhecidos são rejeitados.
4. DONE/FAILED/CANCELLED são terminais e imutáveis até `hmbtn_service_ack`.
   PENDING não pode ser reconhecido nem interpretado como sucesso.
5. O consumidor aplica o resultado e reconhece o ticket. Recursos/áudio recebidos
   seguram o cursor também enquanto estão DONE, até sua aplicação e ack.

`hmbtn_service_complete_data` copia bytes do provedor para memória própria,
limitada a 1 MiB e sem publicar buffer parcial. O provedor conserva e libera
sua origem, inclusive para uma resposta rejeitada. O consumidor empresta o
resultado até ack/clear/destruição; se precisar mantê-lo, copia antes do ack.
Os consumidores verificam comprimento esperado e metadados antes de copiar.
A marca DONE do recurso indica bytes recebidos, não sucesso do decoder:
`action_ready`, troca de evento e demais flags só são aplicados após decodificar.
Falha de formato continua sendo propagada explicitamente pelo carregador.

Cancelamento interrompe a espera como falha explícita; não equivale a uma escolha
ou animação concluída. Não há retry automático de efeitos. `clear` libera os
buffers e cancela o contexto antigo, preservando a sequência de tickets. Abrir
um evento válido e destruir a sessão usam esse caminho. A substituição também
limpa flags dos pedidos antigos, evitando reemiti-los no novo roteiro. Evento inválido conserva
execução e resultados anteriores, como no contrato MA-04b.

## Integração real

`filesystem/service_resources.c` usa `event_resources.c`, `action_resources.c`
e `text_resources.c`, que continuam sobre o leitor delimitado existente.
Somente após uma leitura integral bem-sucedida ele publica uma cópia do buffer;
falha/truncamento não expõe os bytes parcialmente lidos. A cópia temporária do
host é sempre liberada. Scripts/ANI/texturas continuam usando armazenamento
com a vida útil prevista em MA-04b, nunca ponteiros ao resultado já reconhecido.

`audio/service_audio.c` usa `title_audio.c`, `xa_stream.c` e o mixer existente.
Carregamento/início são imediatos neste host. A espera de XA adquire uma vez e
consulta o término nos ticks posteriores; seu ticket de provedor pertence ao
adapter de áudio. Um provedor externo que adquiriu o pedido primeiro não é
substituído pelo adapter imediato. Ausência de dispositivo continua permitindo
a mixagem nativa silenciosa já existente; falha de recurso não é sucesso.

Na cena, os pontos de entrega de recursos/áudio fazem polling e aplicam resultados
no lugar das chamadas antigas, conservando a ordem do fluxo imediato. Leitura
de evento/ação/texto pendente mantém o pedido e sua instrução suspensos. Recursos
de gatilhos e construção usam o mesmo leitor pelo caminho síncrono; esse caminho
recusa pendência explicitamente e não declara carregamento concluído.

`hmbtn_session_event_tick` aplica respostas de mensagens, animações/ações e
espera de XA antes de chamar o handler existente. A UI responde ao fechamento,
incluindo a escolha, em vez de alterar diretamente `intro.message_done`/slots.
`hmbtn_session_talk_tick` faz a mesma ponte para conversas, reutilizando seu
runtime e rollback existentes. Respostas incompatíveis com ator/mensagem atuais
são erros. O término de ação e a espera de sprite usam tickets distintos e os
mesmos sinais ENDED/JUMPED do decoder ANI, sem outro algoritmo de animação.
Não se publica uma espera para o sprite antigo quando um salto acaba de apontar
para um comando de animação que ainda não foi executado.

## Rebind, erros e fronteiras

`hmbtn_session_unbind_resources` retira os empréstimos do host já definidos e
republica intenções pendentes com novos tickets, invalidando o provedor antigo.
Resultados próprios já concluídos sobrevivem. O estado da UI já aberta, atores,
cursores e bytecode permanecem na sessão; a UI pode reassumir o pedido sem abrir
a mesma mensagem outra vez. Pausar o adapter e reassociar terreno/callbacks/
recursos necessários antes de prosseguir, como em MA-04b.

As APIs devem ser usadas no thread da simulação. Um backend assíncrono futuro
precisará encaminhar respostas a esse thread e encerrar seus trabalhos antes
de destruir a sessão; não há promessa de thread safety ou threads novos.
Atraso é suportado pelo protocolo, sem criar agora um scheduler genérico.

MA-05 não extrai o tick da cena. Ela ainda coordena os adapters e pode atualizar
no draw. Carregamento composto de área, portas, efeitos curtos, recursos de
apresentação de menus/título e demais operações gráficas continuam nos backends
atuais, até os consumidores/ticks serem migrados em MA-06..08. Os canais novos
atendem consumidores existentes; não representam uma camada completa de SDK,
interpretador genérico completo ou gameplay inteiro sem SDL. Estado de mundo,
inventário/economia/calendário, regras e fluxo BIN não foram reimplementados.
Saves em disco continuam ausentes. O bootstrap `--diagnostics` não executa o jogo.

## Origem e critérios de conclusão

Esta etapa extrai contratos dos pontos de entrega/espera de `scene_preview.c`
e `session.c` presentes no início da entrega, mantendo seus handlers. As fontes
originais continuam intactas, consultadas como referência:

- `src/main/cd/afile.c` e tabelas de `F/farm_load.c`: leitura de recursos.
- `F/event/event_script.c`: cursores, resultado e suspensão do roteiro.
- `F/event/event_commands.c`: mensagens, espera de sprite, música e XA.
- `src/main/graphics/anim.c` e `src/main/sound/`: conclusão e backends.

`F/` significa `src/overlay/11_GF_FARM/`. Nenhum C de `src/` passou a ser
compilado diretamente; o reaproveitamento mais direto dos algoritmos e a
unificação dos comandos seguem a matriz, sem introduzir um runtime paralelo.

Os dois recortes de MA-05 estão concluídos: contratos públicos neutros, entrada
lógica, propriedade/identidade/resultados explícitos, adapters reais e T-SERVICOS
com resposta imediata/atrasada/erro/cancelamento, polling e buffers. MA-06 não
foi iniciada. As subdivisões do roadmap podem ser executadas na mesma entrega
quando a tarefa inteira estiver autorizada e for viável.

## Validação

Linux x86-64, Debug, SDL3 3.4.18 local, `HMBTN_TEST_GAME_DATA` apontando para `data/`:

- `cmake --preset native-debug`: aprovado.
- `cmake --build --preset native-debug`: aprovado.
- `ctest --preset native-debug`: **77/77**, zero falhas, **44,51 s**;
  50 testes independentes dos dados opcionais e 27 percursos com recursos locais.
- `native_services`: IDs, aquisição única, polling, buffers copiados, respostas
  imutáveis/repetidas, erro, cancelamento, rebind, canais e limite de tickets.
- `native_session_services`: provedores controlados sem SDL; mensagem/slots,
  animação, ação e XA atrasados; recurso recebido antes do ack; falha/cancelamento;
  cursor preservado e efeito único em inventário/economia após a conclusão.
- `native_services_playthrough`: um evento real completado com atraso pelo
  coordenador; dados de ações/textos iguais aos leitores anteriores; backend de
  música e XA não reinicia ao repetir polling; espera real de XA; falhas de
  música/arquivo terminais sem buffer parcial ou alteração de inventário/saldo.
- `native_game_input` verifica frame lógico, consumo único de pressed e held
  mantido. Os 74 anteriores passaram, incluindo abertura, visitas, primeira
  manhã, torta/ingredientes, conversas/presentes/rollback, funeral/nomeação,
  pesca, menus, áreas, sono, economia e inventário.

Windows/cross-build, Release, Android, sanitizers, inspeção manual e instalação
limpa não foram executados. Sanitizers continuam sem nova tentativa; faltavam
runtimes no ambiente registrado anteriormente. Testes não demonstram jogo
completo, fidelidade integral ou persistência em disco.

## Arquivos desta entrega

Implementação e build:

- `port/CMakeLists.txt`
- `port/audio/service_audio.c`
- `port/audio/service_audio.h`
- `port/audio/title_audio.h`
- `port/core/app.c`
- `port/filesystem/service_resources.c`
- `port/filesystem/service_resources.h`
- `port/game/services.c`
- `port/game/services.h`
- `port/game/session.c`
- `port/game/session.h`
- `port/graphics/scene_preview.c`
- `port/input/game_input.c`
- `port/input/game_input.h`

Testes novos e ampliados:

- `port/tests/game_input_test.c`
- `port/tests/services_playthrough.c`
- `port/tests/services_test.c`
- `port/tests/session_services_test.c`

Documentação:

- `docs/port/ARCHITECTURE.md`
- `docs/port/PORTING_MATRIX.md`
- `docs/port/PORTING_STRATEGY.md`
- `docs/port/ROADMAP.md`
- `docs/port/ROADMAP_PENDENTE_CODEX.md`
- `docs/port/ROADMAP_STATUS.md`
- `docs/port/SERVICE_CONTRACTS.md`

Alterações locais preexistentes foram mantidas. `SESSION_STATE.md` e
`SESSION_EVENTS.md`, já não rastreados no início desta tarefa, permaneceram
byte a byte iguais. Nenhum arquivo em `src/`, `sdk/`, `disc/`, `tools/` ou no
CMake PS1 foi alterado nesta entrega. Recursos proprietários não foram adicionados.
