# Interiores iniciais da fazenda

As portas externas agora dão acesso ao estábulo (92), celeiro (96) e
galinheiro (112), com geometria, texturas, entradas e saídas originais nas
quatro estações. Z/Cross dentro da zona, olhando para a porta, inicia o giro.
O jogador espera o giro terminar, caminha na direção da entrada e atravessa
o fade. Caminhar pela saída interna devolve o controle à fazenda.

O giro usa aceleração 8, limite `0x420` e o sinal correspondente à direção,
conforme `world/doors.c`. A rotação inicial do modelo é preservada. Modelos
com porta dupla giram em sentidos opostos. O trecho de caminhada continua
uma adaptação de dez ticks; fechamento, hold e blend completos do PS1 ainda
precisam de paridade.

O relógio para nos três interiores, conforme os cabeçalhos e `place_entries`.
Mochila, mão, ferramentas, terreno e remessas permanecem na sessão. A visita
de Thomas não se repete ao sair. O carregamento segue as seções sazonais e
não usa endereços PS1 como ponteiros host.

## Regras de entrada

O recorte aceita os eventos de porta 0..3 dos edifícios iniciais da fazenda.
Eles não têm regras de horário em `event/event_rules.c`: a busca original
não encontra um bloqueio e permite `farm_zone_trigger`. Isso não permite
ignorar regras de outros estabelecimentos.

Portas da vila dependem de `event_script_lookup_eval`, presença de moradores,
afinidade, calendário e condições de história. Continuam fechadas até que
esses estados e regras sejam adaptados. A estufa e ampliações ainda não
construídas também não são liberadas por esta implementação.

## Atalhos e validação

```bash
# Estábulo
./build/native-debug/port/hmbtn_native --data data --area-preview 92
# Celeiro
./build/native-debug/port/hmbtn_native --data data --area-preview 96
# Galinheiro
./build/native-debug/port/hmbtn_native --data data --area-preview 112
```

Os atalhos criam uma sessão nova; não carregam saves nem concedem animais.
Para testar as portas, comece com `--farm-preview` e vá até cada edifício.

Os 27 CTests Linux incluem destinos sazonais, direção, altura e limites de
zona, modelos inválidos e rejeição de entradas da vila. O percurso opcional
testa os três edifícios em cada estação, espera antes de caminhar, controle
na chegada, renderização, pausa do relógio e retorno com estado preservado:

```bash
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_farm_interiors_playthrough data
```

Também foi revalidada a navegação externa, o passeio de Thomas, o relógio/sono
e a caixa de venda. Evidências locais ficam em `build/native-debug/interiors-*.log`
e `farm-interiors.png`, ignoradas pelo Git. A validação desta etapa usa somente
Linux local; o código continua compartilhado com Windows.

Depósitos e cochos já compartilham o estado da sessão, conforme
[FARM_FEED.md](FARM_FEED.md). A compra de forragem/ração já abastece os estoques
pelo fluxo de [FEED_SHOPS.md](FEED_SHOPS.md). O primeiro ciclo de galinhas adultas,
com compra/nome, caminhada, alimentação e ovos, está em [POULTRY.md](POULTRY.md).
Demais animais, reprodução, troca de montaria, melhorias dos edifícios,
caixas de venda internas e saves continuam pendentes. O bootstrap
`--diagnostics` não executa o jogo; os testes host não substituem o build PS1
e sua validação SHA-1.
