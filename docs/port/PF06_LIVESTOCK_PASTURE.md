# PF-06 — Pastagem, transporte do gado e sino

Recorte de 2026-10-29 da fila do [ROADMAP.md](ROADMAP.md). Adapta o C
extraído e confere as condições divergentes no BIN instalado. Usa a coleção,
inventário, loja, relógio, colisão, portas e apresentação já existentes.
As alterações locais nas animações de portas foram preservadas; `runtime.c`
e `area_doors.c` não receberam novas alterações nesta rodada.

## Comportamento entregue

Barley oferece **Bell, 500 G**, na última linha original. A descrição é
2:0x281 e a confirmação usa 2:0x84. Equipamento, mochila e caixa de ferramentas
participam da consulta de posse; a linha some quando o sino é adquirido.
Saldo e espaço são verificados antes de inserir e cobrar. O cursor conserva
o deslocamento original já corrigido; o percurso verifica seus pixels na
nova linha, além de renderizar a loja com os textos reais.

Square usa a linha ANI **21** e som **13** na mesma ação das ferramentas
animais. O efeito acontece ao terminar a animação, sem consumir o sino ou
stamina; a fadiga segue a função comum de horário/clima. Cada animal vivo
na mesma área atende com probabilidade `afeição/200`, usando o RNG existente.
O destino é a posição do jogador no momento da chamada. Não existe outro
registro para representar um seguidor. O cachorro ainda não está integrado.

Empurrar exige caminhar continuamente contra o animal, com as mãos livres:
51 atualizações de contato antes do comando, conforme `livestock_bump_count`.
O destino fica 800 unidades à frente do animal, na direção do jogador.
Soltar o movimento ou trocar alvo/direção/área reinicia a contagem. Corrida
continua seguindo a regra existente de ignorar colisão de atores. Movimentos
dirigidos usam o mesmo ator, piso, obstáculos do campo e colisão entre animais;
sondagem lateral permite contornar um obstáculo. Prenhez bovina conserva
velocidade 2; os demais usam 6. Movimentos espontâneos e cues integrais seguem
parciais, sem substituir a infraestrutura pelo walker PS1 inteiro.

A passagem usa **`farm_zone_rows[0]`**, não o retângulo mais largo de interação
do jogador. Fazenda `(953,3000)..(1453,3400)` → celeiro `(1534,0,-2000)`;
celeiro `(1200,-3000)..(2000,-2800)` → fazenda `(1240,0,2800)`.
A entrada externa pede o evento de porta **1** ao controlador compartilhado e
aguarda a abertura real. O volume do animal pode cruzar o plano dessa passagem
sem sondar além do fim do mapa; o centro continua dependendo de piso válido.
Uma chegada ocupada aguarda o animal anterior sair, evitando sobreposição.
Não há teleporte por tecla, remoção/recriação de atores ou entrada do jogador
junto com o gado. A mesma coleção é desenhada com os packs originais também
na fazenda; não há sprites substitutos ou instâncias duplicadas.

O observador horário da sessão atende o gado também fora da área visível.
Fora do inverno, acumula cinco atualizações elegíveis e consome **uma célula
de grama madura por animal**, uma vez ao dia. Busca primeiro o bloco 3×3 ao
redor, depois o campo inteiro. A célula volta ao estágio de rebrota 1, zera
os dias e marca `FIELD_TILE_SKIP_DAY`; não acrescenta feno ao silo. Sem grama
madura, permanece sem alimento e pode tentar de novo na hora seguinte.
A apresentação do campo é atualizada na mudança horária e no carregamento.

As condições do BIN foram conservadas, inclusive suas diferenças:

- Vacas: clima 0 permite pastar; qualquer outro tira 5 de afeição por hora.
- Ovelhas: clima 1 tira 5; os demais permitem a contagem e tentativa de pastar.
- Inverno: não executa esses efeitos de pastagem/clima. Contadores não são
  limitados ao horário diurno; o repouso fora de 06h..20h é uma regra distinta.
- A noite aplica +1 de afeição pela alimentação, -10 pela falta e +1 separado
  quando a contagem de pastagem alcançou cinco, então zera a contagem diária.
  Animais no campo não consomem os cochos do celeiro. Moradores do celeiro
  voltam às fileiras originais; animais no campo conservam a posição externa.
  Mães com maternidade reservada voltam ao celeiro, como `farm_items_apply_kind`.

## Referências C e BIN

