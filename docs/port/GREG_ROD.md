# Entrega da vara por Greg

A conversa de Greg executa o script 382 de `12_event_s.bin`, usando o leitor de
blocos e o interpretador já existentes. As condições, saltos, respostas e falas
continuam sendo os bytes da edição instalada. O porte não cria uma oferta nem
substitui mensagens quando a mochila está cheia.

## Fluxo original

O script verifica a flag `0xE7` e espaço nas ferramentas carregadas: mão equipada
e `2 << rucksack_level` posições da mochila. Espaço apenas na caixa da casa não
permite a oferta. Quando há espaço e a oferta ainda não foi registrada, o script
ativa a flag e abre a mensagem 1718 do banco 0. A resposta 1 segue pela mensagem
1719 e pelo comando 32, `event_item_store(56, 0, 1, 1)`.

Aceitar guarda a vara de id `0x38` na mão de ferramenta, quando livre, ou no
primeiro espaço vazio da mochila. O item recebe modificador 0 e quantidade 1.
O restante dos itens é conservado. A vara pode ser selecionada com o ciclo de
ferramentas já existente. A flag da oferta e o inventário persistem entre áreas
durante a sessão.

A flag é ativada **antes da resposta**, como no script. Recusar não concede a
vara e mantém essa flag; uma conversa posterior segue os outros ramos originais.
Mochila cheia segue o diálogo comum sem ativar a flag da oferta. Nesse caso,
liberar um espaço permite recebê-la numa conversa posterior. Não há recusa
inventada nem repetição automática da oferta após escolher não.

## Armazenamento e apresentação

O interpretador agora adapta o comando 32 para os ids de itens suportados pelas
imagens atuais, 1..379. Ele segue a ordem mão → mochila → caixa. O argumento
`quiet` restringe a busca a mão/mochila quando não é zero; sem essa restrição,
um item pode ir ao primeiro espaço da caixa. Se não há espaço, retorna pelo
ramo de falha do bytecode sem alterar os itens. O modo de tabela lê a metade
inferior assinada de 16 bits da variável, como `script_vars[item].half`.
Ferramentas 1..25 recebem o modificador `(id - 1) % 5`; os demais recebem zero.

Quando `notify` pede apresentação e o item não foi para a caixa, o mundo usa a
ANI base `0x47`, a imagem do id original e o efeito 7. A pose usa o frame da
direção do jogador; a imagem usa o deslocamento original do modo de mostrar
ferramenta. O contador de estado original vai de 10 (ou 60) até −1. A execução
nativa conserva a pose por 11 (ou 61) ticks de atualização e então volta ao
controle livre. Movimento e abertura de menus ficam suspensos durante a pose;
depois do fim do script, o relógio externo pode continuar avançando.

O sistema de apresentação breve de ferramentas, o cache de imagens de itens,
a ANI base, o leitor BIN e os decodificadores existentes são reutilizados.
Nenhum novo offset de recurso nem carregador gráfico foi introduzido.
As identidades continuam sendo a animação original e o id do item, conforme
[RESOURCE_IDENTITIES.md](RESOURCE_IDENTITIES.md), permitindo a futura resolução
para PNGs extraídos com os metadados necessários.

Se um comando ainda não portado interrompe o script, o interpretador também
restaura mão, mochila e caixa junto das flags/afinidade. A solicitação de
apresentação é cancelada, sem registrar uma entrega como concluída.

## Reprodução e validação

Na sessão de prévia, Greg está na praia conforme sua agenda original. Para
testar na sexta-feira 6 da primavera às 07h:

```bash
./build/native-debug/port/hmbtn_native --data data --area-preview 56 \
  --preview-date 0:6 --preview-time 07:00
```

Vá até Greg e aperte Z/Cross com as mãos de produtos vazias. Selecione a resposta
com cima/baixo e confirme com Z. Essa prévia começa uma sessão nova e não concede
a vara antes da conversa. O uso da vara está em [FISHING.md](FISHING.md).

`native_event_tool_store` usa scripts sintéticos para verificar ordem de
armazenamento, capacidades 2/4/8, mochila cheia mesmo com espaço na caixa,
transbordo para a caixa, modificadores, notificação, variáveis de 16 bits,
limites e restauração ao chegar a uma ação ausente.

`native_greg_rod_playthrough` usa agenda, cenário, script, textos, retratos,
imagem da vara e ANI originais. Verifica mão livre, mochila, falta de espaço,
recusa, liberação de espaço, seleção da vara pelo ciclo existente, ausência de
duplicação e conservação após recarregar áreas. Compara os glifos/controles de
cada mensagem com o BIN, desenha a entrega e testa a pausa de movimento/menus.
As posições e distribuições das ferramentas iniciais são fixtures do teste;
nenhuma vara é concedida por essas fixtures.

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_greg_rod_playthrough data \
  /tmp/hmbtn-greg-presentation.bmp
```

Captura local conferida em SDL dummy/software. Presentes e aniversários estão
em [VILLAGER_GIFTS.md](VILLAGER_GIFTS.md), e a pesca em [FISHING.md](FISHING.md).
Demais lojas, eventos familiares
e saves continuam pendentes. Não há persistência entre
execuções. Testes Linux não substituem Windows real nem o build PS1/SHA-1.
O porte é parcial; `--diagnostics` executa somente o bootstrap.
