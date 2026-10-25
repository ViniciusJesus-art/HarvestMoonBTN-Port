# Roadmap — concluídos

Registro consolidado do trabalho marcado como concluído. A conclusão de uma tarefa não significa paridade total com o PS1 nem que o jogo completo já esteja portado.

Fonte: [ROADMAP_HISTORICO.md](ROADMAP_HISTORICO.md). Os itens foram extraídos do checklist original, preservando as quebras de linha.

## Situação inicial

- [x] Build PS1 preservado: executável e 13 overlays conferidos com SHA-1.
- [x] Build C11 nativo separado em `build/native-*` e testes de little-endian.
- [x] Importação única do disco para `data/`, com integridade e manifesto.
- [x] Reconstrução PS1 usando somente a pasta extraída.
- [x] CI configurada para Linux/Windows em Debug e Release.

## M1 — Aplicação nativa com janela (primeira implementação)

- [x] Integrar SDL3 ao build host, com versão de download fixada e hash.
- [x] Criar janela redimensionável com apresentação lógica de 320 × 240.
- [x] Implementar apresentação de frames, eventos e temporização sem espera ocupada.
- [x] Fechar por Escape/botão da janela e alternar tela cheia com F11.
- [x] Mostrar diagnóstico explícito: esta etapa ainda não executa o jogo.
- [x] Manter diagnóstico de console e execução finita para testes automáticos.
- [x] Validar build, testes sem display e uma janela real no Linux.
- [x] Preparar a CI Windows com a mesma dependência.

## M2 — Recursos reais na janela

- [x] Implementar leitura nativa de intervalos de bytes dos membros em `data/A_FILE/`.
- [x] Validar limites/offsets e identificar ANI/bancos por seus cabeçalhos e layout.
- [x] Adaptar a decodificação dos bancos `0x0D` de `texture_pack.c`, sem chamadas GPU PS1.
- [x] Decodificar paletas/cores e composição de partes/frames de `anim.c`.
- [x] Exibir imagem e animação reais da tela de título na janela PC.
- [x] Testar dados truncados e compressão com amostras sintéticas, sem assets no Git.

## M3 — Tela de título executando código C adaptado

- [x] Adaptar logo Natsume e entrada gradual de título/copyright/texto antes das opções.
- [x] Adaptar as duas camadas móveis de grama, personagens e flores.
- [x] Reproduzir música SEQ e efeitos VAB/ADPCM do título em SDL3.
- [x] Remover barras/textos de diagnóstico da área do jogo.
- [x] Portar apresentação do prefeito, pergunta pelo nome, fonte e retrato originais.
- [x] Portar grade e confirmação do nome do jogador, com limite de oito glifos.
- [x] Validar janela 1280×720 mantendo conteúdo 4:3 e capturas com margens.
- [x] Portar aniversário, nome da fazenda, nome do cachorro e resumo do cadastro.
- [x] Transferir o cadastro e iniciar a chegada à fazenda/evento `0x74`.
- [x] Adaptar espera por Start, seleção/confirmar, cursor e fades do menu.
- [x] Mapear teclado e gamepad para os bits PS1, com hotplug e teste virtual.
- [x] Registrar no terminal os limites antes de cadastro, saves, demo e fazenda.
- [x] Gerar `.exe` Windows x64 e verificar via Wine, sem confundir com Windows real.
- [x] Mapear teclado/gamepad aos botões exigidos pelo recorte do título.

## M4 — Primeiro trecho jogável

