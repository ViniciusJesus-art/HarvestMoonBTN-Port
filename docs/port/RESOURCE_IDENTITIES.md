# Identidade e leitura de recursos originais

O porte prioriza os arquivos `.bin` extraídos do jogo. Gameplay e interface
solicitam os ids originais dos recursos; a localização física pertence ao leitor
ou à tabela de origem, e a interpretação permanece nos decodificadores atuais.
Diálogos conservam os glifos/controles da edição instalada. Marcadores de nomes
continuam sendo expandidos em tempo de execução conforme o sistema original.

## Leitura já utilizada

`port/filesystem/resource_file.*` continua responsável por ler intervalos exatos,
com limites e caminhos relativos validados. `graphics/asset_decode.*` conserva
a decodificação de ANI, bancos gráficos, paletas e composição em atlas CPU.
`hmbtn_ui_message_record` e `hmbtn_ui_message_portrait` continuam aplicando os
bancos ao atlas usado pela interface; não há outro caminho de renderização.

`port/filesystem/text_resources.*` centraliza duas famílias existentes:

| Família | Identidade usada pelo chamador | Fonte BIN atual |
|---|---|---|
| Mensagem | banco original 0..4 + número do registro | `02_messege0.bin` até `06_messege4.bin` |
| Retrato | número do banco de rosto original | `01_face.bin` |

A resolução de mensagens usa o mapa de `menu_result_file` em `menu_window.c` e
registros de 8.192 bytes. Retratos usam o passo de 6.144 bytes de `text.c`.
Somente esse adaptador calcula o intervalo dessas solicitações. O adaptador
valida família, banco, tamanho do destino e intervalo de 32 bits antes de
reutilizar `hmbtn_resource_read`; não altera bytes nem decodifica imagens.
Os novos diálogos e a loja usam essa resolução, assim como a troca de retratos
na interface compartilhada. A interface anterior de mensagem por arquivo
permanece disponível para os chamadores existentes.

A identidade é o par/trio de valores, não o endereço em `00_gf.bin`, a posição
transitória no atlas ou um ponteiro host. As chaves de cache de retratos continuam
usando o banco original. A seleção de arte de moradores continua usando os packs
e ids de personagens originais através de `village_art` e das tabelas extraídas
no configure; não foi duplicada uma tabela de offsets em novos carregadores.

## Diretriz para os próximos carregadores

1. Procurar primeiro os leitores e interfaces de animação, atlas e mensagem já
   existentes. Reutilizá-los ao suportar uma nova família de recursos.
2. Identificar a família, o id/índice original e sua tabela de origem. Quando a
   tabela está no código PS1, gerar a visão nativa durante o configure; quando
   está serializada, validar e ler seus campos little-endian de 32 bits.
3. Centralizar a resolução do intervalo nessa família. Gameplay/renderização não
   devem ganhar cópias de offsets absolutos nem depender da posição no arquivo.
4. Conservar os decodificadores e a leitura BIN funcional. Uma fonte alternativa
   deve atender à mesma identidade e preservar o comportamento comprovado.
5. Testar com fixtures sintéticas e, quando disponíveis localmente, comparar com
   os BINs originais. Não adicionar recursos proprietários ao Git.

## Futuro uso de PNGs extraídos

Não há carregador nem exportador de PNG de recursos do jogo implementado nesta
etapa. A identificação acima permite relacionar uma extração futura aos mesmos
ids solicitados pelo porte. Um manifesto de extração poderá associar, por
exemplo, um banco de retrato ao PNG e aos seus metadados; isso não exige mudar o
id usado pela conversa ou duplicar a posição do recurso no BIN.

O PNG sozinho não representa toda a informação PS1. A extração deverá conservar
recortes/partes, âncoras, espelhamentos, ordem de composição, duração e saltos de
animação, variante de paleta e semitransparência. O atlas atual usa palavras e
paletas originais; colar RGBA nesse atlas sem preservar essas relações alteraria
o resultado. As ANIs e os dados de cenário/gameplay continuam sendo necessários.
A origem/edição deve acompanhar o manifesto de integridade existente, evitando
misturar imagens de uma cópia com textos ou paletas de outra.

A comparação BIN/PNG deverá verificar composição, cores, transparência e
animação antes de habilitar a fonte PNG. Recursos extraídos e imagens de teste
ficam fora do Git. Essa preparação não constitui suporte PNG atual nem paridade
completa do jogo. `--diagnostics` executa somente o bootstrap.
