# PF-06 — Portas internas, chuva, coleta, descarte e venda de animais

Data: 2026-10-28. **Revisão em 2026-10-29:** [PF06_INTERACTION_FIXES.md](PF06_INTERACTION_FIXES.md) corrige a caminhada das portas, mensagem/cursores e devolução de pedra/galho ao plantio. Adaptações em `port/`, comparadas com o C de `src/` da
árvore atual. O jogo completo ainda não executa no host; o bootstrap
`--diagnostics` não executa o jogo. O build host não substitui toolchain e
validação SHA-1 do original PS1. Nenhum recurso proprietário foi adicionado.

## Correções solicitadas

| Origem / diferença encontrada | Adaptação e verificação |
|---|---|
| `farm_zones_register_script`, `farm_zone_trigger`, `world/doors.c`: quartos usam zonas `0x1000`, destino `-2` (`0xfffe`), evento e modelo. O host exigia um destino de área válido e ignorava essas zonas. | `house_control.c` reconhece a sentinela; `runtime.c` aplica as recusas originais e solicita `area_doors.c`, sem viagem. Os dois lados compartilham o modelo; aceleração 8, giro `0x420`, espera/fechamento e sons já existentes. Teste instalado abre/fecha 18 lados nas áreas 156/192/196/204/208, incluindo quartos da pousada, carregando item; confirma recusa da clínica por amizade. |
| `weather_fx_frame` / `particle_step`: suspensão do relógio não é pausa de partículas. O host congelava chuva durante abertura de portas, controle por evento e mensagens. | `exploration.c` separa a pausa visual da pausa do relógio e mantém também a transição da luz. `scene_preview.c` aplica scroll da câmera mesmo durante controle por roteiro. Interior/estufa e menu continuam respeitando os limites do recorte. Teste usa porta exterior e evento real 128, avança partículas com relógio parado e compara um pixel de chuva no evento com paleta/luz originais; draw repetido não altera estado. |
| `player_probe_target`, `farm_tile_classify`: a célula é consultada pela posição à frente. O host primeiro rejeitava os pés fora da grade. | `farm_carry.c` consulta diretamente o ponto à frente, com o alcance de interação 224 já usado pelo porte. Unidade testa quatro bordas, fazenda/estufa e rejeição distante; percurso instalado pega uma pedra na primeira coluna com os pés fora. A animação/coleta/inventário continuam compartilhados. |
| `thrown_item_resolve_target`, `thrown_item_update`, `picture_load_to_vram` / `picture_cell_draw`: no chão, HOLD por 1 tick, BLINK por 4, segunda parte da figura e retirada. O host removia no impacto e replantava certos objetos soltos. | `HmbtnThrownItem.phase/timer` pertencem ao tick da sessão; movimento termina no impacto, a segunda faixa ANI do próprio item é mostrada com a cadência original e o slot é retirado. Revisão de 29/10: pedras/galhos voltam a células livres do plantio; fora da grade, mantêm o descarte. Ervas seguem o descarte. Remessa continua usando superfície/preço reais e pagamento único. Capturas de pedra em voo/quebra/ausência; unidade testa fases, posição parada, contagem única e proteção de células ocupadas. |
| `cue_history_item_3/_7`, `villager_load_request`: agendas e recursos consultam flags/calendário, sem um novo sistema de rotas ou figurinos. | Comparados todos os 30×4 campos LE das agendas de Karen e Ann com as tabelas originais, em cinco climas: `0x780/0x870` e `0x23a0/0x2490`, respectivamente, na data ordinária de teste. Mantidos seletores de recursos/atores existentes; isto não atesta todas as agendas e aparências de todos os eventos. |

Os campos transitórios de descarte não são serializados. Save continua recusando
um item em voo ou desaparecendo, e aceita novamente quando o slot é retirado.
Formato atual permanece v11. Descarte especial em água/zonas, respingos de
aterrissagem, bola/cesta e afinidade por lixo fora da fazenda ainda não têm
paridade integral; esta correção verifica o descarte comum no chão. Clima
completo, trovões e neve durante menus continuam com limites anteriores.

## Continuação do roadmap: venda com Barley

Adaptados `menu/shop_dialogues.c:shop_barn_step`, `shop_livestock_find` com
filtro 0, `shop_record_list`, `shop_draw_text_row` e `shop_draw_panel`.
`livestock.c` seleciona diretamente registros adultos vivos da coleção única.
Bezerros/vacas jovens e cordeiros não entram; mortos conservam sua vaga para
funeral. Doença e lã recém-cortada não impedem a venda na fonte. Reprodução
ainda não existe: quando integrada, precisa acrescentar o guarda original de
gestação para vacas, sem inventar esse estado nesta entrega.

