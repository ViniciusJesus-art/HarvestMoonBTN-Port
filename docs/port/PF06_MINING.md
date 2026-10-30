# PF-06 — Mineração e aquisição de minérios

Recorte de 2026-10-29 da fila do [ROADMAP.md](ROADMAP.md). Adapta
`src/overlay/11_GF_FARM/field/mine.c` e confere suas tabelas/condições no BIN
instalado. Reutiliza ferramentas/ANI, estado, inventário, relógio, mensagens,
transições, carregamento de mapas e remessa já existentes.

## Entrega

As entradas originais das áreas **68** e **72** alcançam as duas cadeias de
mapas: **234..243** e **245..254**, dez andares cada. Esses IDs são recursos
individuais, não grupos sazonais de quatro. `world_map.c` os reconhece sem
arredondar nem somar a estação. As áreas externas conservam seus recursos,
piso e acessos sazonais; não foi adicionada uma trava de calendário à entrada.
O anexo **244**, ligado ao fundo da primeira cadeia, conserva o estado do
andar ao ir e voltar. Seu conteúdo interativo restante não está concluído.

Cada andar usa seu mapa original; não há labirinto sintético ou rochas geradas
para simular outro jogo. A grade 10×10 é vinculada às primitivas reais com bit
`FARM_QUAD_CELL_BIT`. Células não vinculadas não podem ser escavadas.
A enxada inicial usa a ação/ANI existentes e atinge a célula 400 unidades à
frente. A regra de loot conferida em `mine_dig` é a **escavação com enxada**;
este módulo/mapas não geram uma coleção de rochas quebráveis pelo martelo.
Não foi inventado esse mecanismo como substituto da rotina encontrada.
Melhorias/áreas de efeito das ferramentas continuam fora deste recorte.

Bordas são marcadas sem sorteio. A escada de retorno do andar anterior não
pode ser escavada e a região de raio dois ao redor fica sem novos achados ou
saídas. Nas demais células, a chance da saída começa em **10/100**, cresce
**+1** quando nada é encontrado ou **+2** quando há item; existe uma saída por
andar, e o último não pode descer para um índice inexistente. As tabelas
ponderadas por andar são extraídas do C, incluindo seus valores incomuns;
não são normalizadas. O RNG usa a aritmética PS1 existente, em estado da sessão.

`farm_action.c` conserva o mesmo ciclo de carga/execução, custo de energia,
fadiga e experiência: 150 quando um item é encontrado, 40 nos demais golpes.
`event_item` apresenta minério/sucata/receita com o ANI e a figura originais;
a mão e a mochila existentes recebem o item. A moeda passa pela pose de
guardar e credita **10 G uma vez**. Power Berry usa a pose de consumo e a regra
já usada pela pesca: máximo de energia +10, limitado a 255, recuperação e
contador de berries. Os flags originais impedem repetir berries/receita.
Não há minério concedido fora da escavação para antecipar as máquinas.

Pisar na saída abre a pergunta original e o painel Yes/No. Cancelar recua o
jogador por dez atualizações, evitando reabrir a pergunta sem sair da célula.
Confirmar desce pelo carregador/transição atual; a escada do andar anterior é
posicionada com o modelo original. A confirmação da **escada de retorno sai
da mina inteira**, conforme `mine_update_transition`; não sobe apenas um andar.
Os pontos externos originais são preservados e assentados no piso pelo runtime.
O relógio permanece parado nos locais subterrâneos conforme sua tabela atual.

## Estado e adaptação

`HmbtnMine`, pertencente à sessão, guarda espécie da mina, andar, chance,
escadas, células e RNG. Células nativas usam uma matriz coerente por linha;
a indexação transposta da estrutura PS1 não é copiada para a memória host.
O mesmo estado alimenta regra e retexturização. Não há uma grade de loot,
inventário ou loop de viagem paralelo.

Vínculos de primitivas são offsets nativos nos buffers validados e são refeitos
no carregamento. Não são ponteiros PS1 convertidos para ponteiros host. O
renderer reserva também a geometria da escada inicialmente oculta antes de
revelá-la nos andares inferiores. `mine_ui.c` apenas desenha o painel; não
avança RNG, células, mensagens ou inventário.

O chunk existente de campo grava o estado necessário da mina, codec atual
**v14**. Pergunta, recuo e recebimento de recompensa são transitórios e mantêm
a proteção existente contra salvar uma ação em andamento. Estado subterrâneo
estável pode ser retomado, inclusive com figuras/células/modelo da escada
restaurados. Sair limpa a exploração corrente e conserva os achados no estado
compartilhado. O save foi adaptado conforme a funcionalidade; compatibilidade
entre versões não foi trabalhada como objetivo nem proibição.

## Referências C/BIN

