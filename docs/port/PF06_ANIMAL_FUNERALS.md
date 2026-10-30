# PF-06 — Funerais bovinos/ovinos

Entrega de 2026-10-29, seguindo a reprodução na fila do [ROADMAP.md](ROADMAP.md).
Amplia o evento original 172 já usado pelas galinhas. Não introduz outro
controlador de eventos, lista de animais ou mecanismo de afinidade.

## Comportamento

A manhã na casa verifica mortes de vacas, ovelhas e aves, com prioridade sobre
nascimento/eclosão. O evento 172 usa a igreja/cemitério, Carter, Barley e Rick,
seus modelos/ANI, mensagens, música, fades e ação de choro existentes. Os seis
ramos do roteiro instalado são executados conforme espécie e causa:

| Espécie | Idade: texto/lista | Outras causas: texto/lista |
|---|---|---|
| Vacas, incluindo jovens/bezerros | 1:498 / 0x401B | 1:494 / 0x401E |
| Ovelhas, incluindo cordeiros | 1:500 / 0x401C | 1:496 / 0x401F |
| Galinhas/pintinhos | 1:504 / 0x401D | 1:502 / 0x4020 |

Cada lista preserva nomes e ordem de registros. A paginação nativa existente
agora atende também o rebanho inteiro, sem truncar os vinte nomes de oito glifos.
A distinção de causa usa `died_of_age`, não uma dedução pela idade atual.

O próprio roteiro avança uma hora ao terminar e uma hora adicional por grupo
de espécie com morte por outras causas. Em uma manhã com vacas, ovelhas e aves
nessa situação, são quatro horas ao todo. A penalidade social de doença ocorre
uma vez no evento: -20 de amizade, -1.000 na trilha de afeição das candidatas
e -20 para ajudantes, com as exceções/limites já implementados no comando comum.
Morte por idade sozinha não aplica essa penalidade.

A limpeza também mantém um efeito adicional comprovado no original: o comando
14/1 continua para `ESTATE_GIFT_ROW97_BONUS`, ativando o bônus de **10 G na venda
de mel**. A adaptação usa o booleano persistente já existente na economia;
não cria outro preço ou estoque. A conferência final do BIN em
**0x800ea8e4..0x800ea934** confirma essa passagem e a gravação do bit 22.

Somente o comando de limpeza do evento remove os mortos. Cada registro removido
incrementa o contador de descarte existente, limitado a 99; a reserva vazia de
um filhote não conta como outra morte. Libera também a maternidade e a reserva
associadas à mãe removida. Animais vivos, filhotes já nascidos, outras gestações,
partos pendentes, cochos e incubadoras permanecem. A mesma liberação de reserva
é compartilhada com a venda; não há uma segunda regra para os mesmos pares.

Após o funeral, um parto/eclosão pendente continua pelo evento 299. Reabrir o
estado após a conclusão não repete limpeza nem penalidades. As mortes de idade
e doença continuam sendo produzidas pela atualização noturna existente; não
foram acrescentadas remoções automáticas fora do evento.

## Origem e arquivos

`F` = `src/overlay/11_GF_FARM`; as adaptações ficam somente em `port/`.

| Código original | Adaptação |
|---|---|
| `F/animals/animals.c`: `farm_kind_selected`, `farm_clear_flagged_items` | `game/livestock.c/.h`: consulta por espécie/causa e limpeza dos registros; reservas explícitas do domínio nativo são liberadas junto da mãe |
| `F/event/event_commands.c`: `EVENT_CMD_ANIMAL_DEATH`, teste/limpeza | `game/runtime.c`: amplia o comando 0x51/14 existente e o gatilho matinal; mantém o interpretador e efeitos comuns |
| `src/main/text/text.c`: `text_insert_source`, `text_list_livestock`, `text_list_flagged_livestock`, `text_list_chickens`, `text_append_name` | `game/poultry.c/.h`: expansão comum de 0x401B..0x4020; wrapper anterior das aves preservado, mesma paginação |
| Script instalado 172 em `12_event_s.bin` | Executado sem substituir ou alterar o recurso; ramos de vacas/ovelhas agora recebem as consultas e nomes reais |

