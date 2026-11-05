# PF-06 — Kappa e Mystic Berry

Recorte de 2026-11-05 para Linux/Windows. O runtime permanece parcial e o
bootstrap de diagnóstico não executa o jogo. Fontes/SDK/disco e fluxo CMake
PS1 continuam intactos; testes host não substituem toolchain/SHA-1 PS1.

## Auditoria C/BIN

A referência é `people/player.c:thrown_item_settle`, `player_entry_refresh`
e `player_fatigue_cost`, além de `event_commands.c:event_cmd_check_gift`,
`farm_nudge_axis`, câmera por variáveis e espera do horário. O item 294
(`ITEM_MYSTIC_BERRY`) está em `src/main/game/farm_tables.c`: callback de
status com stamina 0/fadiga −1. Seu consumo também ativa
`PLAYER_FLAG_HALF_FATIGUE`; não aumenta a energia máxima.

A tabela da área 68, e suas variantes sazonais, seleciona o evento **197**,
modo **4**, destinatário **39**, flags **0x7ffcb800**. A janela instalada
`0x3ffffd72` começa às 11h e abrange 18 horas (11h..04h). As condições de clima
aceitam todos os cinco valores; o pré-requisito exige o flag **0xCC** livre.
O teste de item usa a reação original da coluna 39; não há uma segunda
lista nativa de itens permitidos.

`12_event_s.bin:0x47b800..0x47c800` contém o roteiro de 43 instruções. O
primeiro pepino inicia o evento. Após fala/mergulho e retorno da câmera,
o jogador recupera o controle para oferecer mais dois pepinos na mesma
visita. As três falas são **541/542/543**. Sair da área ou chegar ao amanhecer
encerra a sequência incompleta; a espera e o progresso transitório continuam
nos cursores/variáveis do interpretador. O próprio script põe o flag 0xCC
antes de entregar a recompensa, impedindo repetição depois de concluído.

O ator do evento é **27**, com ANI **0x201B**, embora a reação da oferta use
39. A Berry usa a ANI de staging **0x0926** e o banco do bloco. Ela difere
da imagem genérica de `00_gf.bin:0x3c19000 + 294*0x800`; sua paleta/UVs
originais são usados na apresentação. A inspeção detectou a imagem genérica
vermelha e confirmou a Berry azul depois desta adaptação. Nenhuma imagem
ou recurso proprietário foi acrescentado ao Git.

## Adaptação

`game/kappa.c` resolve apenas os comandos ausentes de presente pendente,
recompensa e espera de 06:00. O começo do evento usa o mesmo modo 4 e a
mesma zona 44 de `farm_carry`/`farm_visit`; presentes posteriores chegam ao
campo transitório `visit.gift`, equivalente ao `currentAction` do original.
Presentes recusados são consumidos pelo arremesso, sem avançar o roteiro.
Embrulhos continuam entregando o conteúdo pelo caminho compartilhado.

O nudge vertical já adaptado para a Deusa foi movido para `farm_intro`,
compartilhado pelos dois roteiros. A câmera por variáveis chama o glide
existente. O movimento vertical destes atores não é assentado no terreno
no meio da subida/descida. O respingo continua durante o roteiro. Na virada
da manhã, a espera −1 encerra o script antes de terminar a transição da casa,
liberando os atores; saídas de área usam a notificação existente.

A recompensa usa a sequência de consumo já existente, restaura a mão
anterior e só ativa `farm_action.half_fatigue` ao concluir a ANI. O campo
pertence ao estado do jogador, não à UI nem ao evento. A contagem normal
de Power Berries e a energia máxima continuam independentes.

`hmbtn_farm_fatigue_cost` adapta o cálculo original: clima, noite/antes das
06:00, energia zerada e ajuste; divide a soma por dois com arredondamento
inteiro para baixo. Ferramentas, pesca, cuidados animais e medicina usam a
mesma operação. A recompensa não reduz o custo de energia e não altera as
variações diretas de fadiga de alimentos, horas ou sono.