- [x] Decodificar o TMD prelit da área 0 com offsets/índices validados.
- [x] Implementar oito primitivas, texturas afins, cores e ordenação desse modelo.
- [x] Exibir o modelo base da fazenda em `--scene-preview`, com câmera de diagnóstico.
- [x] Projetar sprites do jogador/Thomas com ordenação junto aos polígonos.
- [x] Executar a chegada: caminhada, poses, diálogos, fades e música do evento `0x74`.
- [x] Renderizar modelos adicionais: portas, caixa, objetos com rotação e visibilidade inicial.
- [x] Desenhar o campo inicial com mato, pedras, tocos e galhos dos layouts originais.
- [x] Carregar a área 230 e portar a lembrança da infância/evento `0x75`.
- [x] Exibir as dez imagens do evento `0x76`, com tempos e fades do script.
- [x] Carregar áreas 231/232 e portar encontro e canção/evento `0x77`.
- [x] Ler/decodificar XA em fluxo para as faixas 4/5, sem disco e sem processo externo.
- [x] Portar despedida `0x78` e retorno a Thomas `0x79`, com música 34 e falas.
- [x] Usar o banco de ação correto para a confirmação com a cabeça e restaurar o sprite básico.
- [x] Carregar o interior da casa/área 80 e encerrar o roteiro da abertura.
- [x] Integrar o término da abertura ao controle na casa, com chão e colisões estáticas originais.
- [x] Integrar a transição inicial casa↔fazenda e interação com a porta externa.
- [x] Carregar um cenário, posicionar câmera e desenhar chão/objetos/personagem.
- [x] Integrar movimento, animação, colisões estáticas e uma transição de porta.

## M5 — Sistemas do jogo

- [x] Relógio/calendário, ciclo visual dia/noite e transições de dia no recorte casa/fazenda.

## M6 — Instalação e distribuição

- [x] Localizar a pasta de dados instalada sem depender do diretório atual.

### Música e interações da casa inicial

- [x] Tema de primavera e passos com os bancos/parâmetros originais.
- [x] TV: quatro canais, programas por data e animação original no aparelho.
- [x] Calendário: navegação e mensagens dos eventos.
- [x] Baú: tampa, interface e cinco ferramentas; equipar, mover, trocar,
  organizar e consultar descrições, conservando os itens na sessão.
- [x] Livro/estante com texto e rolagem contínua original.
- [x] Mesa de opções: manual, tipos de controle, vibração SDL, estéreo/mono e relógio.
- [x] Menu da cama e sequência original de deitar, câmera, coberta e fades.
- [x] Transição inicial casa↔fazenda e leitura das placas externas.

### Distribuição do porte — instaladores

- [x] Extrator nativo em fluxo e preparação gráfica com seleção/arraste do `.bin`.
- [x] Importar somente recursos, sem executável de PS1, overlays MIPS ou disco agregado.
- [x] Dados por usuário, manifesto SHA-256, transação e cancelamento sem pasta parcial.
- [x] Pacotes AppImage, DEB, RPM e EXE; ícone e atalhos; sem Python/SDK/emulador no usuário final.
- [x] Workflow de artifacts Linux/Windows sem dados proprietários, com builds e CTest.

### Primeiro controle fora da casa

- [x] Zona de saída da casa carrega a fazenda/área 0 na entrada original.
- [x] Controle, chão estático, música e estado do inventário preservados.
- [x] Retorno inicial pela porta externa, com giro e som originais; temporização completa ainda pendente.

### Correções das placas, espera da porta e volume

- [x] Banco 2 para placas/estante; TV e menus continuam usando o banco 3.
- [x] Nome do cachorro cadastrado e estoques da sessão nos textos das placas.
- [x] Respeitar direção livre nas placas cujas zonas usam `FACING_ANY`.
- [x] Jogador parado durante a abertura da porta; caminhar somente após abrir.
- [x] Música com volume 80 dentro e 100 fora, inclusive no tick da troca de área.
- [x] 21 CTests Linux, probe com as cinco placas/estante/porta e build Windows x64.

### Fazenda — conclusão funcional solicitada

- [x] Fechamento de diálogos em um único ciclo, sem reexibir a moldura.
- [x] Validar a proporção real de amplitude da música: casa 80% do exterior.
- [x] Consultas originais de chão, retângulos FarmChoice e regras dos ocupantes do campo.
- [x] Bloqueio/deslizamento e salto conforme o tipo do obstáculo.
- [x] Estado mutável dos 1125 terrenos, conservado nas entradas/saídas.
- [x] Cinco ferramentas iniciais: ANI/sons, efeito após a animação, arar, regar e limpar.
- [x] Abastecer o regador com 100 unidades e mostrar a contagem real na descrição.
- [x] Stamina/fadiga/experiência na sessão e animação original por sobrecarga.
- [x] Efeitos de quebra originais no campo (fila limitada e pausa).
- [x] Pegar/carregar/lançar objetos soltos, guardar itens e caixa da fazenda.

### Menus da fazenda, mochila e áudio de ferramentas

