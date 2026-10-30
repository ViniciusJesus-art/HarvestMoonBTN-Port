# PF-06 — Apresentação do preparo e indicadores da cozinha — 2026-10-29

Entregue a sequência original de confirmação, fechamento do texto, fade para
preto, consumo/resultado único, faixa XA e retorno à mensagem de resultado.
Os painéis de seleção/receita agora mostram utensílios, temperos, quantidades
e faltas nas posições originais. O contador de receitas da ficha continua
derivado de `game.cooking`, sem um segundo contador persistente.

## Auditoria C/BIN

- `menu/house.c:cooking_cook_confirm_tick` e
  `cooking_record_confirm_tick`: o preparo só começa depois de fechar a
  confirmação; `fade_start_out(10,0,2)` precede `cooking_apply_result`.
- `cooking_apply_result` espera `fade_is_settled`, consome os ingredientes,
  coloca o prato na mão, escolhe a mensagem pelo registro/qualidade e pede
  `CD_TRACK_COOKING_RESULT=0x16`. `cooking_menu_tick`, passo 8, espera
  `CD_AUDIO_IDLE` e só então chama `fade_start_in(10,0,2)`. Não há uma ANI de
  personagem preparando comida nessa sequência; o preparo ocorre sob preto.
- O overlay instalado `data/A_FILE/11_GF_FARM.bin`, SHA-1
  `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`, confirma a consulta de estado em
  `0x80100010`, fade-in em `0x8010002c`, fade-out da confirmação em
  `0x80102334`, e chamadas da faixa em `0x80102924`/`0x80102958`.
  A segunda chamada carrega `a0=0x16` no delay slot. Os endereços são números
  PS1 usados na auditoria; nunca são convertidos em ponteiros host.
- `main/graphics/screen_fade.c`: fade-out tem comprimento 11 e fica pronto
  no nível 12; fade-in começa no nível 9 de comprimento 10. A sombra usa
  divisão inteira 20.12 e mistura subtrativa. Estado avança no tick do jogo.
- `main/sound/cd_audio.c:cd_audio_tracks[22]` é
  `{0x00e9,3,3}`: arquivo `04.XA`, filtro de arquivo 1/canal 3. A tabela de
  26 linhas aparece uma vez no executável instalado `SLUS_011.15`, offset
  `0x3a280`. O canal local tem 53 setores de áudio e termina no setor 419
  com flags `0xe4`; codificação estéreo de 37.800 Hz. A tabela numérica é
  extraída do C pelo gerador já existente, não duplicada como recurso binário.
- `cooking_draw_pending_flags`, `cooking_draw_utensils`,
  `cooking_draw_value_grid` e `cooking_row_input`: dois quadros 270×74,
  utensílios selecionados acima/disponíveis abaixo, oito células de passo 34,
  conjunto de temperos como grupo `0xf80`, painel de cinco temperos e cursor.
- `cooking_draw_timer_grid`/`item_anim_draw`: badge de quantidade com ANI 10,
  captions de origem, marcas de seleção. `item_effect_submit` usa ANI 11,
  quadro 0; `item_empty_marker_draw` usa quadro 2. A checagem de receita
  marca ingredientes/utensílios ausentes e a lista distingue indisponíveis.
- `cooking_grid_input`: esquerda/direita envolvem a própria linha de quatro;
  cima/baixo trocam a linha. Triângulo mostra a descrição do utensílio.
  `cooking_record_list_tick`: Start fecha e Circle volta à pergunta anterior.
- `cooking_result_repeat/equal/better/worse/fiasco_tick` e
  `cooking_record_store`: nova receita é lembrada após fechar a mensagem;
  melhor/pior só substituem o registro com a resposta positiva, após fechar.
  Repetir registro conserva a qualidade salva. Fiasco não é registrado.
- `status_pages.c:status_load_sprites/status_draw_summary`: contagem de
  receitas e contribuição `/8` no percentual; o porte já derivava esses
  valores da mesma coleção. A entrega valida sua atualização real no status,
  sem inventar indicadores de utensílios na ficha que o original não possui.

Este conjunto local tem traduções/modificações; a auditoria dos recursos
instalados não equivale à validação SHA-1 exigida pelo build PS1 original.

## Adaptação