O flag 0xCC já pertence à vila e já é salvo. Apenas o campo de meia fadiga
foi acrescentado ao codec atual (v24); o restante do progresso do evento
não é serializado. Guardas de ações/eventos e publicação transacional dos
saves existentes são preservadas.

## Percurso e testes

`kappa_test` cobre matriz de clima/horário/energia/ajuste e arredondamento,
consumo único/restauração da mão, custos reais da pesca/medicina, condições
originais do gatilho, consumo da intenção de presente, persistência e manhã.

`kappa_playthrough` compra sementes de pepino por 200 G, prepara nove células,
semeia, rega e dorme nove noites. Os produtos são colhidos por ações normais;
nenhum produto maduro é criado pelo teste. Posição, dinheiro inicial, horário
para os ramos de janela/amanhecer e solo limpo são fixtures.

O percurso verifica alvo seco, horário recusado, item de coleta recusado
durante o evento, cancelamento pela saída real da área, cancelamento na
manhã após duas ofertas, conclusão com três ofertas na chuva, falas/ANI/
câmera e imagem da Berry. Uma enxadada com energia zerada, chuva e noite
custa quatro pontos de fadiga antes e dois depois. Confere energia máxima
inalterada, ausência de repetição e saves anteriores/posteriores à recompensa.
O wrapper abre esses dois arquivos pelo aplicativo em processos novos.

Para conferir jogando, leve três pepinos ao lago da montanha e arremesse-os
na água a partir das 11h, sem sair da área entre as ofertas. Espere o controle
voltar após cada aparição. A terceira entrega apresenta e consome a Berry
azul; o benefício reduz a fadiga das ações futuras, sem ampliar a barra de
energia. Uma sequência abandonada deve ser recomeçada.

Arquivos principais: `game/kappa.[ch]`, `farm_intro.c`, `farm_visit.[ch]`,
`exploration.c`, `runtime.c`, `event_item.c`, `farm_action.[ch]`, `fishing.c`,
`poultry.c`, `save.[ch]`, `graphics/scene_preview.c` e testes/CMake nativos.
O bloco de travessia/animação das portas do usuário foi comparado com o
início da rodada e permanece idêntico.

## Validação desta rodada

Linux: configure/build Debug e Release concluídos. A seleção Debug passou
**17/17** em 45,86 s: Kappa, Deusa, fonte, saves, pesca, medicina, consumo,
ações animais, arremessos, portas/chuva, fidelidade do mundo e águas. Release
passou **3/3** em 3,50 s: unidade Kappa, percurso com duas retomadas pelo
aplicativo e águas. Quatro execuções dirigidas anteriores cobriram a unidade,
o percurso e a inspeção/correção da arte de staging da Berry.

Windows x64: build MinGW Release no contêiner local concluído. No Wine,
passaram a unidade Kappa, o percurso completo, a unidade de regressão da
pesca e duas retomadas em processos novos do aplicativo (antes/depois da
recompensa, três frames cada). Não houve validação em Windows físico/MSVC.

Total conservador: **49 processos de teste**, incluindo as quatro execuções
dirigidas, CTest, wrappers/filhos e lançadores Wine/executáveis. Limite de
50 respeitado, sem suíte inteira. `git diff --check` aprovado; o bloco das
portas do usuário foi comparado novamente e permanece idêntico.

Reprodução das seleções, após os builds correspondentes:

```sh
ctest --preset native-debug -R '^(native_kappa|native_kappa_playthrough|native_goddess|native_goddess_playthrough|native_hot_spring|native_hot_spring_playthrough|native_save|native_fishing|native_fishing_playthrough|native_poultry_medicine_playthrough|native_event_item|native_food_use|native_animal_tools|native_farm_carry|native_weather_doors_playthrough|native_world_fidelity_playthrough|native_water_playthrough)$'
ctest --preset native-release -R '^(native_kappa|native_kappa_playthrough|native_water_playthrough)$'
```

São seleções dirigidas, não a suíte inteira. As seleções Linux usam
renderização por software e áudio dummy; capturas verificam apresentação, mas não
substituem avaliação de janela e som em hardware real. O runtime/PF-06
continuam parciais. Próximo recorte: cachorro inicial.
