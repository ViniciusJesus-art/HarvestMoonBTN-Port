# PF-06 — Banho na fonte termal

Entrega de 2026-11-04. **Revisão posterior:** a porta dupla da cabine, a
colisão, os sons e a distância de saída foram corrigidos em
[PF06_WORLD_ANIMATIONS.md](PF06_WORLD_ANIMATIONS.md), junto às animações de água.
As contagens de testes desta página registram a entrega inicial; a revisão
acrescenta verificações das folhas, colisão, áudio e distância do retorno.

Entrega inicial de 2026-11-04. Entrada pela cabine, salto, duas poses de banho,
recuperação pelo relógio e saída automática voltam ao controle comum.
Reutiliza terreno, bancos de ação, ANI, áudio, clima e atributos existentes.
Runtime/PF-06 continuam parciais; o bootstrap `--diagnostics` não executa o jogo.

## Referência C/BIN

Os eventos **46/47** das zonas **6/7** da fonte não são os scripts 46/47 de
`12_event_s.bin`. São passagens internas com id `0xfffe` e flags `0x1000`,
no local **12**, áreas sazonais **72..75**:

| Evento | Direção | Caixa X/Z | Comportamento |
|---|---|---|---|
| 46 | 2 | `(-9208,4300)..(-8538,4780)` | Cross; mãos vazias; aproximação e salto |
| 47 | 0 | `(-9208,4300)..(-8550,4780)` | Retorno automático, sem novo Cross |

As caixas são lidas do recurso instalado, com limites estritos. Como em
`farm_zone_find`, o teste da zona usa X/Z. O estado livre, as ações, o salto,
as transições e voos pendentes são conferidos antes da entrada. Uma tentativa
com item na mão consome a interação sem entrar ou arremessar o item.

Referências principais:

- `farm_main.c:FARM_MODE_ZONE_WALK/ZONE_WALK_WAIT`: caminhadas 10/27 e latch
  de salto depois da passagem 46; passagem 47 automática na saída.
- `people/player.c:player_zone_action`: direção, Cross na entrada e recusa
  com item carregado; evento 47 dispensa Cross, inclusive no ar.
- `player_start_jump`: X **−8838**, direção 2, velocidade vertical **−140**,
  ANI 4 com frame da direção, som **6** e temporizador **20**.
- `player_run_action`: ao vencer o temporizador, zera a velocidade vertical,
  vira para 0, mantém o personagem parado/submerso, toca som **16** e usa a
  animação de entrada na água. Em seguida inicia o banco de banho.
- `player_mode_tick`: modos **0x58/0x57**, poses 0/1 por `one_minute*10`;
  ambas recuperam **+2 stamina/−1 fadiga** quando passa um minuto. Cross na
  pose 1 retorna à pose 0; novo Cross na pose 0 inicia a saída.
- Saída: modo **0x4b**, velocidade vertical **−250**, som **6**, salto ANI 4
  para baixo; a zona 47 assume a travessia da cabine. O término comum do
  lançamento tem temporizador 45 e caminhada 20 se não houver essa zona.

O `11_GF_FARM.bin` instalado tem base **0x800b7e20**, SHA-1
**fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6**. Conferidos words MIPS LE:

- **0x800c93d8/0x800c93f0**: X `−8838` e velocidade `−140` da entrada.
- **0x800c6bd8**: velocidade `−250` da saída.
- **0x800c6c54..0x800c6c64**: argumentos `2`, `−1`, `0`, `0` e chamada
  a `farm_clamp_status` na recuperação.
- `player_update_position`, **0x800caadc**: **0x800cab78** usa tabela em
  **0x80123100** (`hop_steps`, passo 51); **0x800cab94** seleciona o deslocamento
  `−0x30` na entrada (`move_steps_large`, **0x801230d0**, passo 73).
  Não se reproduz no host o cast C que atravessa os limites de arrays nomeados.

As ações continuam no `00_gf.bin`, base **0x03cb8800**, linhas de **0x3800**:

| Linha | Uso | Offset | Bytes de ANI antes do BANK |
|---|---|---|---|
| 0 | salto/entrada na água | `0x03cb8800` | 424 |
| 1 | poses de banho | `0x03cbc000` | 424 |
| 16 | salto de saída | `0x03cf0800` | 424 |

Os três bancos possuem cinco ranges reais. A pose parada é direcional;
a pose de descanso tem quadros de 60/18 unidades ANI e seu loop original.
`one_minute` vale 30, mas o relógio avança após `one_minute/2` atualizações:
as poses alternam a cada **300 ticks / 20 minutos do jogo**, não a cada dez
minutos. A recuperação consulta o evento de minuto, sem contador paralelo.

A geometria contém água **0x8040** em Y **−1812** e um polígono **0x8000**
submerso, por exemplo Y **−1692** no centro. O salto usa máscara **0xc000**;
o temporizador encerra a queda e mantém o corpo parcialmente submerso.
Esperar um pouso comum nessa água atravessaria os polígonos que o original
manda ignorar. O porte segue o término por tempo da entrada e usa terreno
real na aproximação e no salto de saída, sem criar um piso artificial.

## Integração

