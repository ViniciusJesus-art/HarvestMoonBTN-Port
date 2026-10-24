# Visitas de Thomas e entrega da torta

Os eventos **128..133** e **288** executam os roteiros de `12_event_s.bin`, com
textos de `03_messege1.bin`. Thomas usa ANI e banco gráfico do evento;
Ellen, Elli e Stu reutilizam os recursos de moradores da área. Não há
falas recriadas. O roteiro controla chegada, conversa, escolhas, movimentos e
despedida; `farm_intro` continua sendo o interpretador visual usado pela abertura.

## Gatilhos originais

`game/farm_visit.*` lê as entradas de 36 bytes da tabela de eventos da seção
sazonal em `00_gf.bin`. Os campos são lidos como valores little-endian, sem
converter estruturas ou endereços PS1 em ponteiros host. A tabela tem limite
de 100 entradas e sentinela `0xFFFF` quando termina antes desse limite.

O recorte avalia somente os sete eventos integrados. Confere janela de anos,
dias e horas, local anterior, modo de disparo, intervalo de contador e sua
inversão, amizade, flags exigidas/proibidas, pré-requisitos e máscaras de
estação, dia da semana e clima. A condição 39 verifica a torta original na
mão; o bit de mão livre bloqueia a coleta com Doug quando há outro item.
Condições de disponibilidade, variáveis e portas de inventário fora do
recorte impedem o disparo. Carregar uma tabela
com outros eventos não os torna executáveis.

Na edição instalada usada para validação:

| Evento | Condições relevantes da tabela |
|---|---|
| 128, favor da torta para Ellen | Anos 1..3; verão/outono/inverno; domingo; dias 6..27; 06:00..17:59; amizade de Thomas pelo menos 100; contador 8 fora de 0..3; evento ainda não registrado |
| 288, ingredientes para o festival | Primavera, dia 15; 06:00..11:59; entrada na fazenda vindo do local 2 (casa); contador 26 diferente de zero; máscaras originais de clima |
| 129, coleta com Doug | Balcão da Hospedaria (área 204); modo 10 original; pedido aceito; contador 13 em zero; mão livre; evento ainda não registrado |
| 130, entrega a Ellen | Casa de Ellen (área 172); interação com a moradora 23; torta `204` na mão; coleta registrada; contador 13 em zero |
| 131, retorno com bolo | Fazenda, 06:00..11:59; contador 13 entre 1 e 127; entrega registrada |
| 132, torta buscada mas não entregue | Mesmo intervalo do retorno; coleta registrada e entrega ausente |
| 133, torta não buscada | Mesmo intervalo do retorno; pedido aceito, coleta e entrega ausentes |

O porte usa os campos do BIN, não essa tabela documental como calendário de
gameplay. Atalhos de prévia começam sessões novas: mudar a data não concede
amizade nem simula anos anteriores.

## Conversa e efeitos na sessão

Thomas caminha da entrada norte até a casa e espera por interação. Enquanto
espera, o jogador pode andar, trabalhar e abrir menus; o relógio avança quando
as regras normais permitem. A conversa pausa relógio e controle. A visita usa
a mesma seleção de alvo e a mesma colisão de NPCs: caminhar bloqueia, correr
permite atravessar. Não aparece uma segunda cópia de Thomas da agenda comum.

O bytecode pode receber a interação durante a aproximação. Se o jogador não
conversar, Thomas parte quando chega ao horário inicial mais uma hora; sair
da fazenda também encerra o evento. Os modos de término devolvem controle e
a agenda no horário corrente. Menus e conversas com outros moradores pausam
o andamento do roteiro da visita.

- **Torta:** os registros 385..389 fornecem saudação, pergunta e respostas.
  Aceitar acrescenta 20 de amizade com Thomas e ativa a flag `0xCB`; recusar
  retira 20. O contador 13 é iniciado na chegada. A marca `251 + 128` é
  registrada ao começar, inclusive quando a visita termina sem resposta.
- **Festival:** a saudação usa o registro 2041. O ano original, contado desde
  1, módulo 9 escolhe entre os registros 872, 898, 902, 906, 910, 914, 918,
  922 e 926. Aceitar ativa a flag `0xD9` e segue para o registro 873. Recusar
  limpa a flag, reduz amizade em 20/afeição das candidatas em 1.000 e afeição
  dos ajudantes em 20, com limites inferiores, e usa o registro 874. O
  contador 26 é zerado. O bit original da requisição suprime a marca de
  conclusão permanente, permitindo outra visita em um ano posterior.

O resultado da escolha vai ao slot pedido pelo script: 0 no favor da torta,
1 no pedido do festival. As flags agora cobrem as 37 palavras de `eventFlags`
originais (1.184 bits), necessários inclusive para consultar a marca 539.

## Entrega da torta e retorno de Thomas

Depois de aceitar, vá à Hospedaria no mesmo dia e interaja no balcão de Doug,
com a mão livre. A seleção usa a zona original de entrega, incluindo alvo e
direção; o modo 10 corresponde ao despacho original de Doug nesse local.
O registro 390 e a apresentação com a ANI base colocam a torta `204` na mão,
acrescentando 20 de amizade com Doug. A marca do evento impede duplicação.
O menu geral de compras da Hospedaria continua pendente.

