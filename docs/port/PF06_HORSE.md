# PF-06 — Potro inicial

Implementado em 2026-11-05, para Linux e Windows. Este recorte entrega o
recebimento e os cuidados do potro. PF-06/runtime continuam parciais; o
bootstrap de diagnóstico não executa o jogo.

## Referência C/BIN

Foram consultados `animals/horse.c`, `animals/animals.c`, `people/player.c`,
`farm_init.c`, `farm_load.c`, `menu/status_pages.c`, `event/event_commands.c`
e os eventos instalados **1 e 73** de `12_event_s.bin`. O interpretador executa
esses scripts; os diálogos, escolhas, movimentos e saltos do bytecode não
foram substituídos por uma sequência nova.

- Evento 1: encontro no rancho, área lógica 28, tabela em `00_gf.bin`
  `0xC38000`. Condição 0 exige ausência do cavalo e procedência 42/48.
  A própria tabela restringe ao primeiro ano, primavera, sol, 08:00–18:59,
  dias da semana habilitados (segunda excluída) e contador 8 fora de 0..3.
- Evento 73: visita à fazenda, tabela inicial em `0xDD800`, procedência 2,
  06:00–11:59, contador 8 fora de 0..3. Condição 31 soma pelo menos 200
  células com valor `0x36` e espécie diferente de zero, incluindo a estufa;
  exige ausência do cavalo.
- As duas entradas marcam seu evento como visto. No evento 1, recusar deixa
  o encontro ativo e permite conversar novamente com Barley antes de sair.
  Sair encerra essa oportunidade; a visita 73 também é única. Nenhuma flag
  é apagada para inventar repetição. A porta 8 mantém a recusa 461 enquanto
  o encontro no rancho está ativo, conforme o par transitório do script.
- Comando 49 cria `ITEM_PONY` (`0x15E`) somente se o registro estiver livre.
  Comando 33 abre a nomeação obrigatória existente; não aceita cancelamento.
  O nome é inserido nas mensagens de roteiro por `0x400B`.

`horse_spawn` começa no estábulo, área nativa 92/lugar original 3, posição
`(460,0,600)`, afeto 10. `horse_load_sprite` usa `farm_load_anim8(1)`:
trecho `0x37ED800..0x37F5800`, ANI de 2104 bytes seguida do banco. A leitura
continua limitada ao recurso instalado, com campos LE; nenhum recurso foi
incluído no Git.

## Integração

`session.horse` é o único registro persistente: ator, nome, área, data de
aquisição, afeto/treino, cuidados diários e cursores/RNG. Vila, status e
ferramenta animal recebem empréstimos desse registro. O estágio 27 do
encontro é o ator do roteiro; não cria outro animal na fazenda.

O movimento reaproveita o caminhador já adaptado para o cachorro, agora
exposto como `hmbtn_pet_walk`, com os mesmos pesos direcionais e sondas. Cada
animal conserva seu próprio ator, RNG e cursores. O potro usa a tabela de
poses original, caminhada de 12 e aproximação de 24 unidades por tick.
Ocupação participa das consultas do jogador, cachorro e cesta, incluindo
o bloqueio do jogador correndo. Desenhar não avança comportamento.

O botão de interação abre as mensagens originais 600/601, com o limiar de
afeto 201. Conversar não acrescenta cuidado diário. Escovar usa o banco,
ANI, som e custo da ferramenta animal existente, com alvo do potro, marca
diária e ícone de elogio. L2 usa a pose de assobio do jogador e o som 18;
a resposta testa `random(200) < afeto + 100` e inicia aproximação.

Os pedidos CD-XA 13/14 usam os canais originais 6/7 de `02.XA`; também
atendem a chamada 14 do evento 73. São iniciados pelo serviço de áudio
existente. Um início pendente é consumido antes do próximo pedido.

No coordenador diário, uma vez por data: +2 pela resposta ao chamado,
+2 pela escovação; −1 se não houve chamado/escovação/treino; −5 com clima
adverso somente quando o registro está na fazenda. Afeto e treino saturam
em 255. A regra de treino 1/2/3, por faixas de afeto 100/200 após os cuidados,
fica disponível para a futura montaria; assobiar não marca treino.

O quarto espaço do resumo da fazenda mostra nome/ícone/corações e participa
do total conforme `status_pages.c`. Pausa e modais congelam o potro. O codec
**v27** guarda os campos necessários e refaz empréstimos ao carregar;
nomeação, assobio em curso e pedidos de áudio são transitórios. Não houve
trabalho para impor ou impedir compatibilidade entre versões de save.

## Validação

`native_horse` cobre ausência inicial, aquisição única, limiares, repetição
de cuidados, clima, L2, ocupação, persistência e deduplicação diária.
`native_horse_playthrough` executa os dois scripts instalados: entrada real
52→28, recusa e nova conversa no rancho, nomeação sem cancelamento, saída,
porta real do estábulo, conversa/escovação/L2, caminhada, pausa/status,
manhã e reabertura. A visita posterior testa recusa sem nova entrega,
e aceitação em uma sessão independente. Predicados negativos usam a tabela
instalada: ano/estação/horário/clima/procedência, animal já existente e
limiar 199/200 com contribuição da estufa. Duas retomadas abrem processos
novos do aplicativo; capturas de estábulo/caminhada/status foram inspecionadas.

Linux Debug: as 12 verificações selecionadas foram aprovadas, com
reexecução do percurso do potro após os ajustes. Linux Release: 3/3
(`native_horse`, `native_horse_playthrough`, `native_water_playthrough`).
Windows: build MinGW, unidade e percurso completo no Wine aprovados;
o aplicativo abriu o checkpoint do estábulo em um processo novo. O percurso
Windows também reabriu os checkpoints interno e após a manhã no próprio driver.

Foram **49 processos de teste**: dez execuções diretas iniciais, 18 processos
na primeira seleção Debug, três na tentativa dirigida seguinte, cinco na
reexecução Debug aprovada, sete em Release e seis no Wine (lançadores e
executáveis). As falhas iniciais expuseram o comando de câmera sem registro,
a habilitação dos canais XA e ajustes nos próprios testes de alcance/tempo
assíncrono. Builds e inspeções de recursos/capturas não entram nessa conta.

Os testes dirigidos incluem
cachorro/bola, ferramentas animais, save, estado da sessão, variáveis/eventos,
portas/clima e águas. A contagem inclui tentativas, CTest, wrappers, filhos
e lançadores Wine, respeitando o teto de 50 processos por rodada.

O bloco de travessia de portas de `runtime.c` foi comparado com a cópia
anterior a este recorte e está idêntico. `src/`, `sdk/`, `disc/`, `tools/`
e o fluxo PS1 foram preservados; o build host não substitui toolchain/SHA-1
do original.

## Limites e próxima entrega

Crescimento/adulto, inspeção/devolução de Barley e montaria foram entregues
no [recorte seguinte](PF06_HORSE_RIDING.md). Remessas pelo cavalo, saltos
avançados/autônomos, retorno após retirada e corrida continuam pendentes.
A fila atual está no [roadmap](ROADMAP.md).
