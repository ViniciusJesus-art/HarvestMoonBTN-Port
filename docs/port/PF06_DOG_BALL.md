# PF-06 — Cachorro adulto e bola

Recorte de 2026-11-05, Linux/Windows. Complementa [o filhote](PF06_DOG.md)
sem outro registro do animal. O runtime/PF-06 continuam parciais; bootstrap
de diagnóstico sem jogo. Fontes, SDK, disco e fluxo CMake PS1 preservados.

## Auditoria C/BIN

`animals/dog.c:dog_tick` promove `ITEM_YOUNG_DOG` (0x107) a
`ITEM_ADULT_DOG` (0x106) no outono do ano interno zero. Não é uma idade
arbitrária de noventa dias. O afeto é preservado; a marca de adulto muda
o carregamento do sprite. `dog_sprite_load` usa `farm_load_span(0)`,
`00_gf.bin:0x37f5800..0x3805800`, com banco em **+0x23c8**. A ANI possui
67 ranges, incluindo as poses com bola. A forma jovem mantém seu span 1.

Na tabela instalada da fazenda, a linha **67** seleciona o evento **286**:
id/lugar **0x091e**, modo **2**, variável 40/contador **16** entre 3 e 127,
condição **81** (`ADULT_DOG_GOLD_100`), flags **0x7ff98000** e janela livre.
Isso exige chegada da casa (lugar 2), cachorro adulto, pelo menos **100 G**,
mão vazia, tempo ensolarado e `ESTATE_FLAG_TRIGGER_ARMED` livre. O evento
é repetível: o bit de conclusão não substitui a marca da bola.

`12_event_s.bin:0x6b5000..0x6b7000` contém vinte instruções, incluindo o
terminador. Entradas: ANI da bola **0x0865**, ator de Won **0x2023** (id 35),
banco **0x8000** e roteiro **0x4000**. Falas **866/867/868**. O próprio
roteiro arma a marca, abre a escolha, debita 100 G e apresenta o item 101.
Uma particularidade foi conservada: **ambas as respostas cobram e entregam
a bola**, mudando a fala de Won. O débito é a instrução 7, anterior ao teste
da resposta na 8; os dois ramos executam `receive_item(101)`.

`player.c:thrown_item_spawn/land` dá à bola velocidade horizontal dupla,
impulso vertical −28, gravidade 9 e limite de queda 45. O marcador do item
substitui o descarte comum depois de um pouso seco. Água continua pelo
respingo original; não deve ser interceptada pela colisão alta do terreno.
`estate_trigger_disarm` limpa a marca/bola e zera `days.count0[16]`, permitindo
uma reposição paga depois de três manhãs.

`dog_accept_request` aceita apenas adulto, mesmo lugar, 06h..20h e alvo
estritamente a menos de 2000 em X/Z. Marca `TRAINED_TODAY` na aceitação,
não a cada conclusão. `dog_script_approach_player` busca o marcador, reclama
a bola, retorna ao jogador e a deposita perto dele. As poses **18/19/20/21**
são as variantes com bola; a aproximação adulta usa velocidade **36**.
Espera/busca/retorno usam limites de 300 ticks e a chegada espera 90.

A atualização diária soma **1/2/3** de treino conforme o afeto esteja
abaixo de 10/20 ou a partir de 20, limitado a 255. Usa o afeto depois dos
cuidados do dia e antes da penalidade do clima. Treinar evita a perda por
negligência, sem adicionar por si só afeto. Repetir arremessos não multiplica
o ganho diário.

## Integração nativa

`session.dog` continua como único dono. `adult`, `training`, `trained_today`
e os cursores de busca estendem o registro existente. `dog.ball` guarda a
marca armada e o stamp de chão: a bola está no inventário, no ar, no stamp
ou na boca, sem criar uma segunda bola para desenhar o retorno. Pegar a
bola do chão/boca usa a ação normal de pickup; a mão do cachorro adulto
referencia o mesmo animal e mantém o id correto na promoção.

`farm_carry` empresta o dono da bola e reaproveita seus slots de voo/gravidade/
respingo. A exploração consome os pedidos de pouso/perda, inicia a busca e
emite os sons reais. A bola pode ser arremessada nos interiores, e a porta
tem precedência sobre esse arremesso. Sair com uma bola deixada em lugar
externo aos lugares 1..6 segue a desativação do stamp no carregamento.

