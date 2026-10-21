# Plano de desenvolvimento — HarvestMoonBTN-Port

> Documento de planejamento. Nenhuma etapa de port nativo está concluída.

## Fase 0 — Base verificável
- Preservar o build PS1 e a verificação SHA-1 da decompilação original.
- Documentar as dependências e o fluxo de compilação no Fedora.
- Mapear chamadas do PSY-Q, endereços fixos e carregamento de overlays.

## Fase 1 — Protótipo Linux
- Criar um target CMake nativo independente do build PS1.
- Avaliar SDL3 e bibliotecas de compatibilidade existentes.
- Portar inicialização e ciclo de frames; abrir janela e mostrar a primeira imagem real do jogo.
- Separar extração de recursos proprietários do código distribuído.

## Fase 2 — Jogo funcional
- Implementar renderização, áudio, entrada, temporização e leitura de arquivos.
- Adaptar overlays e dependências de memória do PS1.
- Validar gameplay, eventos, saves e transições de cenas.

## Fase 3 — Windows
- Configurar build nativo e testes em Windows.
- Automatizar CI apenas quando houver um target compilável.

## Fase 4 — Android
- Integrar SDL3/NDK ou alternativa validada.
- Implementar eventos do ciclo de vida, interface e controles por toque.

## Fase 5 — Mods e melhorias
- Definir ABI/API de eventos e recursos.
- Desenvolver manifesto de mods, carregador e detector de conflitos.
- Implementar testes de compatibilidade e versões.

## Definição do primeiro marco
Uma aplicação Linux que executa o loop original e apresenta a tela de título com gráficos e entrada funcionais — sem recorrer ao emulador.
