# Caixa de venda e contas

O porte adapta `estate_shipment_add`, `estate_item_slot_index`,
`estate_shipment_settle` e o lançamento de `people/player.c`. Preços vêm de
`item_info`; os grupos de contagem e as datas sem coleta são extraídos do C
original durante o configure. Os arquivos PS1 e os recursos importados não
são modificados.

## Entrega

Na fazenda, Z/Cross com um item na mão executa a ANI de lançamento. O item
sai da mão somente ao concluir essa animação. Durante o voo, a consulta ao
terreno usa as cinco tolerâncias de `player_settle_ground_tolerance` e
atualiza a altura de aterrissagem. A superfície original da caixa tem o bit
`0x80` no primeiro byte da primitiva, consultado por `farm_status_upper_bits`.
Nenhum retângulo de caixa foi inventado no código host.

Ao aterrissar nessa superfície, um item com preço positivo acrescenta seu
valor pendente, incrementa a contagem e toca o efeito original 19. O item é
retirado do voo uma única vez. Jogá-lo em chão comum não registra venda.
Mato, pedra e galho sem preço não geram dinheiro na caixa. Ferramentas e
sementes continuam protegidas contra esse caminho de descarte.

Existem 25 grupos: 15 cultivos, ovos, leite, lã, maionese, queijo, novelos,
peixes e três ervas. Cada grupo limita a contagem a 999. Produtos vendáveis
sem grupo, como minério e mel, acrescentam valor sem criar um contador extra.
O total de peixes enviados é separado do contador de peixes pescados.
O bônus de 10G do mel é suportado pelo estado da remessa; seu evento de
desbloqueio ainda depende dos sistemas de fazenda não adaptados.

## Pagamento e calendário

O dinheiro permanece pendente até **17h58**, conforme o evento 5 do relógio.
A coleta ocorre uma vez por dia. O saldo e as receitas do menu de estado
usam o mesmo objeto de estatísticas; não existe uma segunda carteira.
A carteira limita-se a 9.999.999G, enquanto a receita contabiliza o valor
integral, seguindo `estate_gold_change`.

As primeiras 14 datas de `clock_date_table`, usadas por `clock_date_index`,
adiam o pagamento. O valor permanece acumulado para a próxima coleta.
Itens enviados depois da coleta também aguardam o próximo dia elegível.
O bloqueio diário é liberado às 06h00. Dormir e o avanço do passeio processam
cada minuto em ordem, preservando o horário de pagamento e as datas.
O histórico das contas é deslocado às 05h59 do último dia da estação,
como o callback original, antes da mudança da data às 06h00.

## Limites e validação

Cultivos, criação de animais, pesca e geração diária de produtos ainda
precisam ser adaptados. O campo inicial oferece os objetos soltos já
suportados; esta entrega não adiciona produtos gratuitos ao inventário.
Ainda faltam cesta, cavalo, caixas de interiores e rotina de Zack. A coleta
monetária já funciona independentemente de um ator de Zack em exploração livre.
Reações a água, obstáculos e efeitos completos de aterrissagem seguem pendentes.
Saldo, remessas e contagens persistem na sessão, sem gravação de saves.
O bootstrap `--diagnostics` continua sem executar o jogo, e testes host não
substituem a validação SHA-1 do build PS1.

Os 26 CTests sem recursos cobrem superfícies, preços, grupos, limites,
pagamento único, datas especiais, avanço durante sono e virada de contas.
O driver opcional injeta um nabo **somente no teste**, executa a animação
real e verifica a caixa, pagamento, menu e sono com os recursos do usuário:

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_shipping_playthrough data
```

As quatro seções sazonais são verificadas. Um terceiro argumento seleciona
uma pasta existente para capturar o menu em BMP. Configure
`HMBTN_TEST_GAME_DATA` com um caminho absoluto para registrar esse percurso,
o passeio inicial e o percurso do relógio como CTests locais opcionais.
