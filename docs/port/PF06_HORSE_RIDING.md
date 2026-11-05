# PF-06 — Cavalo adulto e montaria

Recorte de 2026-11-05 para Linux e Windows. Complementa o
[potro inicial](PF06_HORSE.md), no mesmo registro da sessão. PF-06 e runtime
continuam parciais; o bootstrap de diagnóstico não executa o jogo.

## Referência C/BIN

Consultados `animals/horse.c`, `people/player.c`, `event/event_commands.c`,
`farm_init.c`, carregamento dos animais e os eventos instalados **2/3** de
`12_event_s.bin`. Não se alteram o bytecode nem os recursos proprietários.

- `horse_update_bond` promove o potro quando o ano armazenado difere do ano
  atual, a estação coincide e o dia atual alcança o dia da aquisição.
  Não usa um contador arbitrário de noventa dias. Afeto, nome e treino
  pertencem ao mesmo animal antes/depois da promoção.
- `farm_load_anim8(0)` fornece o adulto em `00_gf.bin`, offset **0x37e3800**,
  comprimento **0xa000**, ANI/banco em **+2592**. O potro conserva seu span.
  `horse_frame_table` seleciona repouso, caminhada, corrida e salto.
- Tabela da fazenda em **0xDD800**: eventos **2/3**, procedência **2**
  (casa), modo **2**, 06:00–11:59, condição de adulto e vínculo **50**
  (cavalo), limiar **180**. O evento 2 aceita `<=180`; o 3 aceita `>=180`,
  mas a ordem da tabela seleciona **2 em 180**. As flags originais de
  pré-requisito/conclusão continuam sendo verificadas, incluindo o evento 1.
- Evento **2**: Barley e ator 27, entrada/saída da cocheira, falas **10/11/12**,
  saída dos atores, amizade **−30** para Barley/May e comando 49/1 que retira
  o cavalo. Evento **3**: falas **10/13/14/15/16**, gesto de concordância do
  jogador, amizade **+30**, restauração do animal e encerramento.
- Comando **92/0** esconde/restaura o animal persistente durante o encontro:
  o ator 27 do roteiro não cria outro cavalo. Comandos 23/13 com X=0 apenas
  aguardam o CD ficar livre; não iniciam uma reprodução inexistente. O gesto
  63/71 com X=−1 conserva a direção do jogador e usa o banco de ação existente.
- `player_confirm_action` monta somente no adulto, marca `TRAINED_TODAY`,
  copia sua direção e usa ANI **50+direção**, som **6**. Repouso montado usa
  ANI **54**, movimento **59+direção**, e o cavalo acompanha o jogador.
  Velocidade é escolhida pelo modo anterior: **73** se caminhando, **112**
  nos demais casos. O controle a pé mantém suas velocidades anteriores.
- Cross inicia a desmontagem **ANI 176**. Ao terminar, o cavalo fica no
  ponto anterior; `player_calc_first_valid` procura uma posição livre a
  **300** unidades, na ordem norte/oeste/sul/leste. Sem opção livre, conserva
  a posição. O treino diário soma 1/2/3 conforme o afeto após os cuidados,
  antes da penalidade climática; montar repetidamente não multiplica o ganho.
- O modo de zonas montado permite **evento 3** (entrada da cocheira) ou
  **destino 0** (volta à fazenda). Não equivale a liberar todos os portões.
  A cocheira abre sem Cross; Cross serve para desmontar. Outras portas/saídas
  exigem desmontar. `player_morning_reset` retira RIDING antes de mover o
  jogador para casa; o cavalo conserva sua última localização real.

## Integração

`horse.c` estende o registro existente com adulto/montaria e uma fase
transitória de subir/descer. A sincronização entre cavalo e jogador acontece
nos ticks, incluindo a conclusão das trocas de área, sem depender do desenho.
O cavalo montado não colide consigo mesmo nem executa a caminhada autônoma.

`house_control` compartilha terreno, sondas, colisão e deslocamento entre as
variantes a pé/montada. A exploração conserva o fluxo de menus e transições,
mas impede ferramentas, assobios e trocas rápidas durante a montaria. As
rotinas personalizadas de abertura/travessia de portas de `runtime.c` foram
comparadas com a cópia anterior ao recorte e estão integralmente preservadas.
A apresentação usa ANI/banco adultos e retira o cursor de chão ao montar.

Os eventos executam o interpretador já existente, com seus atores, textos,
fades e esperas. Comandos de visita adaptam retirada/ocultação do animal;
transições carregam os mapas reais, sem encenar a inspeção por um fluxo novo.

Codec **v28** acrescenta adulto/montaria ao registro existente. Saves estáveis
montados retomam o mesmo jogador/cavalo; a fase de subir/descer é transitória.
A validação exige adulto pertencente à sessão, mesmo mapa/posição e mão vazia.
Nenhum trabalho de política de compatibilidade entre versões foi introduzido.

## Validação

- Linux Debug: configuração/build completos e oito verificações dirigidas
  aprovadas; seleção final **6/6**, incluindo ambos os percursos de cavalo,
  ferramentas animais, save, portas e gatilhos de visitas. Unidades de cavalo
  e controle também aprovadas. Não foi rodada a suíte completa.
- Percurso adulto: aquisição real pelo evento 1/nomeação; manhã de aniversário;
  inspeções **180/181** com mapas/falas/retirada ou gesto/restauração; ANI adulta;
  subir/caminhar/correr/descer, ocupação, restrições de portas/saídas/ferramentas,
  status congelado, ida/volta da cocheira, treino diário e amanhecer forçado.
- Saves comparados antes/depois de reabrir: montado na cocheira e na fazenda,
  desmontado depois de dormir e em casa após o amanhecer montado. Os quatro
  snapshots também foram abertos pelo aplicativo em processos novos.
- Linux Release: configuração/build dos alvos pertinentes, unidades **2/2**
  e percurso adulto completo aprovados.
- Windows: build MinGW dos alvos pertinentes; unidade, percurso completo e
  retomada montada pelo aplicativo aprovados no Wine. Windows físico não testado.
- Capturas locais inspecionadas do adulto/montaria na cocheira e fazenda.
  `git diff --check` aprovado. Fontes/SDK/disco/tools e fluxo PS1 preservados;
  validação host não substitui toolchain PS1/SHA-1.

A rodada usou **47 processos de teste**, incluindo tentativas, CTest,
wrappers/filhos e lançadores Wine, dentro do limite de 50. Builds, auditoria
estática e inspeção de imagens não entram nessa contagem.

## Limites e próxima entrega

Remessas usando o cavalo são a próxima entrega: auditar
`player_ship_thrown_item`, o alvo/callback 0x200 e integrar a colisão do
arremesso à remessa/economia já existentes. Saltos avançados/autônomos,
retorno/aquisição posterior à retirada, corrida e festivais continuam pendentes.
O controle montado reaproveita a sonda de salto simples existente; este recorte
não declara a auditoria integral dos saltos nem paridade completa dos cavalos.
