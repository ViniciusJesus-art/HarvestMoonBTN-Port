# Fonte termal e animações de água — correção de fidelidade

Revisão de 2026-11-04, solicitada após testar o banho no jogo. Corrige a porta,
a distância de saída e os sons da fonte; integra as tabelas originais de
animação das texturas dos cenários. Linux/Windows; runtime ainda parcial e
bootstrap `--diagnostics` sem execução do jogo.

## Porta e retorno da fonte

A implementação anterior confundiu as passagens 46/47 com passagens sem folha
de porta. O recurso instalado da área 72 mostra uma **porta dupla real**:

| Modelo | Flags | Posição original |
|---|---|---|
| 0 | `0x400`, twin 1 | `(-8540,-1892,4400)` |
| 1 | `0x200` | `(-9060,-1892,4400)` |

`world/doors.c:door_step` e `farm_init.c:farm_zone_trigger` fornecem abertura
até `0x420`, aceleração 8, direção do giro, espelhamento, espera 11 e sons
**0x16** ao abrir / **0x4c** ao fechar. `area_doors` volta a aceitar os eventos
46/47; o banho solicita a operação comum e aguarda abertura suficiente antes
de atravessar. O modelo 1 acompanha o giro oposto do modelo 0. Não foi alterado
o bloco de animação/transição de portas do usuário em `runtime.c`.

A colisão consulta a posição da folha e a caixa da zona, com o alcance de
48 usado pelo movimento atual. A porta fechada/em movimento bloqueia a
travessia sem Cross, inclusive correndo. O banho não reutiliza a transição
genérica de cômodos: continua com seus bancos e saltos próprios.

A caminhada de saída de 37 ticks estava começando depois do salto, embora a
zona 47 já pudesse ser detectada no ar, afastando o personagem da cabine.
Agora a passagem espera a porta e devolve o controle assim que o corpo libera
a frente da entrada. O limite deriva das caixas instaladas e do alcance 48;
37 permanece apenas como limite da ação. O percurso observado devolve o
controle em **Z 4231**, perto da entrada, em vez de continuar até o campo.

Há uma diferença sazonal importante: a zona de saída começa em Z **4300** na
primavera, e em **4496** nas demais estações; a entrada continua começando em
**4300**, com folhas em **4400**. A liberação considera ambas as caixas para
não devolver o personagem ainda dentro da cabine no inverno.

Os sons de banho estavam no helper de menu `hmbtn_sound_effect`, que aceita
somente índices menores que 16 e usa outros instrumentos. A fonte agora
resolve `hmbtn_farm_sound` e envia banco/programa/timbre/nota/volume reais:

- **6** nos saltos de entrada/saída;
- **16** na entrada na água;
- **22/76** na abertura/fechamento, pelo mecanismo comum de portas;
- passos durante os trechos caminhados, pela operação já usada na exploração.

O provedor exige uma voz real do banco carregado; não aceita silenciosamente
um pedido de som indisponível. O teste observa os comandos completos e os
encaminha ao áudio original, em vez de verificar somente um número na fase.

## Movimento da água

O original não usa um shader de água: `world/scene_view.c` copia e desloca
retângulos de VRAM, em palavras de 16 bits. As oito entradas de **18 bytes**
começam em **0x8a8** da seção de cada mapa:

`id, duração, próximo, destino X/Y, origem X/Y, largura, altura`.

- Id com bit **0x8000**: copia a figura e troca para o próximo registro ao
  atingir sua duração. A fila registra somente a primeira ocorrência de cada
  id; os demais registros são os quadros seguintes.
- Sem esse bit: desloca a imagem verticalmente em duas fatias, incrementa o
  deslocamento em **2** quando o contador alcança `próximo` e retorna a zero
  ao atingir a altura. Conserva alturas ímpares e a ordem das cópias.
- Local **44**, ids de imagem ímpares: repete a cópia em X+8 e X+16, como a
  espuma do mar original.
- Local **15**: não registra a fila sob chuva/neve na carga do mapa, conforme
  `scene_mode_queue_run`. Lareira da casa de inverno 91 espera a flag 0x45.

Exemplos dos recursos instalados:

| Área | Sequência |
|---|---|
| 0, fazenda | riacho/lago: rolagem de retângulo 16×31 palavras, intervalo 5 |
| 56, praia | mar: quadros com durações 16/12/8/12; espuma com 2/3/4 e repetição |
| 60, lago da montanha | retângulo 32×128, intervalo 32 |
| 68, rio | duas rolagens 8×31 / 10×63, intervalos 8 / 4 |
| 72, fonte/cachoeira | quatro rolagens de alturas 31/31/43/63, intervalos 8/8/8/0 |

No overlay instalado `11_GF_FARM.bin`, base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**, o mapa PS1 referencia:
`slot_add` **0x800db140**, `scene_mode_queue_run` **0x800db2ac**,
`slot_image_step` **0x800db354**, `slot_rect_step` **0x800db4e0** e
`slots_step` **0x800db5cc**. O código nativo adapta essas operações diretamente;
endereços de auditoria não são convertidos em ponteiros host.

