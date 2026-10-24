# Amizade e afeição na mudança de dia

`port/game/village.c` adapta `family_flags_update` de
`src/overlay/11_GF_FARM/people/villager.c`. A atualização consome as flags que
os scripts originais de conversa já escrevem. Não altera textos nem concede
pontos apenas por abrir a janela.

| Estado do morador no dia encerrado | Atualização original |
|---|---|
| Flag `0x400`, variável 44 | +200 de afeição para Karen, Ann, Elli, Mary e Popuri; +1 de amizade para os demais |
| Flag `0x200`, variável 45, sem `0x400` | Conserva os pontos, sem ganho diário |
| Nenhuma das duas flags | Sorteio de perda: 1 em 10; −200 de afeição para as cinco candidatas, −1 de amizade para os demais |

Kai, Kano, Gourmet, Louis, Greg e Won não perdem pontos nesse sorteio e não
consomem uma chamada do gerador. A rotina percorre todos os 37 registros, como
o original, incluindo os reservados. Afeição fica em 0..65535; amizade em
0..255. As duas flags diárias são limpas depois da atualização, conservando
apresentação e outras flags do registro. Conversar várias vezes no mesmo dia
mantém uma flag, portanto não multiplica o ganho na manhã seguinte.

Depois de atualizar os pontos, afeição **maior que 1500** ativa a flag original
correspondente (`0xE0..0xE4`). A rotina não a desativa quando o valor cai. Os
scripts existentes passam a consultar esses pontos/flags na próxima conversa;
isso não implementa as cenas de história ainda ausentes.

## Integração e aleatoriedade

O observador ordenado do relógio chama a rotina uma vez por mudança de dia às
06h, durante sono, passagem da noite sem dormir ou avanço de tempo de scripts.
A atualização ocorre antes da reconstrução da agenda diária. Sincronizar uma
agenda, carregar um interior ou selecionar a data da prévia não concede nem
consome os pontos diários. Menus e conversas continuam pausando o relógio.

`port/core/psx_random.*` adapta `rand_range` de
`src/main/graphics/screen_fade.c`: estado `uint32_t`, multiplicador
`0x41C64E6D`, incremento 12345, overflow de 32 bits e escala pelos 16 bits baixos
do intervalo. O estado dos moradores começa em zero, como a variável original.
Ele persiste na sessão, inclusive entre áreas/dias. O porte ainda não compartilha
esse estado com título, áudio e todos os animais; portanto a sequência global
de sorteios e a semente do instante de New Game não têm paridade PS1 completa.
Não há uso de `rand()` da biblioteca host nem dependência de `long`.

O recorte não tem esposa. A transferência da flag de conversa para a esposa e
`family_daily_update` dependem do futuro sistema familiar. Presentes e
aniversários agora estão em [VILLAGER_GIFTS.md](VILLAGER_GIFTS.md). Cenas
românticas, casamento, perda por lixo e saves continuam pendentes.

## Verificação

- `native_village_affinity`: vetores do gerador original, limites, ganho diário,
  proteção por conversa, negligência, seis exceções, consumo dos sorteios,
  limiar estrito das cinco flags, conservação das flags já ativadas, recarga de
  agenda, virada do ano e pausa.
- `native_village_talk`: uma ação não portada restaura as flags de conversa;
  dormir depois não credita um ganho revertido.
- `native_village_talk_playthrough`: conversa real com Lillia, repetição no
  mesmo dia e animação/transição de sono; amizade cresce uma única vez na manhã
  seguinte. Mantém a comparação das 93 solicitações de mensagem com os BINs.
- `native_world_clock_playthrough`: amizade/afeição atualizadas no sono da
  virada do ano e ao permanecer acordado até 06h, junto com os contadores.

Recursos continuam nos BINs instalados, com os leitores, decodificadores e
identidades existentes. Não há novo carregador gráfico. Testes locais Linux
SDL dummy/software não substituem Windows real nem a verificação PS1/SHA-1.
O porte permanece parcial; `--diagnostics` executa somente o bootstrap.