| Fonte original | Adaptação em `port/` |
|---|---|
| `field/mine.c/.h`: probe, coordenadas, dig, pesos/eventos, textura, perguntas, próximo andar/retorno | `game/mine.c/.h`; tabelas extraídas por `cmake/generate_farm_tables.py` |
| `people/player.c`: enxada, XP e apresentação/recebimento; `world/scene.c`: mapas/posições | `game/farm_action.c/.h`, `game/event_item.c`, `game/exploration.c`, entrada no controlador atual |
| `farm_main.c`: limpar/vincular a grade nos carregamentos | `game/session.c/.h`, gancho `session_area_ready` e `graphics/scene_preview.c` |
| `menu/shop_dialogues.c`: `shop_result_update`; `text_bank.c`: Yes/No | `graphics/mine_ui.c/.h`, mensagem/UI compartilhadas |
| Estado `FarmClearData` | Codec de campos existente em `game/save.c`, índices/valores validados |

Os caminhos de referência são relativos a `src/overlay/11_GF_FARM`, exceto os
arquivos de texto e estado em `src/main`. Nenhum módulo original foi editado.
O gancho novo no fim de `runtime.c` prepara a mina na troca de área; os blocos
de animação de portas do usuário e `area_doors.c` foram preservados nesta rodada.

Conferência por leitura do overlay instalado, base **0x800b7e20**:

- `mine_dig`: **0x80113310**; `mine_update_transition`: **0x80113818**;
  `mine_next_layer_check`: **0x80113b80**.
- Sorteio de itens em **0x80113a20..0x80113b7c**: o lugar **0x37** seleciona a
  tabela alternativa. Os eventos de cada tabela não foram trocados com base
  em nomes inferidos para as minas.
- Pesos básicos em **0x80127fe8**, alternativos em **0x80128088**;
  eventos em **0x80128128** e **0x80128138**: todos os valores conferidos.
  A linha alternativa final começa com **100** para o evento vazio e foi
  conservada. Adamantita é **0x62**; sucata é **0x138**, não uma pedra do campo.
- Flags de achado único **0x60/0x61/0x62**, visitas **0x63/0x64**;
  tabelas/posições/modelos também confrontados com as seções de `00_gf.bin`.

SHA-1 do overlay instalado: `fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`;
referência do build PS1: `664eee540550138d6e3717bb7e1593aeb8fe9d38`.
O disco local possui modificações; esta conferência não declara equivalência
ao disco de referência nem substitui o build/toolchain e a validação SHA-1 PS1.

## Validação

- Linux Debug: configure/build aprovados e **15 casos distintos aprovados**
  nas seleções dirigidas. Seleção inicial: 11/12; expectativa antiga que
  recusava a área 234 foi atualizada. Seleção final: **4/4, 47,21 s**, com
  portas e percursos de pastagem, reprodução e funerais preservados.
- `native_mine`: vetores exatos de RNG/itens, coordenadas/bordas/raio da escada,
  achados únicos, último andar, energia/XP/inventário, codec e limpeza.
- `native_mine_playthrough`: duas entradas/dez andares reais, ANI/texturas e
  figura de cada tipo encontrado, Yes/No/cancelamento, modelo de retorno,
  anexo 244, relógio parado, moeda/berry, adamantita por escavação, mochila,
  remessa e quatro snapshots em processos novos do aplicativo.
- Linux Release: unidade e ambos os percursos diretos aprovados. Capturas
  de chão escavado, escada, pergunta, adamantita e fundo conferidas localmente.
- Windows MinGW: build aprovado com imagem local sem rede. No Wine passaram
  unidade, percurso da primeira cadeia e retomadas subterrânea/concluída em
  dois processos novos do aplicativo. MSVC e Windows real não executados.
- **48 processos**, incluindo dois lançadores CTest, wrappers Python/apps
  filhos, Wine/executáveis e tentativas iniciais. Suíte inteira não executada.

Contagem: quatro processos iniciais; 19 na primeira seleção de 12 casos
(inclui wrapper novo com sete processos e o CTest); 14 na seleção final de
quatro (inclui três wrappers de quatro processos e CTest); três no Release;
oito no Wine, contando quatro lançadores e quatro executáveis. Total 48.
Não ampliar testes sem nova mudança ou preocupação que justifique o custo.

Reprodução dirigida com os recursos locais instalados:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(mine|mine_playthrough)$' --output-on-failure
```

## Continuidade e limites

A próxima entrega é a encomenda/entrega/uso das máquinas de Saibara, agora
com aquisição real de adamantita. Deve respeitar material, dinheiro,
ampliações/estado das instalações, prazo e conversão dos produtos existentes;
não assumir que a presença do catálogo equivale à aquisição funcional.
Outros conteúdos das minas, ações/ferramentas completas, festivais e o jogo
integral continuam parciais. O bootstrap diagnóstico continua sem executar
o jogo. Sem alterações em `src/`, `sdk/`, `disc/`, `tools/` ou fluxo CMake PS1;
nenhum recurso proprietário foi adicionado ao Git.
