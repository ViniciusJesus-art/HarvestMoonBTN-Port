# Transporte das galinhas e ciclo no exterior

Adultas e pintinhos nomeados podem ser pegos com Z/Cross, carregados pelas
portas e colocados no chão. O registro existente conserva nome, afeição,
idade, doença, alimentação e crescimento. Não é criado outro animal ao
trocar de área. As sessões de prévia continuam sem animais concedidos.

## Estado, controles e apresentação

`game/poultry_carry.c` adapta `player_collect_target`, `player_show_item`,
`player_use_held_item`, `player_drop_held_item` e `farm_place_held` das fontes
originais. O registro tem uma área lógica host e um marcador de transporte.
A mão recebe o item original `0x109` ou `0x10A`, mantendo a vaga reservada.
Enquanto carregado, o animal deixa de desenhar e caminhar no chão. A imagem
na mão usa o recurso original de `picture_cell_build_prim` já decodificado
pelo host. Estados e endereços serializados do PS1 não viram ponteiros host.

Com as mãos vazias, Cross sonda 128 unidades à frente contra os limites
exclusivos de 200 em X/Z do animal, na mesma área. Ovos são consultados antes
do animal, conforme a prioridade de recursos do cenário. Incubação, nome
pendente e mortos não podem ser pegos. Uma galinha doente pode ser carregada.

O jogador usa as ANIs originais: pegar 37+direção, parado 41, caminhar
42+direção, correr 19+direção e colocar 23+direção. A apresentação acompanha
as direções e os offsets de `mark_screen_offsets`. Não se inicia um salto
de obstáculo enquanto se carrega o animal. Os sons são 7 ao pegar e 9 ao soltar.

Cross coloca a 300 unidades à frente (`facing_steps[4..7]`). A colocação exige
chão válido e recusa água, caixa de venda, obstáculos do terreno/campo e a
área ocupada por outra galinha. A validação é repetida no fim da animação para
conservar a mão se outro animal tiver entrado no local. As portas têm
prioridade sobre colocar no chão e preservam o registro durante o fade.

As animações de pegar/soltar bloqueiam movimento e menus. Os menus abertos
enquanto se carrega pausam o mundo; a mochila não aceita o animal em vagas
de itens ou ferramentas. R1/R2 não trocam a mão durante o transporte.
Não se pode vender pelo menu da loja um registro que esteja carregado.

## Área de interação e movimento

`farm_object_contains` usa a área de 200 unidades para consultar a galinha
e retorna `FARM_HIT_CHICKEN` (`0x2008`). Em `player_probe_target`, essa
consulta produz o resultado 2; a parada rígida do movimento consulta o
resultado 4. Assim, esse animal é selecionável e participa da recusa de
colocação de outro animal, mantendo a passagem do jogador pelo seu espaço.
A pendência antiga de colisão física no roadmap foi esclarecida por essa
leitura do código. As caixas e a movimentação ainda precisam de comparação
com uma execução PS1; colisões dos demais tipos de animais são outro recorte.

## Alimentação, ovos e virada de dia

`hmbtn_poultry_hour` adapta `chicken_weather_tick` e a chamada horária de
`farm_hourly_refresh`. Uma adulta na fazenda, fora do inverno, acumula cinco
atualizações horárias para marcar alimentação do dia. O predicado original
compara o clima ao código `PLACE_FARM` (1): quando são iguais, retira 5 de
afeição; quando diferem, incrementa o contador. O clima atual do gameplay
nativo é ensolarado (0). As demais condições só estão exercitadas por fixtures;
isso não implementa o sistema completo de clima.

O observador do relógio executa essas atualizações mesmo com o jogador em
outra área ou durante o avanço do sono. Antes do processamento diário, a
alimentação externa fica disponível; o consumo dos cochos ocorre apenas
para adultas no galinheiro ainda sem alimentação marcada. Pintinhos não
consomem alimento. O contador horário é zerado na virada do dia.

Ovos guardam sua própria área e posição. Fora do galinheiro, são postos na
posição da adulta; a normalização para as faixas dos cochos se aplica somente
ao interior. A fazenda usa os índices sazonais de `estate_season_codes`,
incluindo o ovo dourado. Coleta, mochila, remessa e pagamento usam os mesmos
caminhos dos outros produtos.

O animal mantém sua área de origem enquanto está na mão, como o registro
original com `ACTOR_FLAG_HELD`. Ao soltar, recebe a área/posição de destino.
Antes de dormir, uma colocação forçada libera o registro. Na atualização
diária, animais deixados fora da fazenda/galinheiro retornam ao galinheiro
por `chicken_place`. Crescimento de um pintinho carregado atualiza o item
da mão; morte limpa a mão e deixa o funeral remover o registro e contar a vaga.

## Validação

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_poultry_transport_playthrough data
```

São 33 CTests sem recursos proprietários e treze percursos opcionais com os
dados locais. `native_poultry_carry` verifica as quatro direções, identidade,
recusas de colocação, proteção de inventário/venda, movimento, crescimento,
morte, clima/inverno e área dos ovos com terreno sintético.

O novo percurso declara uma adulta e um pintinho como fixtures, preservando
os 500G e os estoques vazios. Exercita menus, portas reais, duas noites de
alimentação externa, movimento da adulta, ovo recolhido/remetido, imagens
de ovos das quatro estações, retorno ao interior, transporte do pintinho,
liberação ao dormir e crescimento. Nenhum alimento ou dinheiro é concedido.
A visualização dourada é uma fixture de recurso; o concurso continua pendente.

Um terceiro argumento aceita pasta de capturas BMP fora do Git. As capturas
em `build/native-debug/poultry-transport/` foram inspecionadas com SDL
dummy/software; os outros onze percursos também foram revalidados no Linux.
Não houve nova janela real, build Windows ou comparação PS1 nesta etapa.

## Limites

Ainda faltam as demais espécies, cães selvagens, concursos, melhorias,
saves, clima completo e reações completas das cues. O PRNG e os intervalos
de caminhada permanecem host. A imagem na mão é composta sobre o cenário;
a oclusão completa com modelos continua pendente. Colocação sobre outros
tipos de ocupantes depende das consultas ainda não portadas.

O bootstrap `--diagnostics` não executa o jogo. Os testes host não substituem
o build original com toolchain PS1 e sua validação SHA-1.
