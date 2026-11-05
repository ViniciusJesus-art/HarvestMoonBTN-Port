# PF-06 — Ofertas à Deusa

Entregue em 2026-11-04 para o runtime nativo parcial Linux/Windows.
O bootstrap de diagnóstico continua sem executar o jogo. O porte não exige
recompilar o PS1; a validação host não substitui toolchain/SHA-1 originais.

## Referência original e escopo

A origem é `people/player.c:thrown_item_settle`,
`event/event_commands.c:event_trigger_mode_match/event_cmd_check_gift`,
`event_cmd_pick_bride`, `farm_nudge_axis`, `event_cmd_cue_reset`, testes de
portas, despacho de recompensas e encadeamento de eventos. `event_script.c`
fornece a tabela de comandos e o carregamento de atores. Esses arquivos de
`src/` permanecem intactos; as adaptações estão em `port/`.

No `00_gf.bin` instalado, a tabela sazonal da área 72 contém o evento **182**,
modo **4**, destinatária **33**, flags **0x7ff88000** (inclui tempo ensolarado
como condição de clima), e o bit de repetição. O primeiro volume ativo que
contém a posição de pouso vence, com bordas X/Z estritas e tolerância Y
original. Apenas o pouso na água da zona **44** produz uma oferta; a zona 38
continua reservada aos ovos termais. O item é consumido pelo arremesso normal,
com ANI 23 de respingo e som 16, mesmo quando o presente/clima não inicia evento.
Embrulhos usam seu conteúdo, sem consultar a mão já esvaziada pelo arremesso.

O bloco 182 em `12_event_s.bin` ocupa **0x43d800..0x442800** e contém 85
instruções. São executadas diretamente pelo interpretador existente, sem
copiar diálogos, reescrever contagens ou substituir escolhas por um fluxo novo.
O script chama os eventos **185/187/189/191/193** para as cinco candidatas.
A seleção mantém ordem original (Karen, Ann, Elli, Mary, Popuri), maior afeição
e desempate pelo RNG da vila, além dos filtros de disponibilidade existentes.
O estado familiar atual do porte é solteiro; casamento continua fora deste recorte.

## Comportamento entregue

- Reação original zero para a destinatária aceita a oferta; outros itens,
  tempo inadequado e arremesso em alvo seco não avançam a contagem.
- O contador continua em `village.days.values[1]`. A primeira oferta passa
  de zero a dois pelo próprio bytecode; os marcos equivalem às ofertas 5/10/20.
- Na quinta oferta: mensagem 540, apresentação/consumo da Power Berry,
  energia máxima +10, recuperação e incremento da ficha. O bit 0x100 do
  original fica em `game.progress.goddess_berry`.
- Na décima: escolha, seleção da candidata e um dos cinco roteiros originais.
  Entrar/sair do banho dentro da espera produz os encontros e +2.000 de
  afeição. Recusar, sair da área ou perder o prazo segue os desvios do roteiro.
- Na vigésima: mensagem 538, madeira apresentada, Gotz com falas/movimento,
  retirada da madeira e pagamento de 1.000 G. O contador satura em 21, sem
  repetir a Power Berry ou o pagamento nas ofertas seguintes.

O domínio novo só resolve os comandos necessários aos roteiros. Atores são
empréstimos da sessão, não cópias permanentes dos moradores. A música passa
pelo provedor existente mantendo o banco de efeitos da fazenda; encerramento
restaura a música do local. Os ícones usam o desenho de cues já existente.

As duas faces da cabine compartilham o mesmo modelo, mas os roteiros esperam
os eventos 46 e 47 em momentos distintos. `area_doors` conserva a identidade
da última solicitação, sem alterar aceleração, ângulos ou animações do usuário.
Durante o encontro, a porta e a caminhada automática terminam mesmo quando
o script suspende os controles. O banho não emite tickets de animação de
roteiro para suas próprias trocas de banco/pose. A animação do respingo também
termina durante o evento, em vez de aguardar a devolução dos controles.

## Estado, arquivos e validação

O codec foi ajustado somente para o novo bit de progresso (v23); contador,
energia, dinheiro, afinidade e RNG já possuem seus donos/serialização. Cues,
portas, atores temporários e intenção de oferta não são serializados. A guarda
de save impede gravar uma oferta ainda não despachada.

Arquivos centrais: `game/goddess.[ch]`, `farm_carry.[ch]`, `farm_visit.c`,
`farm_intro.c`, `session.[ch]`, `runtime.c`, `exploration.c`, `area_doors.[ch]`,
`game_state.h`, `save.[ch]` e `graphics/scene_preview.c`. Sem novos recursos
proprietários no Git e sem alteração ao bloco de travessia de portas do usuário.

`goddess_test` cobre água/zona anterior/bordas, conteúdo embrulhado, pouso único,
reação/clima/modo, seleção de candidata e persistência do progresso/contador.
`goddess_playthrough` compra quatro pacotes de nabo (480 G), prepara quatro
canteiros, semeia, rega e dorme quatro noites antes de colher as ofertas.
Posições/dinheiro inicial são fixtures; não há concessão de produto maduro.
Verifica erro de alvo, chuva, 22 ofertas e os três marcos, ausência de repetição,
as cinco variantes de encontro, banho, prazo e recusa. Afinidades e contador
são fixtures apenas para repetir os ramos alternativos após concluir o percurso
principal. O wrapper abre o aplicativo em processos novos depois da Berry e
depois do pagamento, a partir dos arquivos gerados pelo percurso real.


Validação executada:

- Configure/build Linux Debug e Release aprovados. Seleção Debug **8/8**;
  regressões adicionais de mundo/chuva/portas/águas **3/3**. A unidade ampliada
  passou na repetição isolada **1/1** após restaurar a estação do fixture.
- Release: percurso com duas retomadas em processo novo e águas aprovados;
  unidade ampliada corrigida também aprovada **1/1**. A seleção inicial
  terminou 2/3 por causa da mesma inconsistência de estação no fixture.
- Build Windows MinGW sem rede aprovado. Unidade da Deusa, percurso completo
  (cultivo, 22 ofertas, cinco encontros, banho/prazo/recusa, recompensas e
  reload) e regressão da fonte aprovados no Wine.
- Capturas de Deusa/falas/recompensa e candidata inspecionadas; comparação
  textual do bloco de travessia de portas com o início da rodada idêntica.
  `git diff --check` aprovado.

**49 processos de teste nesta rodada**: 34 Debug (execuções diretas, duas
seleções, wrapper/filhos e repetição isolada), nove Release e seis
lançadores/targets Wine. A contagem inclui as tentativas de desenvolvimento
que detectaram o nudge vertical, espera de portas compartilhadas, ticket de
animação do banho, banco de áudio e fixture da unidade. Teto 50 respeitado.
Sem suíte inteira, sanitizers, instalação limpa, sessão/janela/áudio físicos
ou Windows físico. Wine não equivale a validação em hardware Windows.

Próximo recorte: evento 197/Kappa e efeito real da Mystic Berry, conforme o
roadmap. A entrega atual não declara PF-06 nem o jogo completo concluídos.
