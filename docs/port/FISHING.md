# Pesca nativa e resultados de captura

O porte agora executa o ciclo de lançar a boia, aguardar, fisgar, recolher e
receber o resultado. `port/game/fishing.*` adapta `field/fishing.c` e os ramos
de pesca/recebimento de `people/player.c`. Não há instruções MIPS em execução.
Os sprites e sons continuam vindo dos BINs instalados.

## Controles e sequência

Receba a vara de Greg conforme [GREG_ROD.md](GREG_ROD.md). Com a vara equipada,
mão de produtos vazia e diante da água, pressione e mantenha **C/Quadrado**.
O jogador lança a boia e assume a pose de espera. Quando a pose e a boia mudarem
na fisgada, solte C/Quadrado para recolher. Soltar durante a espera recolhe sem
capturar; segurar além da janela da fisgada retorna à espera. Movimento, menus,
troca de ferramenta e conversas ficam suspensos durante a ação; o relógio e
os moradores continuam avançando nos locais que permitem isso.

```bash
./build/native-debug/port/hmbtn_native --data data --area-preview 56 \
  --preview-date 0:6 --preview-time 07:00
```

Esse atalho começa uma sessão nova na praia, sem conceder a vara. Fale com
Greg usando Z/Cross e depois aproxime-se da água. A seleção de ferramentas
usa E/R1, conforme os controles já existentes. Um peixe recebido pode ser
carregado, guardado na mochila, entregue como presente ou levado à caixa de
vendas. A venda usa o preço original e o pagamento às 17h58.

## Regras e estado

As quatro faixas de horário, cinco tipos de local, tabelas por estação/local
e resultados de compensação são gerados diretamente das tabelas C originais
durante o configure. Nenhum peso de captura foi inventado. A consulta do tipo
usa o local original do cabeçalho da cena: 44, 13, 12 e 66 selecionam os quatro
tipos específicos; os demais usam o tipo padrão. Isso não disponibiliza áreas
que o mapa nativo ainda não integra.

O gerador é a adaptação existente de `rand_range`, com estado e overflow de
32 bits. A vara comum usa a comparação original de 20 em 100 por tentativa;
a vara melhorada usa 40 em 100. A janela da fisgada é sorteada junto da espera.
O estado aleatório da pesca ainda não reproduz a sequência global compartilhada
com título/áudio/animais no PS1. A obtenção da vara melhorada depende do sistema
do lago/peixes, que continua pendente; suportar seu id não concede o item.

A boia usa deslocamento cardinal de 29 unidades por tick, velocidade vertical
inicial −78, gravidade 9 e limite de queda 45, conforme o lançamento original.
O chão vem do modelo decodificado, com a sequência original de tolerâncias.
Obstáculo/ausência de superfície reverte a direção, recua 88 unidades e zera
o deslocamento horizontal. O lançamento espera o fim da animação e da viagem
da boia antes de consultar a pesca. A água válida tem a flag `0x0002` e não tem
as flags `0x0004`/`0x0040`. Solo seco, lago de criação e superfícies inválidas
terminam sem captura.

Cada lançamento concluído gasta uma unidade de energia e aplica o custo de
fadiga do recorte atual, incluindo horário/clima. Os peixes comuns zeram a
sequência de resultados ruins. Os demais resultados incrementam essa sequência
até o limite 11; o resultado seguinte usa a compensação do tipo de local.
Entrar em uma cena zera a sequência e a flag de isca, como `farm_frame_update`.

## Capturas especiais

O sorteio executa as condições e flags originais:

| Resultado | Condição além do peso da tabela |
|---|---|
| Receita em garrafa | Ainda não pescada |
| Power Berry | Ainda não pescada |
| Carpa | As outras cinco capturas especiais registradas |
| Lula | Peixe pequeno lançado na água da praia durante a visita atual |
| Peixe-pescador | Horário fora do intervalo literal 08h00..22h00 |
| Dourado do mar | Meta de 200 peixes vendidos registrada |
| Bagre | Ainda não capturado |
| Char | Flags originais de sashimi, peixe grelhado e sushi |