- [x] Regador libera sua própria voz no fim da ação e ao abrir menus.
- [x] Identificadores de áudio impedem que um release antigo interrompa outra voz.
- [x] Animações de quebra para pedras, galhos, madeira e mato; fila de 25 efeitos.
- [x] Start: resumo, envios, ajudantes, contas e experiência, com o pack original.
- [x] Pausa congela o mundo e efeitos; música continua.
- [x] Triângulo: mochila com os slots desbloqueados, ajuda e descrições originais.
- [x] Mover/trocar/equipar e empilhar sementes, preservando o inventário do baú.
- [x] R1 troca ferramentas; R2 guarda/retira itens; teclado com R1/R2 separados.
- [x] Imagens de itens com cache limitado e centralização pelos recortes originais.
- [x] Coleta de objetos soltos, carregamento, lançamento e caixa da fazenda.

### Coleta, mochila e descarte

- [x] Recolher pedra pequena, galho e mato do campo, removendo imagem/colisão.
- [x] Carregar com as ANIs originais de coleta, repouso, caminhada e corrida.
- [x] Guardar/retirar pela mochila compartilhada, respeitando capacidade.
- [x] Lançar e descartar objetos soltos, com animação, som e três slots de voo.
- [x] Bloquear comandos durante a ação e congelar o voo nos menus.
- [x] 24 CTests e probe de coleta → mochila → retirada → lançamento.
- [x] Consulta real do chão no voo e entrega à caixa da fazenda.

### Ajustes de carregamento e primeiro encontro matinal

- [x] Deslocamentos originais de itens carregados por direção e corrida.
- [x] Pedra/galho/madeira com classe de campo retornam a célula livre no plantio.
- [x] Preservar solo, ocupantes e colisão; mato segue o descarte.
- [x] Primeiro trecho do evento 12: Thomas, textos, nome e escolha Sim/Não.
- [x] Gatilho uma vez por sessão.
- [x] Continuar o passeio aceito a partir do cursor 15 e Zack a partir do 147.

### Passeio de Thomas e apresentação de Zack — 2026-10-23

- [x] Passeio aceito completo: estrada 32, áreas 36/40/48/52 e retorno à fazenda.
- [x] Geometria/texturas reais, caminhadas, poses, Ellen e mensagens 64..77.
- [x] Recusa com falas 78/79 e continuação comum com Zack.
- [x] Subrotinas do roteiro sem modificar os bytes importados; retorno validado.
- [x] Trocas de retrato na mesma mensagem e esperas pelos bancos 105/159.
- [x] Apresentação de Zack, câmera da caixa, marcador e saída original.
- [x] Controle liberado no término do evento; mochila/terreno conservados.
- [x] Acréscimo das duas horas do passeio ao horário da sessão.
- [x] 24 CTests Linux, percurso Sim/Não com dados locais, janela X11/software e cross-build MinGW x64.
- [x] Exploração livre das regiões externas da cidade.
- [x] Entrega à caixa da fazenda e pagamento; produtores e demais caixas ainda pendentes.

### Aproximação de Thomas, escadas e relógio — 2026-10-23

- [x] Aproximação de Thomas pela entrada norte antes da primeira conversa.
- [x] Jogador/atores assentados no chão durante os eventos, incluindo as escadas da estrada 32.
- [x] Relógio de 15 ticks por minuto; pausa em interiores, menus, eventos e fades.
- [x] Virada da data às 06:00, semana, 30 dias/estação e limite de ano original.
- [x] Amanhecer/dia/entardecer/noite com tabelas originais, blend subtrativo e interpolação de 256 ticks.
- [x] Faixas sazonais e noturna, proporção de volume e silêncio no inverno à noite.
- [x] Hora/minutos, calendário e TV atualizados pelo mesmo estado de tempo.
- [x] Sono até a manhã, recuperação de stamina/fadiga e retorno ao controle.
- [x] Seções sazonais da casa/fazenda, solo de inverno e uploads de textura persistentes.
- [x] Secagem do terreno, mato de inverno e conservação de mochila/experiência/descartes.
- [x] 25 CTests, dois percursos com dados locais, janela X11/software e compilação MinGW x64.

### Direção na conversa e caixa de venda — 2026-10-23

