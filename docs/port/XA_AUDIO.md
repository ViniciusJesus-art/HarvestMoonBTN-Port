# Áudio XA da infância

As faixas 4 e 5 de `src/main/sound/cd_audio.c` usam o arquivo extraído
`data/disc/STR/01.XA`, canais 4 e 5, filtrados pelo número de arquivo 1.
A introdução e a canção da menina executam durante o evento `0x77`, inclusive
suas esperas no script. Não se substitui o áudio por um temporizador sem som.

- `port/audio/xa_decode.*`: decodificação C11 de setores de 2.336 bytes com
  subcabeçalho duplicado, ADPCM de 4 bits e histórico independente por canal.
- `port/audio/xa_stream.*`: leitura de um setor por vez, seleção do canal,
  conversão para 44.100 Hz, mistura e estado da faixa.
- `port/audio/title_audio.*`: entrega ao SDL o resultado de SEQ/efeitos mais XA.
- `port/game/farm_intro.*`: solicita a faixa e espera a confirmação do host;
  a consulta de término só libera o roteiro quando a reprodução termina.

O formato segue a pesquisa técnica primária
[PSX-SPX — CDROM XA Audio ADPCM](https://psx-spx.consoledev.net/dev/ps1/cdr/cdromformat/#cdrom-xa-audio-adpcm-compression).
A implementação decodifica os grupos ADPCM com os quatro preditores, saturação
em 16 bits e histórico entre setores do canal selecionado. Aceita mono/estéreo
em 37.800 ou 18.900 Hz; modos de 8 bits/ênfase continuam sem integração e são
rejeitados. O resampling usa interpolação linear; o filtro zigzag do hardware
permanece uma diferença de fidelidade documentada.

O estado mantém um arquivo aberto, um setor de PCM e dois frames para
interpolação. Não carrega os 16 MiB do arquivo inteiro e não guarda a canção
completa em memória. EOF do canal termina o fornecimento de amostras; setores
alheios são ignorados, arquivos truncados e canais ausentes falham com erro.
Os tempos e volumes seguem `cd_audio_fade_tick` e `cd_audio_tracks`, com o modo
original `MAIN_SPEED_FAST_2` (dois decrementos por tick nativo). A cena continua
funcionando sem dispositivo sonoro, assim como o áudio SEQ já existente.

## Validação

O teste `native_xa_audio` usa setores inteiramente sintéticos: mono/estéreo,
preditores, histórico, shifts reservados, arquivos/canais, temporização,
fim de fluxo e rejeição de formatos inválidos. Os arquivos temporários ficam
na pasta do build; nenhum recurso proprietário entra no Git.

Os dois canais locais foram decodificados por inteiro e comparados a uma
execução independente do FFmpeg (`psxstr` → PCM `s16le`):

| Canal | Frames estéreo a 37.800 Hz | Bytes PCM | Diferenças |
|---|---:|---:|---:|
| 4 | 635.040 | 2.540.160 | 0 |
| 5 | 802.368 | 3.209.472 | 0 |

Os artefatos `xa4/5-native.pcm`, `xa4/5-ffmpeg.pcm` e os contêineres de comparação
ficam ignorados em `build/native-debug/`. O FFmpeg serve somente para validar;
o executável Linux/Windows não depende dele nem de um emulador.
Essa comparação comprova a decodificação antes do resampling e da mistura,
não equivalência de toda a saída sonora ao hardware PS1.
