# Compra de forragem e ração

O recorte das lojas permite comprar forragem de Barley no rancho (área 136) e
ração de Lillia na granja (128). Z/Cross inicia o atendimento ao alcançar o
comerciante com as mãos vazias. A presença vem da agenda e da posição real do
morador; o comércio não funciona à distância nem com o vendedor ausente.

Os ramos iniciais dos scripts originais 365/368 em `12_event_s.bin` verificam:

- Barley: 09h inclusive até 15h exclusive, fechado na segunda-feira.
- Lillia: 11h inclusive até 16h exclusive, fechado no domingo.

Os horários de acesso à casa continuam separados das condições de atendimento.
As portas ainda podem exigir presença/afinidade. Estar dentro do edifício não
reaplica a regra da porta ao falar com o vendedor.

## Fluxo e estado

`port/game/feed_shop.c` adapta o ramo de ração de `shop_dialogues.c`: saudação,
seleção, descrição, quantidade, compra/recusa, retorno ao menu e despedida.
O menu expõe somente a mercadoria implementada. Compra/venda de animais,
medicamentos e demais operações não são mostradas como se estivessem prontas.

Na quantidade, direita/esquerda alteram uma unidade; cima/baixo, dez. O valor
circula entre 1 e 99 conforme `shop_quantity_input`. Há repetição ao segurar as
setas. Z/Cross confirma; X/Circle cancela; V/Triângulo mostra a descrição e
retorna ao ponto anterior. O cancelamento não debita dinheiro.

Os preços vêm de `farm_tables.c`: 20G por forragem e 10G por ração. O pedido
credita o estoque compartilhado do depósito, desconta a carteira e soma a
despesa no livro de contas. Saldo insuficiente ou total de estoque acima de
999 mostram a recusa original e permitem corrigir a quantidade. A validação
ocorre antes da alteração; a fase de fechamento bloqueia confirmação repetida.
A mercadoria não ocupa a mão nem uma vaga da mochila.

O mundo, o jogador e o relógio ficam pausados durante o atendimento. O vendedor
vira para o personagem e recupera sua direção ao terminar. Textos e retratos
vêm do banco 2; as saudações expandem os nomes cadastrados de jogador/fazenda,
com limites de tamanho verificados. Os painéis, glifos e ícones usam a interface
original já adaptada para o host. Os rótulos estáticos são extraídos de
`text_bank.c` durante o configure; a tradução desses rótulos acompanha a
limitação atual dos outros menus nativos.

## Testar sem esperar o expediente

```bash
# Rancho, terça-feira 3 da primavera, meio-dia
./build/native-debug/port/hmbtn_native --data data --area-preview 136 \
  --preview-date 0:3 --preview-time 12:00
# Granja, na mesma data/hora
./build/native-debug/port/hmbtn_native --data data --area-preview 128 \
  --preview-date 0:3 --preview-time 12:00
```

`--preview-date ESTAÇÃO:DIA` aceita estação 0..3 e dia 1..30.
`--preview-time HH:MM` aceita hora 0..23 e minuto 0..59. Essas opções exigem
`--area-preview` ou `--farm-preview`. O dia da semana é derivado da data do
primeiro ano. A seção sazonal e a chegada original acompanham a data escolhida.
São sessões novas: 500G e estoque zero, sem alterar o início normal ou saves.

## Validação e limites

`native_feed_shop` cobre presença, mãos vazias, direção, horário/semana,
quantidade, descrição, cancelamento, preços, estoque 999, carteira/contas,
confirmação repetida e expansão limitada dos nomes.

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_feed_shop_playthrough data
```

O percurso opcional parte de uma sessão nova, posiciona o relógio de teste e
usa as passagens originais até as duas lojas. Compra duas unidades de cada
produto com os 500G iniciais, rejeita um pedido de 99 por saldo insuficiente,
volta à fazenda, retira os produtos dos depósitos e enche ambos os tipos de
cocho. Termina com 440G, despesa de 60G e uma unidade de cada produto no estoque.
Nenhum dinheiro ou produto é concedido pela fixture. O teste também verifica
que o atendimento conserva posição, pausa o relógio e bloqueia o menu Start.
Um terceiro argumento opcional seleciona a pasta de capturas BMP, fora do Git.

O recorte não executa os diálogos sociais completos desses moradores nem seus
ramos de presentes/eventos familiares. A loja de Lillia agora oferece também
a compra de galinhas adultas com nome, ligada ao galinheiro e à produção
de ovos: [POULTRY.md](POULTRY.md). Demais animais, reprodução, medicamentos
e outras operações comerciais continuam pendentes.

Validação nesta etapa somente no Linux local; código C11/SDL3 compartilhado
com Windows, sem novo cross-build. O bootstrap `--diagnostics` não executa o
jogo. Os testes host não substituem o build PS1 e sua validação SHA-1.
