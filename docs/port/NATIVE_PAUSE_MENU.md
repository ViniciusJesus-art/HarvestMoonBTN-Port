# Menu nativo de pausa

Entrega adicional solicitada antes de retomar PF-06, em 2026-11-04.
Funciona no título, abertura e gameplay do runtime nativo Linux/Windows.
O bootstrap `--diagnostics` continua sem executar o jogo.

## Uso

**Esc** abre imediatamente o menu e pausa o jogo no ponto atual. **Continuar**
ou **Esc novamente**, inclusive nos submenus, retorna ao jogo. Relógio,
animações, eventos, chuva, física e sequenciador/áudio ficam suspensos.
O tempo passado no menu não é recuperado ao voltar.

- **Carregar jogo:** primeiro escolhe a fazenda, depois uma gravação daquela
  fazenda. As entradas mostram nome do personagem e fazenda, dinheiro,
  Power Berries, estação, dia e horário. O histórico mostra também a data
  da gravação e o ano do jogo, em ordem da mais recente para a mais antiga.
- **Configurações:** gráficos, idioma do menu, som e controles.
- **Sair do jogo:** encerra o aplicativo. Para registrar progresso, continua
  valendo a gravação pelo diário/cama do jogo.

Mouse seleciona e clica; roda do mouse, setas e Page Up/Down percorrem listas.
Enter ou o botão de confirmação escolhe; Backspace ou Circle volta um nível.
O controle usa D-pad, South para confirmar e East/Back para voltar. Esc é
reservado ao menu nativo; Start continua sendo o botão original do jogo.
Listas têm paginação por rolagem, sem um número fixo de saves por fazenda.

## Aviso de gravação

Ao concluir a gravação do histórico e do slot recente, o aplicativo mostra
**Jogo salvo** no canto inferior direito da imagem do jogo. O texto branco
suavizado tem sombra, entrada com deslocamento/fade e saída automática em
2,8 segundos. Não abre janela, não pede OK e não suspende gameplay ou áudio.
O idioma acompanha a preferência do menu. Carregar outra partida cancela
um aviso anterior; falhas de gravação continuam sendo informadas como erros.
O aviso usa tempo de apresentação, sem alterar o save ou a simulação.

## Configurações efetivas

| Categoria | Opções |
|---|---|
| Gráficos | Tela cheia, janela de 640×480 a 1600×1200, VSync, escala inteira ou ajuste proporcional, filtro pixel/nearest ou suave/linear. |
| Idioma do menu | Português e Inglês. Os diálogos do jogo mantêm o idioma dos recursos instalados; esta opção não traduz o disco. |
| Som | Volume geral 0–100%, incluindo música, efeitos e XA; zero silencia. Setas ajustam em passos de 5; o mouse seleciona o nível na barra. |
| Controles | Remapeamento das 14 entradas PS1 no teclado, nomes das teclas e restauração dos padrões. Teclas em conflito são trocadas; Esc/F11 ficam reservados. O mapeamento padrão de gamepad é mantido. |

Preferências são gravadas em `settings.ini`, no diretório de preferências
`hmbtn/HarvestMoonBTN` do usuário. Arquivo ausente usa os padrões; arquivo
inválido restaura padrões na memória e informa o diagnóstico no log.
Falhas ao aplicar uma configuração de vídeo restauram a seleção anterior;
falhas de gravação aparecem no menu. F11 também persiste a escolha de tela cheia.
A imagem do jogo continua em 320×240. O menu usa coordenadas independentes,
com altura lógica de 720 e largura proporcional à saída real da janela,
cobrindo a janela inteira sem alterar a proporção dos textos.
Não foram alteradas coordenadas, texturas ou regras do mundo para ampliar a UI.

## Histórico de fazendas

Cada gravação pelo aplicativo cria um snapshot em:

```text
<pasta do slot>/saves/<identidade da fazenda>/<instante>-<sequencia>.hms
```

O slot `farm.hms` continua representando a gravação mais recente para o fluxo
original de Continue/diário e para a linha de comando. `--save-file` e
`--load-save` continuam disponíveis; o histórico acompanha a pasta do slot
escolhido. Não é gravado dentro dos recursos extraídos do disco.

A identidade nativa de 128 bits distingue partidas com nomes iguais e
acompanha retomadas de snapshots antigos. Novas gravações não apagam as
entradas anteriores. O slot único anterior é preservado no histórico antes
da primeira substituição. Arquivos quebrados ou de formato não reconhecido
são ignorados na lista, com indicação da quantidade, sem ocultar os válidos.