`hot_spring` guarda somente fases/temporizadores transitórios na sessão.
`exploration` consome os comandos durante a ação, carrega as linhas 0/1/16
pelo provedor atual e devolve o banco normal ao atravessar a saída. A cura
ocorre depois da atualização do relógio, com limites de stamina/fadiga.
Menus, transições e pausas existentes mantêm suas regras; Esc continua
suspendendo a simulação pelo host. Clima/partículas continuam atualizando.

`house_control` reserva essas passagens ao banho; `area_doors` aciona a porta
dupla real nos modelos 0/1 e a colisão acompanha seu estado. O bloco completo
de animação/transição de portas do usuário em `runtime.c` ficou byte a byte
igual ao início deste recorte. A única alteração nesse arquivo impede iniciar
uma visita durante o banho. Nenhuma curva, velocidade, recuo ou ângulo de
porta do usuário foi refeito.

A passagem das 6h segue o pernoite já existente; `daily` limpa a fase e o banco
de ação antes da manhã na casa. `save_available` recusa a ação em andamento,
como já faz com as demais ações. Antes/depois do banho, atributos e inventário
são persistidos pelos donos atuais. Codec **v22**, sem novo campo serializado
ou trabalho de compatibilidade de saves.

## Validação

`hot_spring_test` cobre mãos ocupadas, local/tamanho/direção/borda incorretos,
exclusão das falsas portas, bancos/sons/término por ANI, recuperação/limites,
alternância de poses, Cross, salto de saída, prioridade da passagem 47 sobre
o temporizador e save bloqueado/estável. Usa asserts também em Release.

`hot_spring_playthrough` usa mapa/terreno, ANI/BANK e entradas reais:

1. Colhe um bambu original na montanha, tenta entrar segurando-o, confirma
   a recusa sem descarte e guarda o bambu na mochila pelo controle comum.
2. Salva/retoma, entra pela zona original e confere salto, respingo, pose
   submersa; a revisão posterior verifica as folhas e seus giros espelhados.
3. Observa três minutos: stamina **30→36**, fadiga **20→17**. Espera a segunda
   pose, retorna com Cross, confirma que manter a tecla pressionada não sai,
   verifica os limites e sai com um novo Cross.
4. Confirma o personagem fora da cabine, em chão seco, com banco normal e
   controle livre; salva/retoma os atributos e o bambu guardado.
5. Repete a entrada à noite com chuva e confere que as partículas avançam.
   Passa das 5h59 às 6h pelo relógio e confirma manhã na casa, sem fase/banco
   de banho pendente; salva/retoma.
6. Abre uma sessão de inverno/neve e repete entrada, recuperação e saída no
   pack sazonal, sem conceder item ou alterar a geometria instalada.

Datas, posições, condições meteorológicas e atributos iniciais são fixtures.
Três checkpoints são comparados ao reabrir e carregados no aplicativo em
processos novos. Capturas locais de banho/descanso/saída/chuva foram produzidas;
banho e retorno foram inspecionados. Nenhum recurso proprietário foi incluído
no repositório.

Linux Debug **13/13** selecionados aprovados, incluindo relógio, recursos de
ação, save/sessão, exploração, ovos termais e regressões de portas/chuva.
Release **3/3** aprovados: unidade, percurso com três retomadas e codec.
Build Windows MinGW sem rede aprovado; unidade, percurso completo, três
retomadas em processos novos e regressão de portas no Wine aprovados.
A chamada inicial de build terminou com sinal 143 após a vinculação dos
executáveis; a confirmação incremental posterior retornou sucesso, sem
trabalho pendente. Os executáveis testados são os do build atualizado.

**46 processos de teste**: **26 Debug**, **8 Release** e **12 Wine/targets**,
incluindo CTest, wrappers, filhos e lançadores. A conta inclui cinco tentativas
iniciais do driver durante a adaptação/verificação da geometria. Não houve
execução da suíte inteira; teto de 50 respeitado.

Os ensaios iniciais identificaram a necessidade de terminar a entrada pelo
temporizador original e distinguir a superfície de água do primeiro polígono
submerso na verificação. Não se alterou a geometria para fazer o teste passar.
`src/`, `sdk/`, `disc/`, `tools/` e CMake PS1 preservados. A validação host não
substitui a toolchain/SHA-1 do PS1 nem testes em hardware Windows físico.

## Limites e próximo recorte

Este recorte trata o banho do jogador nos mapas instalados. Não implementa
banho de NPCs, eventos de romance, ofertas à Deusa nem toda a física/colisão
do jogador original. O movimento reaproveita a consulta nativa de terreno;
entrada e saída conservam as ações, velocidades e temporizadores auditados.
Próximo recorte: ofertas à Deusa, auditando zona 44, itens realmente obtidos,
roteiros/condições, consumo da oferta e recompensas no estado atual.


A regressão de acesso natural passou a iniciar 600 unidades antes da zona
46, caminhando/correndo até a porta antes de interagir. A colisão não soma
novamente as 48 unidades já usadas pela sondagem do controle; também
bloqueia o contato exato com o plano, para que a corrida não o atravesse.
Ver detalhes de renderização do oceano em `PF06_WORLD_ANIMATIONS.md`.
