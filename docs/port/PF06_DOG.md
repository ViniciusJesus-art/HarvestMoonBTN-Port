# PF-06 — Cachorro inicial

Recorte de 2026-11-05 para Linux/Windows. O runtime/PF-06 permanecem parciais;
o bootstrap de diagnóstico não executa o jogo. Fontes, SDK, disco e fluxo
CMake PS1 preservados; builds host não substituem toolchain/SHA-1 PS1.

## Referência original

- `animals.c: farm_reset_tables` cria `ITEM_YOUNG_DOG` (0x107), lugar 1,
  posição **−1800, 0, 3100**. `dog_request_start` começa com **10 de afeto**
  e conserva o nome já cadastrado.
- `dog.c: dog_pick_idle_phase`, cues e `cue_anim_table` controlam os descansos,
  poses e caminhadas. A escolha diurna do filhote usa oito resultados;
  entre 21h e 06h usa a sequência noturna. Caminhada ociosa: **12 unidades**;
  aproximação ao jogador: **24**. `farm_walker_move/pick_heading/pick_turn`
  usam sondas frontal/laterais, limite 720/1080 dividido pela velocidade e
  pesos originais, inclusive o viés para o destino.
  O passeio livre usa `ACTOR_ACTION_MOVING` (1); a aproximação usa a linha
  própria de `dog_action_table` (4). As duas sequências de ANI são distintas.
- `player_collect_target`, `farm_action_begin`, `farm_place_held` e
  `player_latch_place`: a mão referencia o mesmo registro; pegar usa a pose
  37+direção, soltar usa 23+direção. A posição é colocada 300 unidades à
  frente. O lugar do cachorro acompanha o jogador enquanto ele o carrega.
  O probe do cachorro é **0x2100**, sem o bit 0x4000 de parada sólida do
  jogador, assim como as galinhas não são uma parede para o personagem.
- Soltar chama a fase 9 e `dog_set_bit2_cue`: marca cuidado do dia e mostra
  cue 37 por 45 quadros somente na primeira vez. `dog_tick` aplica **+2**
  por esse cuidado e **+2** por uma resposta ao assobio; sem ambos perde 1.
  Mau tempo em um dos lugares externos enumerados no original tira mais
  **5**. Limites 0..255, limpeza das marcas após a atualização diária.
- `player.c` inicia o assobio com **L1**, pose **67+direção**, som **18**.
  `farm_mode_check_poll` espera a ação terminar. `dog_try_action` responde
  quando `rand_range(200) < affection+50`, toca som **25**, marca o dia e
  começa a aproximação. O contador de acompanhamento dura até 300 quadros.

`dog_sprite_load` lê `farm_load_span(1)` em
`00_gf.bin:0x3805800..0x380b000`, ANI de **48 ranges** até +0x1420,
seguida pelo banco original. Os frames/UVs/paletas são decodificados dos
recursos instalados. A imagem carregada na mão usa o picture loader existente
do item 0x107. Nenhum recurso proprietário ou captura foi adicionado ao Git.

## Adaptação e propriedade

`HmbtnSession.dog` é o único dono da posição, ANI, afeto, marcas de cuidado,
cursores das poses e RNG. O nome continua em `game.registration.dog_name`.
`menu.dog` e `village.dog` são empréstimos somente de leitura; substituem o
afeto e a condição de mão antes desconectados do animal. Os ponteiros são
ligados ao construir a sessão, inclusive após decodificar um save.

`game/dog.c` adapta as fases/poses e o walker ao terreno, campo e sondas
nativas existentes. `dog_turns.inc` transcreve a tabela C de pesos, sem
endereços PS1. O gerador original `hmbtn_psx_random_range` usa um fluxo
persistente do animal; a ordem do RNG global de todos os sistemas PS1 ainda
não é reproduzida. Movimento/ANI/cues avançam no tick de exploração,
independentemente do desenho, e param durante menus/transições.

A mão guarda apenas o id reservado. As ações impedem guardar o animal na
mochila ou trocá-lo por uma ferramenta. Água, remessa, terreno ausente,
ocupantes e células obstruídas recusam a soltura sem perder o cachorro.
O transporte pelas portas existentes conserva o registro e atualiza sua
área; o início do sono o coloca no chão antes da atualização diária.
Os sons usam banco/programa/tom/nota reais de `hmbtn_farm_sound`, incluindo
o assobio e a resposta, em vez do helper limitado aos tons de menu.

