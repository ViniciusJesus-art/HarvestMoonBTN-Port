# PF-06 — Ovos termais

Entrega de 2026-11-04. Ovos produzidos pela coleção atual de galinhas podem
ser arremessados na fonte termal e recebidos como ovo termal. Reutiliza voo,
respingo, apresentação, inventário, consumo e remessas. Runtime/PF-06 seguem
parciais; o bootstrap `--diagnostics` não executa o jogo.

## Referência C e BIN instalado

`people/player.c: thrown_item_settle` exige primeiro um piso de água segundo
`farm_status_kind`: bit 4 tem prioridade, depois `0x40`, depois 2. Só então
consulta a zona e transforma o pouso em `THROWN_ITEM_EGG_LANDED` se ela for
`ZONE_EGG_TARGET` (**38**) e `picture_cells.c:item_is_egg` aceitar o item:

| Ovos aceitos | Resultado |
|---|---|
| `0x122`, `0x123`, `0x124`, `0xe8`, `0x125` | `ITEM_SPA_BOILED_EGG`, **0x87 / 135**, uma unidade |

Ovo cozido comum `0x86`, ovo termal `0x87`, frutas e peixes não adquirem essa
conversão. Não existe teste de estação, pagamento, utensílio ou amizade.
`farm_zone_at_probe` considera a **primeira** caixa ativa (`0x4000`) que
contém o ponto: X/Z estritos, `minY-128 < Y < maxY+120`. Água sem zona 38,
ou caixa 38 em chão seco, não produz ovo termal.

No `00_gf.bin`, a seção da área 72 começa em **0x19e5000**; local **12**,
zona **5**, caixa `(-9732,-1812,6320)..(-8256,-1812,7100)`. As variantes
sazonais 73..75 têm a mesma caixa. O host lê os registros instalados,
limitados por tamanho; não duplica essas coordenadas na lógica do jogo.