- [x] Jogador vira para Thomas ao terminar o deslocamento da porta e permanece assim na primeira fala.
- [x] Altura dos objetos lançados consultada no terreno real.
- [x] Entrega reconhecida pelo bit 0x80 da superfície original, nas quatro estações.
- [x] Preços originais, 25 grupos de produtos, limite de 999 e total de peixes enviados separado.
- [x] Valor pendente, pagamento único às 17h58, datas sem coleta e entregas após o horário.
- [x] Saldo/receitas no estado compartilhado do menu; limite de carteira original.
- [x] Eventos de economia em ordem durante sono; histórico de contas às 05h59 do último dia da estação.
- [x] 26 CTests Linux, três percursos com dados locais, passeio X11 e cross-build MinGW x64.

### Regiões externas do mapa — 2026-10-23

- [x] Fazenda e 14 outras regiões externas conectadas por zonas originais.
- [x] 34 passagens direcionais, fade e retorno ao controle na chegada.
- [x] Resolução das quatro seções sazonais de cada região.
- [x] Caminhada/corrida no terreno real; altura de chegada, escadas e encostas.
- [x] Limites originais da câmera em exploração livre.
- [x] Leitura das placas do vilarejo/trilhas, incluindo nome cadastrado da fazenda.
- [x] Música regional, exceções de volume e pitch wheel real das SEQs da praia.
- [x] Inventário, terreno e remessas conservados; retorno sem repetir Thomas.
- [x] 27 CTests e quatro percursos de integração com dados locais no Linux.

### Interiores iniciais da fazenda — 2026-10-23

- [x] Estábulo 92, celeiro 96 e galinheiro 112, nas quatro estações.
- [x] Entrada por Z/Cross, direção e altura da zona; saída interna ao caminhar.
- [x] Giro por direção, rotação inicial e suporte a modelos de portas duplas.
- [x] Relógio pausado nos interiores e estado conservado no retorno.
- [x] Atalhos de teste `--area-preview 92/96/112`.
- [x] 27 CTests e cinco percursos de integração revalidados no Linux local.
- [x] Portas da vila: regras de presença, afinidade, calendário e horário.

### Interiores do vilarejo e infraestrutura de alimentação — 2026-10-24

- [x] 22 interiores de base 128..212 e suas quatro variantes sazonais.
- [x] Portas, escadas e passagens; recusa com o texto original do banco 2.
- [x] 31 tabelas originais de acesso e agendas para determinar presença.
- [x] Sprites dos 35 packs de moradores e rotas com retomada pelo relógio.
- [x] Atalhos de teste para os 22 interiores.
- [x] Depósitos e cochos ligados ao estoque/mão, animações e sprites originais.
- [x] Limpeza diária dos cochos, inclusive no avanço durante o sono.
- [x] Compra de forragem/ração pelo gameplay normal; demais lojas pendentes.

### Compra de alimentos da fazenda — 2026-10-24

- [x] Forragem de Barley e ração de Lillia, com presença e expediente dos scripts originais.
- [x] Saudação, seleção, descrição, quantidade, compra/recusa, retorno e cancelamento.
- [x] Preços reais, limite de 999, débito único da carteira e registro de despesas.
- [x] Crédito nos estoques usados por depósitos/cochos; preservação nas passagens.
- [x] Pausa de jogador/mundo/relógio e nomes cadastrados nas mensagens.
- [x] Atalhos de região com `--preview-date` e `--preview-time`, sem conceder recursos.
- [x] Percurso loja → compra → retorno pelo mapa → depósito → cocho, usando os 500G iniciais.
- [x] 30 CTests e oito percursos com recursos locais no Linux.

### Galinhas adultas e coleta silvestre — 2026-10-24

- [x] Coleta dos runs originais 11–14: posições, estações, flores/ervas e reposição diária.
- [x] Venda dos produtos coletados pela mesma caixa, carteira e livro de receitas.
- [x] Compra de galinha adulta de Lillia por 1.500G, nome de oito glifos e limite inicial de cinco.
- [x] Recusa por saldo/lotação, cancelamento e débito único após confirmar o nome.
- [x] Sprites originais, caminhada sobre o terreno, repouso noturno e pausa em menus.
- [x] Consumo individual antes da limpeza dos cochos e afeição diária.
- [x] Ovos por alimentação anterior, graus, coleta animada, mochila e venda.
- [x] 32 CTests e nove percursos com dados locais; ciclo econômico partindo dos 500G iniciais.