A atualização diária ocorre no coordenador existente, antes de trocar o
clima, protegida pela mesma chave de data. Apenas os campos necessários do
cachorro entram no codec **v25**, na seção de animais; não foi aberto trabalho
de compatibilidade/migração. Saves durante a ação ou com animal na mão seguem
a guarda de animais já existente. Nenhuma alteração nas curvas/transições
das portas do usuário.

## Percurso e limites

A unidade cobre as quatro direções, posse única, recusas de soltura, inventário,
afeto/clima/lugares internos, limites, L1, save e aplicação matinal única.
O percurso começa com o filhote criado pela sessão, sem injetar um animal.
Espera a caminhada sorteada, pega o cachorro, recusa água, atravessa a porta
da casa, solta e abre o status. Confere pausa do registro inteiro, incluindo
RNG, e retomada interna. Dorme segurando o filhote, confere cuidado interno,
retorna à fazenda, repete cuidado/assobio, verifica pose noturna e dorme com
chuva para conferir a perda externa. Retoma o checkpoint externo. Data,
nome e posições do jogador são fixtures; o sono usa o fluxo real com o
modo de cama selecionado pelo driver.

O primeiro ensaio de movimento parava após 900 ticks sem um sorteio de
caminhada; passou após esperar até 3600 ticks, sem forçar fase/seed/posição
do animal. Capturas do filhote inicial, na mão, dentro da casa e no status
foram inspecionadas.

Crescimento/adulto, bola/busca/treino, concursos, ataques e rotas autônomas
entre mapas continuam fora deste recorte. As fases do animal fora da área
atual ficam preservadas, como nos consumidores nativos de animais atuais.
O próximo recorte deve integrar crescimento e bola com suas dependências
reais, sem criar um adulto ou uma bola gratuita para saltar a aquisição.

## Validação da rodada

Linux: configure e build completo Debug aprovados. Seleção **14/14** em
48,09 s: cachorro/unidade/percurso, save, exploração, regras/estado de
inventário, menu, galinha/transporte, portas/chuva, águas, fonte, Deusa e
Kappa. Configure/build Release dos alvos pertinentes aprovados; seleção
**3/3** em 2,81 s (cachorro, seu percurso/duas retomadas e águas).

Windows x64: build MinGW Release dos executáveis pertinentes concluído no
contêiner local. Unidade, percurso e duas retomadas em processos novos do
aplicativo (casa/fazenda, três frames cada) passaram no Wine.

Após a revisão final separar a pose de passeio da pose de aproximação,
os executáveis afetados foram recompilados nas três configurações e o
percurso completo foi repetido com sucesso em **Linux Release e Wine**.
Ele agora exige explicitamente a ANI 1+direção durante o passeio e também
observa os sons reais de assobio/resposta. As seleções anteriores não foram
repetidas integralmente após esse ajuste de pose.

**50 processos de teste** no total: quatro execuções dirigidas iniciais
(duas interrompidas pelo limite de espera do sorteio de movimento), 28 na
seleção Debug contando CTest/wrappers/filhos, sete em Release e onze nas
execuções finais/Wine contando lançadores/executáveis. Limite da rodada
respeitado, sem suíte inteira. `git diff --check` aprovado e comparação do
bloco de portas do usuário idêntica ao começo da rodada.

Reprodução das seleções, após os builds correspondentes:

```sh
ctest --preset native-debug -R '^(native_dog|native_dog_playthrough|native_save|native_inventory_rules|native_inventory_state|native_farm_menu|native_poultry_carry|native_poultry_transport_playthrough|native_weather_doors_playthrough|native_water_playthrough|native_hot_spring_playthrough|native_kappa_playthrough|native_exploration|native_goddess_playthrough)$'
ctest --preset native-release -R '^(native_dog|native_dog_playthrough|native_water_playthrough)$'
```

As seleções Linux usam renderização por software e áudio dummy. Capturas
foram inspecionadas; não houve avaliação de janela/áudio em hardware real,
Windows físico ou MSVC. Os testes host não substituem validação PS1.