`scene_slots` valida os limites de origem/destino e dos saltos, mantém oito
slots e copia tanto palavras quanto presença no atlas do cenário. `memmove`
e a ordem das linhas conservam retângulos sobrepostos sem alocação por tick.
O host inicializa a fila na carga da área e a atualiza na simulação, nunca no
desenho. O menu pausa a atualização; Esc já suspende os ticks pelo aplicativo.
Troca/carregamento de mapa reinicia somente essa apresentação transitória.

## Verificação e limites

`scene_slots_test` verifica rolagem/retorno, altura ímpar, cadência de quadros,
deduplicação, espuma repetida, pausa, clima, lareira e dados fora dos limites.
`water_playthrough` usa os cinco mapas acima, compara as palavras das regiões
animadas por 48 ticks, desenha pelos recursos reais e confirma que desenhar
sem atualizar não altera a textura. Confere a pausa pelo menu em cada mapa.

O percurso da fonte foi ampliado: correr contra a porta fechada não atravessa;
as duas folhas giram em sentidos opostos, os sons chegam ao provedor real,
o personagem sai perto da cabine e as folhas fecham. Mantém os casos de
recuperação, três retomadas, noite/chuva e inverno/neve. Capturas locais da
porta aberta e do retorno foram inspecionadas; nenhum asset proprietário foi
acrescentado ao repositório.

Configure/build Linux Debug e Release aprovados: Debug **13/13**, Release
**4/4** selecionados, incluindo os percursos e três retomadas por configuração.
Build Windows MinGW sem rede aprovado; unidades dos slots e banho, percurso
com os cinco mapas de água, percurso completo da fonte e três retomadas no
Wine aprovados.

**47 processos de teste**: **24 Debug**, **9 Release**, **14 Wine/targets**,
incluindo CTest, wrappers, filhos, lançadores e três tentativas iniciais que
falharam durante os ajustes/verificações. Não houve suíte inteira; teto de 50
respeitado.

A contagem inclui tentativas iniciais: ajuste da expectativa de retorno de
uma altura ímpar na unidade e verificação da caixa sazonal de saída da fonte.
O banco de save continua v22, sem novo estado serializado ou política de
compatibilidade. Recursos originais/SDK/disco/toolchain PS1 permanecem intactos.
A validação host não substitui a verificação SHA-1 do PS1; Wine não equivale
a teste em hardware Windows físico. Ofertas à Deusa seguem como próximo
recorte do roadmap, depois destas correções solicitadas.


## Correção após teste manual: acesso ao banho e fundo do mar

A colisão da cabine acrescentava 48 unidades ao raio que `house_control`
já inclui no ponto de sondagem. O personagem parava em Z=4274, antes da
caixa original de interação (Z>4300). A barreira agora usa o plano original
da porta (Z=4400), incluindo igualdade para impedir que o deslizamento da
corrida atravesse o plano. O percurso começa 600 unidades antes da caixa e
chega pelo controle normal: corrida na primavera e caminhada no inverno.
As animações personalizadas de portas em `runtime.c` não foram alteradas.

O escurecimento do mar vinha do fundo preto. `world/scene.c` inicia o fundo
da praia em (0,165,213) e alterna entre (0,99,127) e (0,165,213), em 512
passos por sentido, com repetição das extremidades. O mar translúcido
precisa desse fundo para a mistura de cores. `scene_slots` adapta essa
sequência e `scene_preview` usa a cor antes de desenhar a geometria; a luz
do horário/clima continua aplicada depois, conforme o original. Outras
áreas mantêm o fundo preto. Pausa congela a sequência e a carga da área a
reinicia; não há alteração no formato do save.

A unidade cobre os dois extremos, o ciclo completo, a pausa e o retorno ao
fundo preto em outra área. O percurso de água agora olha para o oceano e
confere um pixel de fundo azul na imagem renderizada, além das texturas.
A captura anterior na escadaria e os hashes das texturas, isoladamente,
não verificavam a cor do mar.


Validação desta correção: configure/build Linux Debug e Release aprovados;
CTest selecionado **8/8 Debug** e **3/3 Release**, incluindo retomadas em
processos novos. Build Windows aprovado, com unidades dos slots/banho e
percursos completos da água/fonte aprovados no Wine. Capturas do oceano e
da entrada da fonte inspecionadas. `git diff --check` aprovado.

Foram **44 processos de teste** nesta revisão: cinco execuções diretas,
23 processos nas duas rodadas Debug (a primeira encontrou a passagem no
contato exato do plano), oito na rodada Release e oito lançadores/targets
Wine. A primeira execução direta reproduziu o bloqueio anterior à zona de
interação. Nenhuma suíte completa foi executada; a contagem inclui falhas,
CTest, wrappers e filhos. Wine não substitui teste em Windows físico e o
build host não substitui validação SHA-1/toolchain PS1.