| Fonte em `src/overlay/11_GF_FARM` | Adaptação |
|---|---|
| `people/player.c`: contato, `PLAYER_MODE_USE_ANIMAL_TOOL + ANIMAL_TOOL_BELL`; `farm_shared.h`: `farm_mover_update` | `port/game/livestock.c`, controles existentes em `exploration.c` |
| `animals/animals.c`: `farm_mode_check`, `farm_set_destination`, `farm_walker_try_exits`, `farm_items_apply_kind` | Comandos, passagem/espera e posicionamento na coleção atual |
| `farm_probe.c`: `farm_zone_rows` / lookup | Tabela extraída no configure por `generate_farm_tables.py`, sem duplicar suas coordenadas no domínio |
| `animals/livestock.c`: action timers, aproximação/velocidades e overnight | Atualização horária, afeição/alimentação, metas e repouso |
| `field/field.c`: `field_find_ready_grown`, `field_grown_cell_request` | `farm_field.c`: consumo e rebrota no campo compartilhado |
| `menu/shop_dialogues.c`: `shop_fodder_menu_setup`, `shop_barn_step` | `feed_shop.c/.h`, UI existente e label `bell` extraído de `text_bank.c` |
| Carregamento de animais da fazenda/celeiro | `graphics/scene_preview.c`: mesmos ANI/atlas nas duas áreas |

Foi lida a desmontagem MIPS do overlay instalado, base **0x800b7e20**:
`livestock_action_timer` em **0x800de9c8** compara localização com **1**,
clima com zero, contagem com cinco e retorno da busca com **1**.
A rotina ovina em **0x800e05a8** compara o clima com **1** e chama a busca no
ramo diferente de 1. A busca em **0x800d9108** aceita somente valor **0x36**,
estágio ≥3, procura local/global e retorna 1 após consumir a célula.
Isso resolve as comparações enganosas do C extraído com `PLACE_FARM`;
a área lógica correspondente no port é **0**, e a do celeiro é **96**.

A tabela de passagens foi encontrada no BIN em **0x80122d8c**, com os oito
campos de cada direção idênticos ao inicializador C. Endereços de referência
não são convertidos em ponteiros host. O SHA-1 do overlay instalado é
`fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`, diferente do build PS1 de referência
`664eee540550138d6e3717bb7e1593aeb8fe9d38`: o disco local é modificado e esta
conferência não substitui a validação SHA-1/toolchain PS1.

## Estado e limites

LSTK passa a guardar a contagem de pastagem e o destino de movimento, além de
aceitar a área externa no registro existente. Codec atual **v13**, sem dumps
ou ponteiros. Apenas as adaptações necessárias à funcionalidade foram feitas;
compatibilidade entre versões não foi tratada como objetivo nem proibição.
O contato antes do empurrão e poses de portas continuam transitórios.

Ainda faltam cues/vozes completos, pets/ajudantes/concursos e máquinas.
O sino atende o gado integrado; não declara o ciclo canino concluído.
O bootstrap diagnóstico continua sem executar o jogo; o runtime jogável
permanece parcial. Sem alterações em `src/`, SDK, disco, ferramentas ou fluxo
CMake PS1, e sem novos recursos proprietários no Git.

## Validação

- Linux Debug: configure/build aprovados; **18 casos distintos aprovados**
  nas seleções pertinentes. A seleção final de cinco passou **5/5, 48,31 s**,
  incluindo os percursos completos de pastagem, ferramentas, reprodução e
  funerais, com snapshots em aplicativos novos. Os outros 13 casos cobrem
  regras/cochos/loja/save/grama e fidelidade de portas, chuva e cursores.
- Linux Release: configure/build e percurso novo completo aprovados; capturas
  da loja, pasto antes/depois do consumo, chuva e retorno conferidas localmente.
- Windows MinGW Release: build aprovado com imagem local e sem rede.
  Execução Windows/Wine não repetida nesta rodada, respeitando o orçamento.
- **50 processos**, contando conservadoramente seis lançadores CTest,
  wrappers Python, executáveis, apps filhos, tentativas iniciais e o percurso
  Release. Suíte inteira não executada. MSVC/Windows real e SHA-1 PS1 pendentes.

Contagem auditável: sete processos nas tentativas iniciais; 16 na primeira
seleção de 15 casos (o wrapper novo parou antes de abrir seus dois snapshots);
quatro na repetição desse percurso; 16 na seleção final de cinco; um percurso
Release; seis lançadores CTest. Não executar mais testes nesta rodada.

Testes novos: `native_livestock_pasture` e
`native_livestock_pasture_playthrough`. O percurso compra sino e animais com
Barley, nomeia, empurra nos dois sentidos, aguarda a chegada ocupada, verifica
a abertura externa, contorna rocha, usa ANI do sino, consome grama pelo relógio,
aplica chuva, repousa à noite e retoma snapshots externos/internos em apps novos.
O teste de unidade cobre falta de grama, culturas não comestíveis, inverno,
condições específicas por espécie, cochos/afeição, maternidade, espaço/saldo
e resposta ao sino com afeição 0/200. Capturas locais ficam fora do Git.


Reprodução dirigida, usando apenas os recursos locais instalados:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(livestock_pasture|livestock_pasture_playthrough)$' --output-on-failure
```

O próximo recorte selecionado é mineração e aquisição de minérios. A compra
de máquinas depende de adamantita na rotina original da loja; essa aquisição
precisa estar jogável antes de liberar encomendas de máquinas.