### Incubação e eventos do ciclo das galinhas — 2026-10-24

- [x] Incubadora na zona original, reserva de vaga e depósito animado de um ovo.
- [x] Recusa por incubadora ocupada, melhoria ausente ou lotação, sem perder o ovo.
- [x] Três noites de incubação e evento original 299 para nomear o pintinho.
- [x] Nome obrigatório, liberação da incubadora e sete noites de crescimento.
- [x] Sprites de pintinho e transição real ao ciclo adulto; sem ração/ovos durante crescimento.
- [x] Evento original 172 de funeral por idade/doença, personagens, cenário, música e luto.
- [x] Lista de nomes, penalidades por doença, remoção dos mortos e liberação das vagas.
- [x] Retorno à casa e continuação do fade após encerrar o script.
- [x] 32 CTests e dez percursos com recursos locais no Linux.

### Venda de galinhas pela loja de Lillia — 2026-10-24

- [x] Terceira opção da loja, descrição e recusa original sem adultas disponíveis.
- [x] Lista de quatro linhas com rolagem, nomes, data/idade, estação, afeição e prêmio.
- [x] Exclusão de incubação, pintinhos, nomes pendentes e mortos da lista de venda.
- [x] Oferta de 500G/2.000G, confirmação Yes/No e cancelamentos sem efeitos.
- [x] Remoção individual, liberação/reuso de vaga e atualização da contagem.
- [x] Crédito único em carteira/receitas, com limite de saldo e receita integral.
- [x] 32 CTests e dez percursos com recursos locais revalidados no Linux.

### Medicamento animal e cura das galinhas — 2026-10-24

- [x] Medicamento de 1.000G em Lillia/Barley, descrições e mensagens originais.
- [x] Verificação de posse no equipamento, mochila acessível e baú; opção oculta enquanto guardado.
- [x] Compra para o equipamento/vaga da mochila, recusa por saldo/lotação e débito único.
- [x] Equipamento por R1 e tratamento por C/Square nas quatro direções.
- [x] Banco de ação 22, alvo fixado antes da animação, cura e consumo de uma dose ao concluir.
- [x] Cue original de coração, som, fadiga e retorno ao controle normal.
- [x] Bloqueio de movimento, menus, coleta e troca de ferramentas durante o uso.
- [x] 32 CTests e onze percursos com recursos locais aprovados no Linux.

### Transporte de galinhas e ciclo no exterior — 2026-10-24

- [x] Pegar/colocar adultas e pintinhos nomeados mantendo o mesmo registro reservado.
- [x] Poses originais de duas mãos, quatro direções, sons e travessia das portas.
- [x] Proteção contra guardar em mochila/equipamento, vender carregado e duplicar animais.
- [x] Validação do chão, água, caixa de venda, obstáculos do campo e espaço de outra galinha.
- [x] Consulta original de ocupação estudada: galinhas selecionáveis sem parada rígida do jogador.
- [x] Alimentação externa horária, consumo dos cochos por área e avanço durante o sono.
- [x] Ovos externos por posição/área, sprites sazonais, coleta e remessa.
- [x] Retorno ao interior, liberação antes de dormir e sincronização da mão em crescimento/morte.
- [x] 33 CTests e doze percursos com recursos locais aprovados no Linux.

### Página de estado das galinhas — 2026-10-24

- [x] Corrigir o erro ao abrir a página de galinhas após comprar um animal.
- [x] Ler registros reais da sessão, incluindo animais carregados e pintinhos nomeados.
- [x] Ícones originais de adulta/pintinho/doença, nomes, afeição, data/idade, estação e prêmio.
- [x] Sete linhas, rolagem circular e setas pulsantes do pack original de estado.
- [x] Contagem do resumo exclui incubação/nome pendente sem liberar suas vagas.
- [x] Troca de página/reabertura zera a rolagem; pausa conserva relógio e animais.
- [x] Renderizar a página no percurso econômico depois de uma compra normal.
- [x] 33 CTests e treze percursos com recursos locais aprovados no Linux.

Total: **219 itens concluídos**.
