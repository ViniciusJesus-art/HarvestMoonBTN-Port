# Primeiro ciclo das galinhas e coleta silvestre

O porte permite ganhar dinheiro com coleta, comprar e nomear uma galinha de
Lillia, vê-la no galinheiro, alimentá-la e vender seus ovos. Todo o estado
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

São 32 CTests sem recursos proprietários e nove percursos opcionais com os
dados extraídos locais. O novo percurso começa com 500G, coleta e vende
21 brotos em sete dias, incluindo um dia sem coleta de Zack. Compra e nomeia
uma galinha e duas rações, testa cancelamento/recusa, atravessa as portas,
alimenta por duas noites e recolhe/vende o ovo: termina com 80G, receita
de 1.100G e despesa de 1.520G. Nenhum recurso é concedido pela fixture;
posições e horários são selecionados para abreviar o percurso. Também verifica
altura, movimento, pausa, repouso e recursos sazonais dos quatro mapas.
Um terceiro argumento aceita uma pasta de capturas BMP fora do Git.

## Limites

Não há incubação, pintinhos, reprodução, transporte de galinhas ao exterior,
venda de animais, medicamentos, concurso, melhorias do galinheiro ou saves.
Os contadores diários de doença e idade seguem o original, com PRNG host;
o evento de funeral ainda não é executado. Um animal marcado como morto fica
invisível com seu registro reservado, aguardando esse evento; a remoção e
liberação da vaga fazem parte do próximo recorte. O jogador ainda não colide
fisicamente com as galinhas. Vacas, ovelhas, cavalo e cachorro continuam pendentes.

Validação somente Linux local, usando C11/SDL3 compartilhado com Windows;
nenhum novo build Windows. O bootstrap `--diagnostics` não executa o jogo.
Os testes host não substituem o build PS1 e sua validação SHA-1.