Não há política nova de migração entre versões. O codec vigente recebe apenas
a seção opcional `IDNT`, contendo quatro palavras little-endian de 32 bits.
Snapshots vigentes anteriores sem essa seção também podem ser listados e
recebem identidade ao retomar/gravar. Para arquivar o slot anterior, o codec
valida o arquivo e altera somente a identidade/header/checksum, mantendo as
seções de gameplay byte a byte. Nenhum empréstimo de recursos é inventado
para tornar uma sessão desserializada artificialmente pronta.

O arquivo do histórico é publicado antes da atualização do slot recente,
usando a mesma escrita temporária/rename do save existente. Os nomes são
conferidos antes da publicação para não reutilizar entradas da mesma instância
mesmo com relógio do sistema repetido. Escritores simultâneos no mesmo slot e
durabilidade da entrada de diretório após perda de energia continuam fora do
contrato da camada existente.

## Integração

`core/app.c` interrompe o consumo do relógio de atualizações enquanto o menu
está aberto e limpa tempo/input pendentes nas transições. Eventos de foco e
conexão de controle continuam sendo recebidos. Confirmações do menu não são
reenviadas ao personagem. O dispositivo de áudio é pausado sem consumir a
fila e o mixer não avança; ganho e pausa sobrevivem às trocas de música.

`graphics/pause_menu.*` cuida da navegação e composição. O catálogo só é lido
ao escolher Carregar; abrir Esc não percorre arquivos. As fazendas são
indexadas após a leitura. O carregamento cria e valida um candidato antes de
substituir a cena atual; falhas mantêm o jogo pausado e mostram o erro.

`filesystem/save_library.*` organiza e lista os snapshots, reutilizando o codec
e a escrita atômica de `save_file`. `platform/preferences.*` persiste e aplica
as opções; `game_input` recebe as teclas configuradas. A UI usa o atlas antialiased incorporado descrito abaixo, sem novos recursos
do disco nem dependência de fontes instaladas no sistema.

Portas e demais alterações locais do usuário foram preservadas. `src/`,
`sdk/`, `disc/`, `tools/` e o fluxo PS1 permanecem intactos. O build host não
substitui a toolchain e verificação SHA-1 do executável original.

## Visual atual — 2026-11-04

Menu alinhado à esquerda, texto branco proporcional e gradiente quase preto
à esquerda que se torna transparente à direita. O mesmo tema vale para todos
os submenus. Foram retirados o painel central, cartões coloridos, descrições
das opções principais, branding e rodapé de instruções. Os dados de fazendas
e saves continuam nas próprias entradas. Apenas erros reais aparecem em linha.

Controles têm o nome da entrada à esquerda e a tecla atual à direita, em
cápsulas escuras arredondadas; a captura de uma nova tecla mostra reticências.
A lista exibe oito entradas por vez, com rolagem e restauração dos padrões.
Valores de gráficos/som também ficam numa coluna própria. Listas de saves
exibem cinco entradas por vez, mantendo os metadados relevantes. As áreas de
clique e o desenho usam o mesmo layout e transformação de coordenadas.

`graphics/native_font.*` desenha texto UTF-8 por avanços proporcionais e
cobertura suavizada. O atlas e as métricas em `native_font_atlas.inc` derivam
de Liberation Sans Regular sob SIL OFL 1.1, com licença distribuída nos pacotes.
A textura usa filtragem linear, independentemente do filtro escolhido para
o jogo, e é mantida entre aberturas do menu. O renderer lógico e seu modo
de composição são restaurados depois de desenhar a interface.

O atlas contém as letras latinas e acentos, pontuação e setas necessários à
interface. Para regenerá-lo, forneça a fonte original e Pillow (somente esta
operação precisa de Pillow):

```sh
python3 port/packaging/bake_menu_font.py LiberationSans-Regular.ttf port/graphics/native_font_atlas.inc
```

O SHA-256 da fonte de origem fica no cabeçalho gerado. Builds e executáveis
normais usam o atlas versionado: não precisam de FreeType, SDL_ttf, Pillow
ou fonte do sistema. A fonte não é um recurso proprietário do jogo.

## Validação da entrega inicial

`native_menu_test` cobre fazendas com nomes iguais, várias gravações, metadados,
carga histórica, isolamento de arquivo quebrado, importação do slot anterior,
persistência/validação de preferências, troca de teclas em conflito, controles
sem input residual, volume pelo mouse, ganho/pausa reais do dispositivo e desenho.