Foi conferido o roteiro extraído, suas seis consultas, textos, subrotinas,
avanços de relógio, penalidade e limpeza. A desmontagem do overlay instalado
confirma `farm_kind_selected` em **0x800cfb20** e `farm_clear_flagged_items` em
**0x800cfc20**: seleção por flags de espécie/causa e limpeza dos registros
marcados, com contador limitado a 99. SHA-1 local do overlay:
`fa5c694b54c7c82be5adfd23975c6a2b6e5c83f6`. Como registrado nos relatórios
anteriores, esse disco local é modificado; não é alegada igualdade com o build
PS1 de referência. Endereços servem apenas à auditoria, nunca como ponteiros host.

A liberação das reservas mãe/filhote e a preservação de filho já nascido são
adaptações de integridade à coleção explícita do port. O caminho PS1 consultado
limpa o registro marcado; não se declara que ele implementa a mesma estrutura
ou sequência de manutenção dos pares nativos.

## Estado, validação e limites

As flags de morte, causa e reservas já eram persistentes. O codec não precisou
de alterações nesta entrega. A orientação atual é adaptar o save somente quando
a funcionalidade precisar, sem exigir nem proibir compatibilidade entre versões.
Os testes verificam o estado antes/depois e a retomada em processos novos.

- Linux Debug: configure/build e **14/14 CTests selecionados**, **58,94 s**.
  Unidade nova: espécies/causas, vinte nomes completos, mortes noturnas,
  remoção única, reserva de mãe morta, filho já nascido e estado recarregado.
- Percurso novo: morte após sono real, os seis ramos do evento 172, listas
  paginadas, igreja/atores/imagens reais, tempo/afinidade/descarte, reposição na
  vaga liberada e nenhuma repetição. Outra maternidade e incubadora concluem
  seus nomes pelo evento 299 depois de a mãe do primeiro par ser removida.
- Os snapshots pendente e concluído também abrem em processos novos do app.
  Após a conferência do bônus do mel, percurso e regras de remessa foram
  verificados novamente no Debug; o percurso também foi repetido em Release/Wine.
- Linux Release: **3/3 CTests**, **18,77 s**, incluindo unidade, percurso novo
  com processos filhos e fidelidade do mundo; percurso direto final aprovado.
- Windows MinGW Release: build; unidade, percurso completo de funerais e
  regressão de fidelidade validados no Wine; percurso repetido após o efeito do mel. Windows real/MSVC não executados.
- Orçamento da rodada: **43 processos de testes/wrappers/aplicativos** incluindo
  tentativas e repetições; contando também quatro lançadores Wine, **47**, abaixo
  do limite 50. Suíte inteira não executada.

Regressões selecionadas cobrem galinhas/incubação/funeral, reprodução, venda,
medicina/ferramentas, transporte de galinhas, saves do estado atual, chuva,
portas internas e entrada do Doug. O código de portas/transições do início da
rodada foi comparado ao resultado e permaneceu idêntico. Ajustes locais do
usuário e trabalho anterior foram preservados.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -R '^native_(animal_funeral|animal_funeral_playthrough)$' --output-on-failure
```

PF-06 continua aberta: pastagem/transporte bovino/ovino, sino, máquinas,
concursos e demais sistemas ainda têm recortes próprios. Cues/movimento animal
integrais e equivalência audiovisual com PS1 continuam parciais. O bootstrap
diagnóstico não executa o jogo; estas mudanças pertencem ao runtime jogável
parcial. Nenhum código/recurso de `src/`, `sdk/`, `disc/`, `tools/` ou fluxo PS1
foi alterado, nem recursos proprietários/artefatos acrescentados ao Git.