`house_interaction` acrescenta somente fases transitórias. A confirmação
verifica disponibilidade, fecha o texto e inicia o fade. O commit usa
`hmbtn_cooking_make` uma vez ao escurecer; não se reimplementam receitas,
inventário, consumo mochila→geladeira ou avaliação de qualidade. Falha de
disponibilidade no commit volta à seleção sem consumir parcialmente.

`runtime.c` solicita START_XA/WAIT_XA ao serviço de áudio existente. O mesmo
stream/mixer/decodificador usado pelos eventos recebe a faixa 22, com os
metadados da tabela original. Pedidos são reconhecidos antes do adaptador de
eventos, evitando apropriar o resultado de outro consumidor. Ausência/erro de
recurso é erro explícito; não há substituição por silêncio ou temporizador
que finja uma faixa carregada. Cues 4/5 da infância permanecem suportados;
os demais IDs não foram habilitados sem consumidores implementados.

O renderer aplica a sombra depois do fundo, ícones e mensagem. Desenhar não
avança fade, stream, inventário ou receita. A tela de resultado conserva o
fundo/mensagem do original; foi removido o cartão de prato adicional do porte.
`cooking_item_stored` fornece uma consulta pura para marcas de falta; seleção
e preparo continuam usando as validações já existentes.

Não foi necessário alterar o codec: as fases novas são transitórias e o save
já recusa menus/ações em andamento. O estado estável continua com os mesmos
campos de utensílios, registros e inventário. Não houve trabalho de migração
ou política de compatibilidade. Suas alterações de portas foram preservadas
pela comparação do bloco completo TRANSITION/DOOR com a cópia inicial.

## Validação

- Debug: configure/build; seleção de regras **7/7**, incluindo a nova máquina
  de preparo, XA, saves, duas casas e mensagens/interação. O teste de fases
  verifica ausência de consumo na confirmação/fade, commit único, bloqueio
  até reconhecer áudio, retorno, receita e navegação original.
- `xa_test`: setores sintéticos próprios validam arquivo/canal/volume da
  faixa 22, PCM, quatro ticks de início, redução por 2 e fim após o countdown;
  mantém a cobertura das faixas 4/5. Nenhum áudio proprietário foi adicionado.
- `cooking_playthrough`: carrega utensílios comprados pelo percurso completo
  da TV/telefone/Zack. Sete preparos reais cobrem nova receita, melhor/pior
  aceitos/recusados, repetição salva e fiasco. Verifica consumo único, prioridade
  da mochila, saldo intacto, relógio pausado, espera de um único
  ticket XA, fases/fade e tela totalmente preta durante a faixa.
- A sondagem independente decodifica o canal original até seu setor final e
  exige energia PCM não nula. Os sete preparos usam o serviço/stream real;
  não são liberados por um flag de áudio fabricado pelo teste instalado.
- Desenho repetido no mesmo tick conserva fase, inventário e volume XA.
  Ingredientes realmente esgotados produzem a marca de falta; ficha mostra
  duas receitas, e o checkpoint final é retomado com as mesmas qualidades.
- Wrapper em processos separados: compra os oito utensílios e Power Berry,
  executa o percurso de cozinha e retoma pelo aplicativo. Passou em Debug.
  Portas/chuva e fidelidade do mundo também passaram. As duas regressões de
  ampliação foram ajustadas à espera real (antes esperavam prato imediato)
  e à navegação original dos temperos; seleção final dessas casas **2/2**.
- Release: configure/build, regras/decodificador **2/2**, percurso instalado
  completo e retomada pelo aplicativo aprovados.
- Windows x64: build no contêiner local sem rede; regras, XA, percurso e
  retomada pelo aplicativo aprovados sob Wine. Capturas dos painéis de receita,
  falta de ingrediente e ficha foram inspecionadas. Logs locais em
  `/tmp/hmbtn-cooking-*.log`; capturas em `/tmp/hmbtn-cooking-shots/`, fora do Git.

Foram contabilizados **45 processos de teste/lançadores**, incluindo tentativas,
wrappers e filhos, dentro do limite de 50. Suíte inteira, sanitizers,
instalação limpa, Windows físico e validação de janela/áudio físicos não foram
executados. O stream continua com interpolação linear já usada no porte;
a interpolação XA por hardware do PS1 permanece uma diferença documentada.

Reprodução com dados locais extraídos:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(cooking_presentation|xa_audio|cooking_playthrough)$'
```

O bootstrap `--diagnostics` não executa o jogo. O runtime continua parcial;
esta entrega não conclui PF-06 nem substitui o build/toolchain/SHA-1 PS1.