| Afeição / prêmio | Vaca | Ovelha |
|---|---:|---:|
| 0..100 | 3.000 G | 2.000 G |
| 101..200 | 4.500 G | 3.000 G |
| 201..255 | 5.000 G | 3.500 G |
| Premiada | 8.000 G | 6.000 G |

`feed_shop.c` integra as duas opções ao modal existente, com descrições
`0x69/0x6a`, mensagens de vazio `0x76/0x78`, lista `0x5a`, confirmações de
qualidade/prêmio e resultado `0x83` ou cancelamento `0x79`. A lista tem quatro
linhas, nomes/datas/corações e ícones originais, inclusive ovelha tosquiada.
Confirmação remove somente o registro selecionado e credita a economia/contas
uma vez; UI, mensagens e pressionamentos seguintes não repetem a operação.
O cursor de confirmação recebeu o alinhamento inicial de `shop_draw_panel`; a revisão de 29/10 acrescenta o deslocamento vertical de `text_draw_cursor` que faltava.

Não há nova coleção, contador, moeda ou formato de save. As referências
centrais anteriores foram preservadas, assim como as alterações preexistentes
de ferramentas animais, inventário, documentos, CI e READMEs.

## Validação e reprodução

- Configure/build Linux Debug e Release e cross-build Windows x64 MinGW
  Release aprovados, reutilizando SDL3 local, sem rede.
- Seleção Linux Debug: **21/21 CTests**, **6,50 s**; protege criação/ferramentas,
  lojas, inventário, saves, exploração, portas/clima, pesca, interiores e remessas.
  Inclui aquisição e ferramentas animais com retomada em aplicativo novo.
- Novos `native_livestock_sale`, `native_livestock_sale_playthrough` e
  `native_world_fidelity_playthrough` aprovados. Venda instalada verifica
  vazio, página, cancelar lista/confirmação, vaca doente, ovelha premiada
  tosquiada, preços, preservação de vizinhos/cochos, compra real com nomeação
  reutilizando vaga e save carregado em outro processo pelo aplicativo.
- Após inspeção visual, o percurso de descarte foi refinado para capturar a
  pedra `ITEM_STONE=0x3a` e o cursor da confirmação foi alinhado à fonte. Os dois percursos
  dirigidos passaram novamente. Unidade de venda passou também em Release.
- A CI automática passa a selecionar 12 casos por configuração, 48 na matriz
  Linux/Windows Debug/Release; não é execução comprovada da CI remota.
- O percurso Windows de fidelidade do mundo passou no Wine com recursos reais.
  A tentativa anterior de iniciar a unidade no Wine foi bloqueada pelo socket
  do sandbox antes do teste; execução do percurso fora dessa restrição passou.
  Orçamento de até 50 processos de teste respeitado, incluindo tentativas,
  wrappers/filhos e contagem conservadora de Wine/aplicação.

Falhas iniciais de fixtures foram corrigidas: alcance na última coluna deve
respeitar a conversão original por 399; abertura de quartos depende também de
afeição/horário. O teste de pesca passou a esperar a retirada posterior do item.
Uma compilação de teste detectou includes ausentes e foi corrigida antes da
validação final. Não foi executada a suíte inteira. MSVC/Windows real, áudio e
janela físicos, sanitizers e instalação limpa não foram validados.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(livestock_sale|livestock_sale_playthrough|world_fidelity_playthrough)$'

SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_world_fidelity_playthrough data
```

Logs locais: `/tmp/hmbtn-fidelity-final-tests.log`,
`/tmp/hmbtn-fidelity-playthrough.log`, `/tmp/hmbtn-sale-playthrough.log`,
`/tmp/hmbtn-fidelity-release-tests.log` e
`/tmp/hmbtn-fidelity-windows-build.log`, com execução em
`/tmp/hmbtn-fidelity-windows-world.log`. Capturas em
`/tmp/hmbtn-fidelity-shots/` e `/tmp/hmbtn-sale-shots/`; save de demonstração
da venda em `/tmp/hmbtn-livestock-sale.hms`. Recursos continuam vindo do BIN
instalado pelo usuário e ficam fora do Git.

PF-06 permanece aberta. Próximo recorte: reprodução bovina/ovina, começando
pela aquisição/uso de poções e reserva real da vaga do filhote.