O evento de Won usa o interpretador/serviços de mensagem e economia existentes;
apenas os quatro comandos ainda ausentes deste roteiro são adaptados no
despacho de visita. A ANI de staging é usada durante a entrega. O array e os
laços de atores de evento foram alinhados a `HMBTN_VILLAGERS` (37), com guarda
de índice: o antigo limite 35 excluía exatamente o ator desta visita.

`dog_art` prepara ambos os packs originais para a promoção não depender de
I/O no meio da manhã. O desenho escolhe a forma pelo estado autoritativo,
sem atualizar regras. O benefício de treino e a promoção passam pelo mesmo
coordenador diário, uma vez por data. Codec **v26** acrescenta somente os
campos necessários e a marca/cursores; vistas emprestadas não são salvas.
Não houve trabalho novo de migração ou política de compatibilidade.

## Aceite e limites

`dog_ball_test` cobre a fronteira de estação e id na mão, horário/lugar/raio,
faixas/limites/ganho diário de treino, condições da visita e retomada com
bola na boca. `dog_ball_playthrough` começa com o filhote na véspera do
outono, dorme pelo fluxo existente, espera o contador real, recebe Won e
percorre as duas respostas a partir de um checkpoint anterior à compra.

O percurso guarda a bola pelo inventário, carrega o adulto pelas portas,
arremessa e recebe duas devoluções, abre/fecha o status durante a busca,
salva/reabre no retorno, recusa busca noturna, perde a bola com um arremesso
real na água, espera três manhãs e paga a reposição. O treino sobe uma vez.
Data e posições do jogador são fixtures; não injeta adulto, bola, afeto ou
treino. O wrapper abre os três checkpoints em processos novos do aplicativo:
adulto antes da compra, bola no retorno e reposição/treino concluídos.

Capturas da entrega e da bola na boca foram inspecionadas. Não foram
adicionados recursos proprietários ou capturas ao Git. O bloco das animações/
transições de portas do usuário permanece idêntico ao início da rodada.

Concursos, ataques/defesa, condução de rebanhos, saltos autônomos de cercas e
rotas autônomas entre mapas continuam pendentes. O walker e o RNG persistente
do animal permanecem as adaptações nativas descritas no recorte do filhote;
isto não declara paridade integral da IA PS1. Próximo recorte: cavalo inicial
e aquisição pelo evento original de Barley.

## Validação da rodada

Linux: configure/build completo Debug aprovados; seleção **12/12**, 12,24 s,
com adulto/bola, filhote, saves, sessão, inventário, arremesso, portas/chuva,
águas, Deusa e transporte de galinhas. Configure/build Release dos alvos
pertinentes aprovados; seleção **3/3**, 1,96 s, com unidade/percurso da bola,
três retomadas pelo aplicativo e águas.

Windows x64: build MinGW Release dos executáveis pertinentes concluído no
contêiner local. Unidade, percurso completo e três retomadas em processos
novos do aplicativo (três frames cada) passaram no Wine. Capturas reais de
Won/entrega/adulto/bola na boca inspecionadas. Nenhum recurso proprietário
foi incorporado ao repositório.

**46 processos de teste** no total: cinco execuções dirigidas iniciais,
23 na seleção Debug incluindo CTest/wrappers/filhos, oito em Release e dez
nas verificações Wine contando lançadores/executáveis. As tentativas iniciais
revelaram o antigo limite de 35 atores e a precedência de colisão sobre água,
corrigidos antes das seleções finais. Limite de 50 respeitado; suíte inteira,
Windows físico/MSVC e avaliação de áudio/janela em hardware não executados.
Renderização por software e áudio dummy nas seleções Linux.

Reprodução das seleções, após os respectivos builds:

```sh
ctest --preset native-debug -R '^(native_dog_ball|native_dog_ball_playthrough|native_dog|native_dog_playthrough|native_save|native_inventory_rules|native_farm_carry|native_weather_doors_playthrough|native_water_playthrough|native_goddess_playthrough|native_poultry_transport_playthrough|native_session_state)$'
ctest --preset native-release -R '^(native_dog_ball|native_dog_ball_playthrough|native_water_playthrough)$'
```

`git diff --check` aprovado. A comparação do bloco de portas com o início
da rodada permanece idêntica. Os testes host não substituem toolchain e
validação SHA-1 PS1; o bootstrap continua sem executar o jogo.
