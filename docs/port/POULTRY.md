# Galinhas, incubação e coleta silvestre

O porte permite ganhar dinheiro com coleta, comprar e nomear uma galinha de
Lillia, vê-la no galinheiro, alimentá-la, vender seus ovos e revendê-la à loja. Todo o estado
pertence à sessão: carteira, despesas, mochila, estoques, cochos e remessas.
Os atalhos continuam iniciando sem animais e com 500G; não concedem produtos.

## Compra e produção

A loja da área 128 mantém o expediente original: 11h–16h, fechada aos domingos,
com Lillia presente e jogador de mãos vazias. A segunda opção compra uma
galinha adulta por 1.500G, conforme `item_info[ITEM_CHICKEN]`.
O limite inicial é cinco. O editor reutiliza os oito glifos e controles de
`name_entry_step`: Cross insere, Circle apaga/cancela, Start confirma e a
pergunta final permite voltar. Cancelar conserva dinheiro e animais. A compra
só cria o animal e registra a despesa depois da confirmação; mensagens de
falta de dinheiro e lotação vêm do banco original.

`game/poultry.c` adapta `animals/chicken.c`: afeição inicial 10 e posições
de `place_grid_spot`, uma ração por adulta usando o primeiro cocho cheio,
mais 10 de afeição quando alimentada, menos 20 quando não, limite 0–255.
Ovos em incubação e pintinhos não consomem ração nem produzem ovos.
O processamento diário ocorre **antes de limpar os cochos**, inclusive no sono
e ao permanecer acordado até 6h. Os ovos anteriores são substituídos diariamente.

A produção consulta a alimentação de ontem e a afeição antes da atualização
de hoje. Uma galinha recém-comprada e alimentada precisa de duas viradas
de dia para produzir seu primeiro ovo. Os graus seguem os limites originais
de 100 e 200 de afeição; o grau dourado exige o prêmio, ainda não obtível
pelo concurso nativo. O ovo normal vende por 50G. Cross recolhe o ovo com
animação, R2 guarda/retira da mochila e Cross lança na caixa da fazenda.
O pagamento usa o fechamento já existente às 17h58 e respeita feriados.

`graphics/poultry_art.c` lê ANI de galinha adulta e texturas do slot 6 original.
A caminhada usa velocidade 6, quatro direções, consultas ao terreno e
distância do jogador/outras galinhas. Ações e animações param durante menus
e transições; às 21h começa o repouso. O intervalo de decisão usa um PRNG host
e não reproduz o fluxo global de aleatoriedade nem todas as cues do PS1.

## Coleta para financiar a compra

`game/farm_gather.c` usa os registros originais 125–172, runs 11–14,
com local, posição, máscara de estação, substituições de flores/ervas e
reposição diária. Inclui ervas daninhas, flores, ervas medicinais,
brotos de bambu, uvas e cogumelos nas estações apropriadas. Os recursos vêm
do pack de itens 5; não são imagens de inventário usadas como cenário.
Coletar exige mãos vazias, alcance, direção e altura do registro. Cada objeto
some uma única vez e retorna na próxima virada de dia quando seu flag permite.
Tocos e pedras que exigem ferramentas continuam fora deste recorte.

Produtos coletados e ovos usam a mesma mão/mochila das demais interações.
Lançamentos agora funcionam também fora da fazenda, com três slots e área de
origem. Voos de outra área ficam suspensos e não aparecem na região atual.
Lançar um produto no chão, sem caixa, descarta-o; não cria uma cópia permanente.

## Testar localmente

```bash
# Bosque/águas termais: brotos de bambu na primavera
./build/native-debug/port/hmbtn_native --data data --area-preview 72
# Loja de Lillia aberta; a compra exige ganhar o dinheiro na sessão
./build/native-debug/port/hmbtn_native --data data --area-preview 128 --preview-date 0:3 --preview-time 12:00
# Galinheiro inicialmente vazio
./build/native-debug/port/hmbtn_native --data data --area-preview 112

cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_poultry_playthrough data
```

São 33 CTests sem recursos proprietários e treze percursos opcionais com os
dados extraídos locais. O novo percurso começa com 500G, coleta e vende
21 brotos em sete dias, incluindo um dia sem coleta de Zack. Compra e nomeia
uma galinha e duas rações, testa cancelamento/recusa, atravessa as portas,
alimenta por duas noites e recolhe/vende o ovo: termina com 80G, receita
de 1.100G e despesa de 1.520G. Nenhum recurso é concedido pela fixture;
posições e horários são selecionados para abreviar o percurso. Também verifica
altura, movimento, pausa, repouso e recursos sazonais dos quatro mapas.
Um terceiro argumento aceita uma pasta de capturas BMP fora do Git.

O percurso também retorna à loja em um novo dia de expediente, cancela a lista
e a confirmação, vende a mesma galinha e confere o galinheiro sem esse animal.
Depois da venda, termina com 580G, receita de 1.600G e despesa de 1.520G.
Uma fixture separada, executada somente após concluir esse ciclo econômico,
habilita capacidade ampliada e o prêmio para testar a rolagem, os filtros e
a oferta de 2.000G. Essas condições ainda não podem ser obtidas pelo gameplay.

## Venda de galinhas

