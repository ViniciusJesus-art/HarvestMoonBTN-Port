# PF-06 — Utensílios por TV Shopping — 2026-10-29

Implementado o percurso de oferta na TV, ligação da hospedaria de Doug,
compra, entrega de Zack, aparelhos na casa e uso dos utensílios nas receitas.
Não se concedem utensílios ao ampliar a casa. PF-06 continua em andamento;
indicadores completos da ficha e a animação/som originais do preparo ainda
precisam de um recorte próprio.

## Referência C e BIN

- `event/messages.c`: `message_refresh_clock`, `message_pick_calendar_row`,
  `message_pick_ending`, `ending_offer_advance` e `ending_offer_confirm`.
  Após a primeira ampliação, sábado oferece o primeiro dos nove slots ativos,
  com mensagem `3:0x258+slot` e programa gráfico 2. Antes das 6h,
  tempestade/nevasca e programas especiais do início da primavera/fim do
  inverno têm prioridade. Abrir a TV prepara os quatro canais; só chegar ao
  fim da mensagem da oferta muda OFFERED para ACCEPTED. Trocar de canal ou
  sair antes não basta. A rotação usa o stamp de data u16 e só ocorre uma vez
  por data assistida, mantendo as assimetrias do original.
- `people/player.c`: `ZONE_FEE_DESK=21`, `player_start_request` e
  `player_act_zone_front`. Zona instalada em 204..207, índice 3. O telefone
  precede o descarte de um item na mão, sem consumi-lo.
- `menu/shop_dialogues.c`: `ending_offer_begin/find/buy`, `shop_fee_step`,
  `ending_offer_answer` e `shop_option_items`. Fora de sábado ou sem oferta
  assistida usa `2:0x11a`. Confirmar `0x11b` debita 10 G; `0x11c` conecta;
  `0x11d+slot` pede a segunda confirmação. Compra aceita usa `0x126`,
  cancelamento depois da ligação usa `0x127`, saldo insuficiente usa `0x4f`.
  Cancelar ou não conseguir pagar o produto conserva a taxa já debitada e
  permite tentar novamente. Cada nova ligação cobra seus próprios 10 G.
- No overlay instalado `11_GF_FARM.bin`, SHA-1
  `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`, a sequência little-endian
  `shop_option_items={0x72..0x79,0x67,0}` aparece uma única vez em `0x7042c`,
  endereço PS1 `0x8012824c` com base `0x800b7e20`. Esta identificação não é a
  validação SHA-1 do build PS1; o conjunto local contém traduções/modificações.
- Scripts instalados 290..297 em `12_event_s.bin`: Zack/ANI `0x201a`, banco
  `0x8000` e bytecode `0x4000`. Os oito scripts preservam fade, posicionamento,
  mensagens `1:0x370` e `1:0x372..0x378`, despedida `0x371`, caminhada do jogador e saída de Zack.
  Despacham `EVENT_CMD_UTENSIL_GET(0..7)` apenas no fim da entrega.
- Gatilhos originais da fazenda: condições 84..91 leem flags `0x67..0x6e`;
  modo 2, lugar anterior 2, dias da semana 2..5 e horas 6..11. O campo de
  calendário `0x3ffffcc6` e flags `0x79ec3800` produzem entrega de terça a
  sexta, entre 6h e 11h59, ao sair da casa. Não existe contador de três dias
  neste fluxo. O motor comum de gatilhos aplica também os demais campos.
- A nona oferta é a Power Berry, 5.000 G. O script instalado 298 usa condição
  92/flag `0x6f`, pede mãos vazias e executa `receive_item(0x67,0,0,0)`.
  `player_receive_item` escolhe comer pelos flags `0x1200`; o porte usa a ANI
  base 5/som 74, aumenta o máximo em 10, repõe energia, reduz fadiga em 1 e
  conta a fruta uma vez. O script instalado termina com `ESTATE_BITS(0)`,
  preservado; sua flag de evento concluído evita repetição. Não se substitui
  esse comando pelo `UTENSIL_GET(8)` que existe no C mas não é usado aqui.
- `estate.c`: `estate_items_restore`, `estate_gate_results_place`,
  `estate_result_rows_house1/2` e seleção de ANI 4..11. As posições das oito
  peças são extraídas das tabelas, sem copiar recursos gráficos para o Git.