Leve essa torta na mão à casa de Ellen e interaja com ela no mesmo dia. O
evento 130 retira o item e abre a escolha do registro 391. A primeira opção
segue diretamente para o agradecimento. A segunda usa o registro 393 e a
sequência original com Elli: entrada, caminhada, conversa 394, mudança de
olhares quando o banco de retrato chega a 145 e devolução de uma torta ao
jogador. Os dois caminhos acrescentam 20 de amizade com Ellen e **300G**,
também registrados como receita no livro-caixa. Uma torta guardada na mochila
ou uma receita de outro ID não substitui o item exigido pela condição original.

Após a entrega, o evento permanece até sair da casa. Ellen pode repetir o
registro 392 e Stu usa o 2045 conforme o alvo da interação. O roteiro devolve
as cópias dos atores às agendas ao terminar, sem duplicar personagens.

No dia seguinte, volte à fazenda antes do meio-dia. O contador 13 avança pelo
observador de dias já existente; a tabela escolhe um dos três retornos:

- **131:** Thomas agradece nos registros 395..397 e oferece o bolo `225`.
  O comando original pede a pose de comer, portanto o bolo é consumido com
  a ANI base 5, aplicando a linha original do item (fadiga -4, energia +0).
  O item que estava na mão é restaurado integralmente depois da animação.
- **132:** se a torta foi buscada sem ser entregue, a chegada retira 30 de
  amizade com Doug e Thomas e 20 com Ellen; a conversa usa 398.
- **133:** se a torta não foi buscada, a chegada retira 30 de amizade com
  Thomas e 20 com Ellen; a conversa usa 399.

Os valores saturam no limite inferior. As penalidades já ocorrem na chegada,
inclusive se o jogador ignorar Thomas até ele partir. As marcas próprias
impedem repetição dos retornos; os itens não são apagados por uma cobrança.
As escolhas, pagamentos e consequências são comandos do BIN, sem condições
de missão reescritas no coordenador.

## Recursos e limites

`filesystem/event_resources.*` resolve o ID original de evento ou de seção
sazonal. Os intervalos vêm de `event_script_offsets` e `section_table` durante
o configure; o coordenador não contém offsets novos dos eventos. A abertura,
primeira manhã, funeral e nascimento também usam essa interface. Leitor BIN,
decodificadores de bloco/ANI/banco, atlas e leitura de mensagens permanecem os
existentes. Essa identidade permite uma futura associação dos gráficos
extraídos a PNGs; não há suporte PNG implementado. Os atores 9, 23 e 34 do
evento 130 usam `village_art` por ID, sem carregar bancos por offsets novos.
`game/event_item.*` executa a apresentação da torta e o consumo do bolo com
a ANI base e as imagens por ID já usadas pelo inventário. A animação continua
até terminar mesmo quando o roteiro de Doug já devolveu controle.

O pedido da torta inclui coleta, entrega, escolhas e retorno com recompensa
ou cobrança. A entrega dos ingredientes e o festival completo continuam
pendentes. Aceitar um pedido não concede itens ou recompensas antecipadamente.
As flags, dinheiro, itens e contadores ficam na sessão; ainda não há save/load.
Outros eventos da tabela continuam pendentes. Paridade temporal/visual exata
com o PS1 e execução Windows real não foram verificadas.

## Verificação

`native_farm_visit` usa fixtures sintéticas para truncamento, limite de 100
entradas, janelas, inversão de intervalo, bases de flags, clima, modos,
repetição, efeitos e limites de recursos. `native_farm_visit_playthrough`
usa somente os arquivos originais instalados localmente: percorre as nove
perguntas do festival, aceitação/recusa, tempo esgotado, saída da fazenda e
os dois caminhos do pedido da torta. Compara todos os glifos/controles das
mensagens com os BINs, verifica os slots de resposta, amizade/afeição,
controle, pausa do relógio, colisão/corrida e ausência de repetição na mesma
data. Os testes existentes de abertura/primeira manhã continuam aplicáveis.

`native_event_item` verifica consumo, limites e restauração do item anterior.
`native_pie_quest_playthrough` executa os eventos 128..133 com os BINs locais,
sem conceder itens/flags de missão por fixture. Usa a zona real do balcão,
percorre ambas as escolhas de Ellen, sincronização do retrato, 300G/livro-caixa,
bolo/restauração da mão e os dois tipos de cobrança, incluindo ignorar a visita.
Todas as mensagens são comparadas com os glifos/controles originais. A suíte
Linux tem 63 testes aprovados: 43 da base e 20 percursos com BINs.

Capturas locais de escolhas foram inspecionadas em SDL dummy/software, fora
do Git. Para configurar, compilar e testar:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

O teste com BINs exige `HMBTN_TEST_GAME_DATA` apontando para a extração local.
O modo `--scene-preview` continua sendo uma vista geométrica sem gameplay e
não carrega tabelas de eventos. `--diagnostics` continua sendo apenas o
bootstrap; a validação host não substitui o build PS1/toolchain/SHA-1.
