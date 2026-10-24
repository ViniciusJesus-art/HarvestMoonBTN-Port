# Página de estado das galinhas

Com uma galinha na sessão, Start → direita → direita abre a lista de galinhas
do estado da fazenda. O caminho anteriormente selecionava uma página que
ainda retornava erro no renderizador, podendo encerrar o programa depois de
uma compra normal. A página agora lê os mesmos registros usados por criação,
loja, transporte e produção.

## Dados e apresentação

`graphics/farm_menu_ui.c` adapta `status_page_chickens`,
`status_draw_chicken_row`, `status_list_draw` e `status_draw_item_values`, de
`src/overlay/11_GF_FARM/menu/status_pages.c`. Usa o pack original
`14_Status.Bin`, sem imagens novas ou recursos proprietários no Git.

Cada linha apresenta número, ícone, nome de até oito glifos, afeição,
marca de prêmio, estação, dia e idade. Adultas usam a animação 15 do segundo
pack; o frame indica a doença. Pintinhos usam a animação 14, sem o frame de
doença. A afeição tem um coração por 25 pontos, até dez, nas posições originais.
O cabeçalho usa o ícone de adulta e `name_panel_title` de `text_bank.c`.

A formatação da data conserva o valor de dia armazenado, conforme
`status_draw_item_values`; o formatador semelhante da lista de venda soma
um ao dia. Essa distinção do C original é preservada. A comparação da
apresentação e dessas datas com uma execução PS1 permanece pendente.

O menu consulta os registros da sessão por referência host de leitura.
Ovos em incubação e pintinhos ainda aguardando nome não aparecem nem entram
na contagem do resumo. Continuam ocupando sua vaga para compra/incubação.
Animais carregados permanecem na lista. Registros livres são ignorados;
os números exibidos seguem a ordem dos registros válidos, sem buracos.

## Navegação e pausa

São sete linhas, em Y 48..192, espaçadas por 24 pixels. Com oito a dez animais,
cima/baixo move a primeira linha visível, com circulação: cima na posição
inicial mostra as últimas sete; baixo depois da última posição volta à primeira.
As setas pulsantes indicam conteúdo acima/abaixo.

Esquerda/direita troca de página e zera a rolagem; reabrir o menu também
começa no resumo com rolagem zero. Start, Cross ou Circle fecha usando os
fades existentes. Páginas de animais sem registros são puladas. O menu
conserva relógio, animais, alimentação, inventário e produção enquanto aberto.
Esta lista não vende, renomeia nem altera afeição.

## Testar

```bash
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/native-debug/port/hmbtn_poultry_status_playthrough data
```

Um terceiro argumento aceita uma pasta de capturas BMP fora do Git. Evidências
locais em `build/native-debug/poultry-status/`: adulta, sete linhas, rolagem,
últimas linhas, registros filtrados e retorno ao menu sem animais.

O percurso usa fixtures declaradas de uma a dez aves, incluindo pintinho,
adulta doente, prêmio, capacidade ampliada, nome de oito glifos e animal na mão.
As condições de prêmio/melhoria servem ao teste de interface e não são concedidas
ao gameplay. Verifica renderização, rolagem, filtros, pausa e fechamento/reabertura.
O percurso econômico `hmbtn_poultry_playthrough` também abre e renderiza a
página logo depois de comprar/nomear sua primeira galinha pelo fluxo normal.

Validação Linux: 33 CTests e treze percursos opcionais com dados locais
aprovados. As capturas da lista foram inspecionadas com SDL dummy/software.
Não houve nova janela real, build Windows ou comparação PS1 nesta etapa.

## Limites

Vacas e ovelhas ainda não têm registros nativos ou páginas funcionais; suas
contagens permanecem zero no gameplay e essas páginas são puladas. A página
de galinhas não completa os demais sistemas de animais, concursos, melhorias,
clima ou saves. O bootstrap `--diagnostics` não executa o jogo. Os testes host
não substituem o build PS1 e a verificação SHA-1 original.