- `main/text/text.c:text_draw_face` ignora banco -1. As mensagens do telefone
  têm FF6/FF4/FF5 sem escolher um retrato. O renderer agora respeita isso,
  evitando desenhar textura inexistente ou herdar a imagem de outra conversa.

| Ordem | Item | Preço, além da ligação |
|---|---|---:|
| 1 | Faca | 3.000 G |
| 2 | Frigideira | 2.500 G |
| 3 | Panela | 2.000 G |
| 4 | Liquidificador | 2.500 G |
| 5 | Batedor | 1.000 G |
| 6 | Rolo | 1.500 G |
| 7 | Forno | 5.000 G |
| 8 | Temperos | 5.000 G |
| 9 | Power Berry | 5.000 G |

Preços vêm do catálogo comum, extraído de `farm_tables.c:item_info`.

## Adaptação

`game/tv_shopping.c/.h` adapta a rotação/confirmação/compra, com estado em
`game.shopping`. Flags pendentes permanecem no estado de eventos da vila;
`game.cooking.utensils` continua sendo a única posse de utensílios. O modal
existente de casa incorpora o telefone, emprestando economia, construção e
vila da sessão. Não há outro inventário, relógio, contador de prazo ou menu
de compras independente. Taxa e produto passam pelo débito/ledger comum.

O runtime executa os scripts originais e só adiciona os comandos de domínio
faltantes. O mesmo carregador valida ANI/banco de Zack, o mesmo controlador
move os atores, e os mesmos serviços carregam mensagens e áudio. A entrega
libera opções da cozinha e os sprites do cenário pelas mesmas oito flags.
O renderer consulta posse e layout; desenhar não altera gameplay.

O codec acrescenta a data/slots da oferta ao estado que já guarda utensílios,
receitas e flags de eventos. A versão corrente é v16; não foi aberto trabalho
de compatibilidade/migração de saves. Pedidos, posse e receitas sobrevivem à
retomada na versão corrente.

As alterações de portas presentes no início desta rodada foram preservadas:
comparação do bloco completo TRANSITION/DOOR contra a cópia inicial confirmou
igualdade, incluindo recuo 30, aceleração 8, ângulo e tempos editados pelo usuário.

## Validação

`tv_shopping_test.c` verifica disponibilidade, revisão da mesma data, rotação,
estado assistido, esgotamento das nove ofertas, stamp u16, pagamento sem saldo,
duas confirmações, cancelamento por opção/Circle, taxa retida, flags e retomada.

`tv_shopping_playthrough.c` usa mensagens, mapas e ANI instalados: casa sem
ampliação, saída precoce da TV, chamada sem oferta, item na mão, nove semanas
de ofertas/compras/entregas, ausência de entrega sábado/segunda, limite 11h59/
12h e lugar/mode de entrada, nenhuma posse antes da entrega e nenhuma entrega
duplicada. A faca recebida libera a seleção real; preparar salada consome o
pepino da geladeira e registra a receita. Seu preço de remessa original é
zero: jogá-la na caixa não inventa receita financeira. A Power Berry é comida
uma vez. As duas cozinhas desenham seus aparelhos nas posições originais.

O wrapper `tv_shopping_process.py` retoma snapshots pendente/concluído pelo
aplicativo em outros processos. Configuração e build Debug/Release passaram;
seleção Debug 8/8 (incluindo portas/chuva e fidelidade do mundo), wrapper 1/1,
seleção Release 2/2. Capturas do telefone, TV, entrega, receita e duas cozinhas
foram renderizadas; telefone e segunda cozinha inspecionados visualmente.

Build Windows x64, unidade/percurso sob Wine e duas retomadas pelo aplicativo
aprovados. O percurso final com item na mão foi repetido em Debug, Release e
Wine. Até 49 processos de teste nesta rodada, incluindo tentativas, wrappers,
filhos e lançadores; sem suíte inteira.
Logs locais desta rodada: `/tmp/hmbtn-utensils-*.log`; capturas fora do Git em
`/tmp/hmbtn-utensils-shots/`.

Reprodução, com dados locais já extraídos:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_tv_shopping(_playthrough)?$'
```

Não foram executados suíte inteira, sanitizers, Windows físico, instalação
limpa nem validação de janela/áudio físicos. O bootstrap `--diagnostics` não
executa o jogo. O runtime é parcial; o build host não substitui o build PS1
com toolchain e SHA-1 originais.