No overlay `11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**, conferidos os words MIPS LE:

- `item_is_egg`, **0x800d7504**: subtração `0x122`, comparação com 3 e
  alternativas `0xe8`/`0x125` em **0x800d750c..0x800d7524**.
- `farm_zone_at_probe`, **0x800bde84**: bit ativo em **0x800bdea4** e
  folgas de altura `-128`/`+120` em **0x800bdf4c/0x800bdf64**.
- `farm_status_kind`, **0x800bf3f4**: máscaras 4, `0x40`, 2 em
  **0x800bf41c/0x800bf428/0x800bf430**.
- `thrown_item_settle`: ANI **23** e som **16** em
  **0x800cc52c/0x800cc534**, comparação com zona 38 em **0x800cc614**.
- `thrown_item_update`: resultado **0x87** em **0x800cc31c**.

`thrown_item_update` aguarda o fim da ANI do respingo antes de chamar
`player_show_item`. Esta rotina (**0x800cb540**) não exige mão vazia nem
espaço na mochila; `player_anim_advance` grava o resultado na mão ao terminar
sua apresentação, substituindo o item que eventualmente estiver nela.
O porte mantém essa particularidade somente na entrega de ovo termal;
outras apresentações conservam suas guardas existentes. O modificador do
novo registro é explicitamente zero, sem reproduzir dado local não inicializado.

`farm_tables.c: ItemInfo[ITEM_SPA_BOILED_EGG]` fornece preço **80 G**, figura
**18** e consumo **+4 stamina/-1 fadiga**. O catálogo nativo já continha esses
dados; não foi criada outra tabela de preços ou atributos.

## Integração e estado

`farm_carry` recebe a seção limitada da área junto ao terreno. No pouso em
água, ovos elegíveis consultam a primeira zona e usam o mesmo respingo ANI 23.
O slot guarda o ovo de origem até o término da animação. Um estado transitório
marca a entrega pronta; `exploration` começa a apresentação existente e limpa
o slot uma única vez. Se outra ação nativa estiver terminando, a entrega
aguarda sua conclusão, para não deixar uma ação interrompida no host.

`event_item` usa a ANI `11+direção` e só grava o ovo termal na mão no fim.
A mochila não é alterada. Mais de um voo usa os três slots existentes e as
apresentações são serializadas. Voo/respingo/entrega permanecem vinculados à
área em que começaram. `scene_preview` desenha o mesmo respingo para ovos,
oculta o slot pronto e usa a figura de item existente na apresentação.
Não há atualização de estado no desenho.

A trajetória continua sendo a adaptação nativa de arremesso já existente;
este recorte não reimplementa toda a física/colisão de `thrown_item_land`.
O reconhecimento usa o ponto de pouso resolvido por essa trajetória. Banho,
entrada/saída da fonte e ofertas à Deusa são recortes separados.

Saves já recusam slots de voo ocupados e apresentação ativa. O resultado
estável é um registro comum do inventário; codec **v22** inalterado. Não foi
criado estoque paralelo, estado de save novo ou política de compatibilidade.
`runtime.c`, `daily.c` e `save.c` ficaram idênticos ao início deste recorte;
portas e demais modificações do usuário foram preservadas.

## Validação

`spa_egg_test` cobre as cinco variantes, itens recusados, três bits de água,
chão seco, zona ausente/inativa/truncada, prioridade de sobreposição, bordas
estritas e altura. Verifica os quinze ticks nativos da ANI 23 antes da entrega,
mochila cheia/mão ocupada, substituição só após a apresentação, save recusado
no trânsito e persistência do ovo termal estável.

`spa_egg_playthrough` usa os recursos instalados e entradas de controle:

1. Compra e nomeia uma galinha com Lillia; compra seis rações.
2. Alimenta pelo depósito/cocho e dorme pela escolha original da cama durante
   quatro noites; coleta três ovos realmente produzidos, guardando dois na mochila.
3. Retoma o estado com os três ovos, viaja à fonte pela saída real e arremessa
   o terceiro. Confere respingo/apresentação/recebimento com a mochila cheia.
4. Retoma o ovo termal e come: **+4/-1**, sem segundo resultado nos ticks seguintes.
5. Converte outro ovo, remete por **80 G** e arremessa o terceiro em chão seco:
   pisca/some pelo descarte comum, sem ovo termal.
6. Dorme até receber os 80 G e retoma o estado final. Os três checkpoints
   também são carregados pelo aplicativo em processos novos.

Dinheiro/data/posições e atributos anteriores ao consumo são fixtures;
galinha, ração, ovos e resultados não são concedidos pelo teste. A busca de
posição de lançamento usa o terreno instalado e o voo atual; não altera zonas
nem polígonos. Capturas locais de respingo/recebimento foram inspecionadas,
sem incluir assets proprietários ou imagens geradas no repositório.

Linux configure/build Debug e Release aprovados: Debug **12/12**, Release
**3/3**, com percurso e três retomadas por configuração. Regressões incluem
voo, remessas, peixes/lago, apresentações, codec, exploração e portas/chuva.
Build Windows MinGW sem rede aprovado; unidade de ovos, percurso e três
retomadas no Wine aprovados, assim como regressões de carregar/arremessar e
apresentar/consumir itens. Após a revisão final da espera por ações da cesta
ou saída pendente, o percurso passou novamente em Debug, Release e Wine.

**48 processos de teste**: **23 Debug**, **9 Release** e **16 Wine/targets**,
incluindo CTest, wrappers, filhos, lançadores e três tentativas iniciais do
driver. Não foi executada a suíte inteira; teto de 50 respeitado.

Os ensaios iniciais do driver corrigiram a data da fixture e evitaram o dia
do festival em que a tabela original suspende o pagamento de remessas; a
regra do jogo foi preservada. `src/`, `sdk/`, `disc/`, `tools/` e CMake PS1
não foram modificados. Build host não substitui a toolchain/SHA-1 do PS1;
Wine não substitui teste em hardware Windows físico.

Próximo recorte recomendado: banho na fonte termal, auditando as zonas e
roteiros originais 46/47, entrada/saída, recuperação, relógio e retomada.
