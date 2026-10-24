# Pausa, mochila e efeitos do campo

O porte continua parcial. Esta etapa integra os menus ao inventário real da
sessão, as animações de quebra e a liberação do som do regador. Os recursos
continuam sendo lidos de `data/`, sem copiar imagens, músicas ou discos para o Git.

## Controles

| Teclado | Controle PS1 | Ação |
|---|---|---|
| Enter | Start | Estado da fazenda; fecha a página/volta ao jogo |
| V | Triângulo | Mochila; dentro dela mostra a descrição do item selecionado |
| Setas | Direcional | Cursor da mochila ou páginas do estado da fazenda |
| Z | X/Cross | Marcar e transferir/trocar itens na mochila |
| X | O/Circle | Cancelar marcação ou fechar a mochila |
| E | R1 | Circular ferramentas do equipamento e da mochila |
| 2 | R2 | Guardar o item da mão na mochila, ou retirar um item com a mão vazia |
| Q / 1 | L1 / L2 | Botões separados; seus apitos/ações ainda não estão integrados |

Os gatilhos R2/L2 do controle SDL já são lidos separadamente. E/Q deixaram de
acionar dois botões ao mesmo tempo; a navegação do baú continua funcionando.
Ao segurar R1, esquerda/direita também permite trocar a seleção. A apresentação
rápida conserva o item inteiro, inclusive modificador e água do regador.

## Estado da fazenda

Start pausa movimento, animações do mundo e efeitos de quebra; a música continua.
O menu usa as cinco ANIs e o pack de texturas original de `14_Status.Bin`.
A abertura/saída tem os fades de 10 ticks de `status_pages.c`.

As páginas acessíveis no estado inicial são resumo, itens enviados, ajudantes,
contas e experiência das ferramentas. O resumo mostra os nomes cadastrados,
ouro, estoque de madeira/rações e os contadores da sessão. A experiência muda
ao usar ferramentas e a madeira acompanha os galhos cortados.
As páginas de galinhas/vacas/ovelhas são puladas quando a contagem é zero,
conforme `options_page_next/prev` do original.

Contagens de envios, contas e galinhas acompanham o estado real da sessão.
A página de galinhas mostra adultas/pintinhos nomeados, saúde, afeição,
data/idade, estação e prêmio, com sete linhas e rolagem. Animais carregados
continuam listados; incubação e nome pendente não entram na contagem do
resumo. Detalhes e testes em [POULTRY_STATUS.md](POULTRY_STATUS.md).
As páginas das demais espécies aguardam seus registros nativos. Abrir o menu
não concede animais, recursos nem contratação de ajudantes.
Rótulos estáticos vêm do banco C US da decompilação; mensagens/descrições vêm
dos dados importados, inclusive PT-BR no teste local.

## Mochila

Triângulo abre o layout original de `storage_swap.c`: duas mãos/equipamentos,
slots de ferramentas e slots de itens. A mochila inicial libera dois de cada;
o núcleo respeita também os níveis com quatro/oito, sem conceder upgrades.

Cross marca uma origem e depois move/troca com o destino. Repetir Cross na
mesma origem tenta equipar o item. Circle cancela a marcação; sem marcação,
fecha. Start fecha a mochila. Triângulo abre a descrição; ao fechar o texto,
retorna à mochila. O regador mostra sua quantidade atual de água.

As coordenadas e links direcionais são gerados da tabela original. Itens são
movidos com id/modificador/contagem, respeitando restrições de armazenamento
e limite de pilha de 99 sementes. A mochila usa o mesmo equipamento e slots
do baú; não existe uma cópia independente do inventário. R1 faz a compactação
e troca das ferramentas; R2 respeita o espaço disponível para guardar o item.
Os slots começam vazios; coleta e carregamento de objetos soltos do campo
estão ligados ao inventário, conforme [FARM_CARRY.md](FARM_CARRY.md).

O cache de imagens de itens é limitado a 18 entradas com substituição da menos
usada. As imagens são centralizadas pela geometria original dos recortes.

## Som e quebra

O efeito do regador é uma amostra ADPCM em loop. Agora cada início devolve um
identificador da voz; ao terminar a animação de regar, somente essa voz entra
na fase de release ADSR. Um identificador antigo não interrompe uma voz que
reutilizou o mesmo canal. Abrir pausa/mochila também libera esse efeito,
preservando a música e outros efeitos.

Pedra pequena, galho, madeira e mato usam o intervalo 1 de suas ANIs originais.
A fila tem 25 posições, com um quadro a cada três ticks, conforme
`field_item_requests.c`; remove o efeito no fim e congela durante a pausa.
As posições das peças 2x2 seguem a submissão original de cada canto.
Ferramentas básicas continuam recusando tocos e pedras grandes que exigem
melhoria. Não se altera esse requisito para demonstrar uma animação.

## Validação

A página de galinhas acrescenta o percurso opcional
`hmbtn_poultry_status_playthrough data`, com recursos originais e fixtures
de estados/lotação. O percurso econômico existente também verifica a página
após uma compra normal. São 33 CTests e treze percursos locais aprovados.
A evidência abaixo registra as validações anteriores dos outros recortes.

- Configure/build Linux e 23 CTests: liberação de loop, identificadores antigos,
  inventário sem perda/duplicação, pilhas, slots bloqueados, páginas/fades,
  fechamento de mensagens e limite/tempo dos efeitos.
- Probe local: páginas acessíveis, mochila, ajuda PT-BR, descrição do regador,
  transferência, troca rápida com imagem, mundo pausado e a amostra real do
  regador ficando silenciosa após release.
- Probe do campo: as cinco ferramentas, efeitos visíveis ao cortar/quebrar,
  reabastecimento, sobrecarga e ida/volta com conservação do terreno.
- Windows x64 compilado com MinGW; execução em Windows real/MSVC e comparação
  sistemática da apresentação com PS1 permanecem pendentes.

Logs locais: `build/native-debug/farm-menu-probe.log` e
`farm-corrections-probe.log`; capturas `farm-pause-*.bmp`, `farm-rucksack*.bmp`,
`farm-cycle-tool.bmp` e `farm-tool-*.bmp`. Não estão incluídos no Git.