`pause_app_test` compila o loop real do aplicativo e observa suas fronteiras
públicas de atualização/carga. Eventos SDL abrem Esc, verificam ausência de
ticks durante a pausa, retomam, escolhem outra fazenda e uma gravação antiga,
conferem o dinheiro restaurado, alteram/persistem volume e encerram pela opção
Sair. Não existe um loop alternativo de pausa ou hook de teste na produção.

Configure/build Linux Debug e Release aprovados. Debug: **14 casos únicos**
selecionados, incluindo menu/aplicativo, save/arquivos/retomada/Continue,
sessão/relógio, input, título independente do desenho, eventos, Greg e
regressões de chuva/portas/coleta/descarte. Os casos afetados foram repetidos
após os ajustes; a checagem final avulsa do menu também passou.

Release: **quatro casos únicos** aprovados (menu, aplicativo, codec e input),
com repetição dos dois testes de UI. O driver de teclado passa a inicializar
explicitamente `repeat=false`: a inicialização apenas do membro inicial da
união SDL_Event não garantia esse campo nos eventos sintéticos em Release.
A importação do slot anterior também foi corrigida para trabalhar sobre o
arquivo validado, preservando o requisito de recursos reais das sessões.

Build Windows MinGW em contêiner local sem rede aprovado. O teste do loop real
no Wine passou com saída **0**, cobrindo pausa/retomada, duas fazendas, seleção
de snapshot antigo, volume persistido e saída pelo menu. Capturas locais do
histórico e menu em português foram inspecionadas. Nenhuma captura ou recurso
proprietário foi acrescentado ao Git.

**50 processos**, em contagem conservadora: **38 Debug**, **8 Release**,
**2** da tentativa de depuração impedida pelo ptrace do ambiente e **2**
contando Wine/target. Inclui CTest, wrappers, filhos, cinco execuções de testes
que falharam durante os ajustes e suas repetições. Teto de 50 respeitado;
a suíte inteira não foi executada. Avisos antigos de const nos testes de
mundo e avisos Mesa/DXGI/EDID no Wine não impediram a validação.

`git diff --check` passou; `runtime.c` ficou idêntico à cópia da retomada desta
entrega e os caminhos PS1 permaneceram intactos. [Status do roadmap](ROADMAP_STATUS.md).
Windows sob Wine não substitui teste de vídeo/áudio em hardware Windows físico.


## Validação da atualização visual

Configure/build Linux Debug e Release aprovados. Debug **4/4** selecionados
(menu, loop real do aplicativo, teclado e atalhos da janela); Release **2/2**
(menu e aplicativo). Dois percursos avulsos com os recursos instalados
capturaram todas as páginas sobre o jogo e conferiram ausência de ticks ao
renderizar os submenus. Capturas de início, gráficos, fazendas e controles
foram inspecionadas; nenhuma foi acrescentada ao Git.

O teste de menu agora confere avanços proporcionais, pixels de cobertura
suavizada, clique na coluna de teclas, remapeamento, rolagem de oito linhas e
restauração dos padrões, além dos casos existentes de histórico/áudio/saves.
O teste do aplicativo conserva a pausa/retomada, carga histórica, persistência
de volume e saída pelo menu. Os cliques foram adaptados às novas coordenadas.

Build Windows MinGW sem rede e os dois testes no Wine aprovados. A instalação
Linux em diretório temporário conferiu a presença de `MenuFont-OFL.txt`.
Foram **14 processos de teste**: dois avulsos, cinco Debug com CTest, três
Release com CTest e quatro contando Wine/targets. Não houve execução da suíte
inteira. `git diff --check` passou; `runtime.c`, portas e caminhos PS1 ficaram
inalterados nesta atualização. O runtime permanece parcial; o bootstrap não
executa o jogo, e Wine não substitui verificação em hardware Windows físico.

## Validação do aviso de gravação

Builds Linux Debug/Release e Windows MinGW aprovados. Testes selecionados
`native_save`, `native_pause_menu` e `native_pause_app` passaram; os dois
últimos foram repetidos após ajustar a âncora à imagem do jogo. Um percurso
avulso do loop real, sem limite de frames, solicitou a gravação e conferiu
a escrita efetiva, ausência de janela modal e mais 90 ticks sem confirmação.
Capturas com aviso e após a expiração foram produzidas; a captura com texto
foi inspecionada. O percurso foi repetido para conferir a posição em janela
com margens laterais. Foram **9 processos de teste**, incluindo CTest e
as duas execuções avulsas; nenhuma captura entrou no repositório.