A terceira opção de Lillia adapta os estados 18–20 de `shop_poultry_step`
em `shop_dialogues.c`. A lista segue a ordem dos registros, mostra quatro
linhas, circula entre a primeira e a última e rola com indicadores acima/abaixo.
Nome de oito glifos, data, idade, estação, afeição e prêmio usam os recursos
originais da interface. O título incorpora o nome cadastrado da fazenda.
Ovos em incubação, pintinhos, nomes pendentes e animais mortos ficam fora
da lista; sem adulta disponível, a loja mostra a recusa `0x59`.

Cross seleciona o animal e abre a oferta original `0x60` (500G) ou `0x61`
(2.000G para vencedora). Cima/baixo escolhem Yes/No, Cross confirma e Circle
cancela. A lista também pode ser cancelada com Circle. Cancelar conserva
animal e dinheiro. Confirmar remove somente o registro escolhido, libera
a vaga, atualiza a contagem e credita carteira/receitas uma única vez, mesmo
com confirmações repetidas durante o fechamento. A carteira limita-se a
9.999.999G; o livro registra o valor integral da venda, como `estate_gold_change`.
Os ovos já postos permanecem independentes do registro do animal.

O jogador, o mundo e o relógio continuam pausados durante lista e confirmação.
Textos/retratos vêm do banco 2 (`0x49`, `0x58..0x61`); rótulos estáticos vêm
de `text_bank.c`. A comparação visual e temporal com uma execução PS1 ainda
está pendente. O concurso que concede o prêmio continua fora deste recorte.

## Incubação, crescimento e funerais

Cross na incubadora (zona original 34 da área 112, virado para a esquerda)
coloca o ovo que está na mão. A interação reserva uma das cinco vagas antes
da animação de depósito e consome somente esse ovo. Uma incubadora ocupada
recusa outro ovo com a mensagem `0x1E6`; sem vaga, usa `0x1A1`. A reserva não
conta como uma galinha visível na tela de estado, mas impede compras além da
capacidade. O ovo aparece no ninho com o sprite original, inclusive o dourado.

Depois de três viradas de dia, o pintinho aguarda o nome. O evento original
299 (`0x12B`) roda na manhã seguinte: fala inicial em casa, fade para o
galinheiro, pintinho no ninho, editor de oito glifos e volta à casa. Esse editor
não permite abandonar o nome, conforme `name_entry_start_no_cancel`. A
incubadora só é liberada no encerramento do evento. O pintinho passa mais sete
viradas de dia crescendo, sem precisar de ração. Ao virar adulto, passa a usar
o mesmo ciclo de alimentação e produção das galinhas compradas. Nascer de um
ovo dourado não concede um prêmio de concurso ao novo animal.

O evento original 172 (`0xAC`) resolve mortes por doença ou idade, com Carter,
Rick, cenário da igreja/cemitério, falas, animação de luto, música e fades.
As mensagens expandem a lista de nomes dos animais mortos por cada causa;
listas extensas são paginadas pelo renderizador nativo. O comando original
`ANIMAL_DEATH_CLEAR` remove apenas os registros marcados e libera suas vagas.
A remoção também incrementa o contador de descartes. O evento avança uma hora
no caso de idade e duas no de doença; o caso de doença aplica as perdas de
amizade/afeição aos moradores e ajudantes. O retorno libera controle na casa.
O fade continua após o fim do script, como o fade global do original.

Os dois eventos executam os recursos originais pelo interpretador C nativo,
com comandos específicos ligados ao estado real dos animais. Não há execução
MIPS nem substituição dos eventos por uma mensagem isolada. O teste adicional:

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_poultry_lifecycle_playthrough data
```

Esse percurso usa fixtures declaradas para morte por idade/doença e um ovo
inicial, permitindo testar sem esperar anos. Exercita a zona real de depósito,
três noites, nome obrigatório, volta à casa, sete noites de crescimento,
penalidades e reabertura das vagas. Renderiza os eventos com recursos locais.
O teste econômico anterior continua sem concessão de dinheiro ou produtos.

## Limites

O estado da fazenda já inclui a lista real de galinhas, com saúde, afeição,
data/idade e rolagem, conforme [POULTRY_STATUS.md](POULTRY_STATUS.md).

O medicamento animal já pode ser comprado de Lillia/Barley, equipado e usado
para curar a adulta doente com animação, consumo de dose e cue originais.
Comportamento, testes e limites em [ANIMAL_MEDICINE.md](ANIMAL_MEDICINE.md).

Adultas e pintinhos nomeados já podem ser carregados entre áreas. A fazenda
tem alimentação horária e ovos na posição da adulta; o retorno conserva o
registro. Regras, consulta de ocupação e evidências em
[POULTRY_TRANSPORT.md](POULTRY_TRANSPORT.md).

Ainda faltam venda das demais espécies,
tratamentos das demais espécies, concursos, melhorias do galinheiro e saves.
A segunda incubadora e a capacidade de dez vagas já têm regras na lógica,
mas a melhoria e o interior ampliado ainda não estão disponíveis no gameplay.
Os contadores de doença e idade seguem o original, com PRNG host; os
intervalos de caminhada continuam sem paridade com todas as cues. A consulta
original das galinhas seleciona o animal sem a parada rígida do jogador,
conforme o estudo de movimento em [POULTRY_TRANSPORT.md](POULTRY_TRANSPORT.md).
Vacas, ovelhas, cavalo e cachorro
continuam pendentes.

Validação somente Linux local, usando C11/SDL3 compartilhado com Windows;
nenhum novo build Windows. O bootstrap `--diagnostics` não executa o jogo.
Os testes host não substituem o build PS1 e sua validação SHA-1.