A condição de horário mantém o limite do código: minuto 1320 (22h00) ainda
é bloqueado e minuto 1321 (22h01) é permitido. Uma entrada especial já tomada
ou sem pré-requisitos continua pelo restante da tabela acumulada; não ocorre
outro sorteio no seu lugar. As seis flags de captura e bits correspondentes
de registro permanecem na sessão. Cozinhar e o conjunto completo de áreas
ainda não estão portados, portanto nem todos os pré-requisitos podem ser
obtidos por gameplay neste estágio.

Peixe pequeno arremessado na água da praia ativa a flag de isca. A contagem de
peixes vendidos ativa a flag de 200. Os testes distinguem o comportamento
real dessas integrações dos cenários com flags preparadas artificialmente.

Peixes comuns, lixo e a garrafa entram pela animação de recebimento original e
ficam na mão de produtos. A Power Berry executa a pose de consumo, aumenta a
energia máxima em dez (limite 255), repõe a energia, reduz a fadiga em um e
incrementa o contador de berries. Dormir passa a respeitar o máximo adquirido.

Um peixe especial é mostrado, lançado de volta e substituído por sua estampa
original. A estampa é apresentada e guardada sem ocupar a mochila, mantendo
as flags de captura. A galeria e os eventos finais que usam esses registros
continuam pendentes. Às 06h, o retorno à casa interrompe a ação/boia e conserva
as aquisições permanentes da sessão. Não há saves entre execuções.

## Recursos de ação por identidade

`filesystem/action_resources.*` resolve o índice original de
`farm_load_sector_row`, junto do limite ANI de `player_anim_banks`. A pesca
pede a linha 10. A mesma interface agora atende confirmação com a cabeça (12),
funeral (4), sono (15), sobrecarga (3), medicina (22) e reabastecimento (23).

A base física vem de `farm_load.c`; os 28 limites ANI vêm de `player.c`.
Linha 27, cujo limite é zero, ids fora da tabela e destinos com tamanho errado
são recusados. A leitura usa `hmbtn_resource_read` e a apresentação usa os
decodificadores ANI/banco/atlas atuais. A imagem do resultado conserva o id
original do item, e a boia usa os ícones 27/28 já carregados da interface.
Não foi introduzido outro decodificador nem uma cópia de offsets nos módulos
de gameplay. Veja [RESOURCE_IDENTITIES.md](RESOURCE_IDENTITIES.md) para o futuro
uso dos PNGs extraídos com seus metadados.

## Validação e limites de paridade

- `native_fishing`: faixas/limites de horário, bandas de captura com sementes
  conhecidas, condições e repetição de especiais, compensação, lançamento,
  solo/água inválida, cancelamento, fisgada perdida, recebimento, consumo da
  berry, isca e ciclo de soltura/estampa.
- `native_action_resources`: ids estáveis, limites ANI e recusa de solicitações
  inválidas, incluindo a linha final sem recurso.
- `native_fishing_playthrough`: Greg entrega a vara pelo script real; o jogador
  lança sobre a água real da praia, cancela, fisga, recebe e vende um peixe com
  pagamento. O teste confere a suspensão de movimento/menus, carregamento das
  sete linhas de ação em uso, imagens e sons. Sementes de capturas especiais
  são fixtures para executar as ANIs de berry/estampa. Também confere meta de
  vendas, resets na troca de cena, interrupção às 06h e energia máxima após sono.

```bash
mkdir -p /tmp/hmbtn-fishing-check
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_fishing_playthrough data \
  /tmp/hmbtn-fishing-check
```

Capturas locais da espera, fisgada, resultado e estampa foram inspecionadas em
SDL dummy/software. A animação de respingo/espera do efeito de pouso ainda não
foi portada: o contato válido entrega a boia diretamente à espera. Colisões
com alvos dinâmicos de eventos, vibração da pesca, ordenação completa da boia
com a geometria e comparação exata de tempos/GPU/SPU com o PS1 permanecem
pendentes. O balanço usa cosseno host no mesmo período, sem prova de equivalência
pixel a pixel com a tabela PS1.

Linux/Windows continuam sendo o foco, mas esta validação foi em Linux. Testes
host não substituem Windows real nem o build original PS1/SHA-1. Recursos e
capturas ficam fora do Git. O porte é parcial; `--diagnostics` só executa o bootstrap.
