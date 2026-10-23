# Primeiro passeio com Thomas e apresentação de Zack

A primeira transição casa 80 → fazenda 0 da sessão dispara o
evento original 12, em `12_event_s.bin`, intervalo 0x36000..0x41800.
O gatilho é consumido uma vez na sessão e preserva inventário e terreno.
Os scripts continuam despachando funções C nativas, sem executar MIPS.

Antes da primeira fala, o host executa a aproximação de Thomas pela faixa
da entrada norte, desde `(-3888,0,7895)` até `(-3888,0,3959)`, com a ANI de
caminhada e passos de 24 unidades. É uma preparação nativa da visita: o
bloco do evento 12 começa com o posicionamento perto da casa. O roteiro
importado só começa após a aproximação, sem exibir Thomas diretamente no
ponto da conversa. Os bytes do evento permanecem intactos.

Durante as caminhadas, os atores consultam o chão com máscara `0xC000` e o
jogador com `0xC080`, tolerância 200, antes da câmera. A altura acompanha o
modelo original, incluindo a descida na estrada 32; não fica presa em -800.

O evento usa as ANIs/texturas do jogador, Thomas, Ellen e Zack, posições,
direções, caminhadas e diálogos 64..80 do original. Expande os nomes cadastrados e reconhece a lista Sim/Não
no meio da mensagem. Setas selecionam, Z confirma; o resultado do menu é 1/2,
conforme `menu_choice()`. Os comandos de retrato selecionam a próxima face;
`0xFF7` aplica seu banco, lido em intervalos de 6 KiB de `01_face.bin`.
Isso permite trocar o retrato durante as falas sem substituir o texto ou
reabrir a caixa. As esperas do roteiro pelos bancos 105 e 159 acompanham
essas trocas reais, nas conversas com Ellen e Zack.

Aceitar continua a partir do cursor 15. A caminhada aciona a zona norte real
da fazenda e carrega a estrada/área 32. O fade de entrada vem do próprio
roteiro, depois da confirmação do carregamento. Seguem as áreas 36, 40, 48 e
52: biblioteca, Ellen, casa de Thomas, supermercado, clínica, indicação da
igreja, hospedaria e praça. As conversas usam a câmera e as poses do script.
Ao retornar à fazenda, o comando `0x4C` acrescenta as duas horas do passeio
ao relógio da sessão (06:00 → 08:00). Relógio contínuo e calendário estão
integrados; detalhes em [CLOCK.md](CLOCK.md).

Recusar usa as falas 78/79 e segue diretamente para o cursor 147, sem
avançar o horário. Ambos os caminhos continuam com Zack: aproximação,
apresentação, conversa 80 completa, movimento de câmera até a caixa de
venda, marcador sobre o jogador e restauração da posição/câmera.
O controle volta somente após a saída de Zack e o término original do
evento (cursor 174). Thomas, Ellen e Zack deixam de ser atores do evento.

O interpretador suporta as subrotinas `0x4F/0x50` sem modificar os registros
importados. O retorno é um cursor host validado; nenhum endereço PS1 é
convertido em ponteiro. Cargas pendentes conservam o cursor até que os
recursos existam. O terreno mutável é reaplicado ao recarregar a área 0,
mesmo enquanto o controle livre está bloqueado pelo evento.

O evento completo está integrado; o **porte do jogo permanece parcial**.
Os destinos externos do passeio agora têm exploração livre, conforme
[WORLD_MAP.md](WORLD_MAP.md). Rotinas de moradores e interiores continuam
pendentes. A entrega de produtos e o pagamento diário foram
integrados separadamente em [SHIPPING.md](SHIPPING.md). Saves e persistência do gatilho continuam
pendentes. O bootstrap de diagnóstico não executa o jogo e os testes host
não substituem a validação SHA-1 do build PS1.

## Validação reproduzível

Configure/build e os 26 CTests Linux passaram, incluindo casos sintéticos
de chamadas/retornos, espera por retrato durante uma mensagem, carregamentos
pendentes, restauração das coordenadas `(y,x,z)` e mudanças de banco por página.

O driver de integração opcional usa os dados extraídos do usuário:

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_first_morning_playthrough data
```

Ele percorre Sim e Não, verifica os seis cenários, todas as mensagens de cada
ramo, nomes com oito glifos, bancos de Ellen/Thomas/Zack, marcador, horário,
fim real do evento, mochila/terreno conservados e nova entrada sem repetir
a visita. Verifica a aproximação e o jogador virado para Thomas na primeira
fala, além de alturas intermediárias na
escada para os dois personagens. Renderiza cenários e trocas de retrato.
Pode ser registrado no CTest configurando `HMBTN_TEST_GAME_DATA` com o caminho
absoluto da pasta extraída. Sem essa opção, a suíte continua sem assets.

Um terceiro argumento seleciona uma pasta existente para capturas BMP. Evidências
locais ignoradas pelo Git: `build/native-debug/first-tour-playthrough.log`,
`first-tour-x11.log` e `tour-captures/`. O driver também passou em janela real
Linux X11/software. O cross-build MinGW x64 passou; Windows real, comparação de
imagem/ritmo com PS1, mistura de áudio e paridade dos fades continuam pendentes.
