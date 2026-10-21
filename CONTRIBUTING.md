# Contribuindo

Este é um fork experimental de [coalslav/hmbtn-decomp](https://github.com/coalslav/hmbtn-decomp) com o objetivo de criar um port nativo para Linux, Windows e Android.

## Organização

- `main`: documentação e base estável de integração do projeto.
- Branches `feature/*`: desenvolvimento de recursos e correções.
- Preferir pull requests pequenos e com descrição dos testes executados.
- Evitar alterar diretamente o código decompilado quando uma camada de compatibilidade bastar.
- Nunca incluir imagens de disco, recursos de jogos protegidos, chaves, SDKs ou outros arquivos sem licença adequada.

## Mantendo o upstream sincronizado

```bash
git remote add upstream https://github.com/coalslav/hmbtn-decomp.git
git fetch upstream
git switch main
git merge upstream/main
```

O merge poderá exigir resolução de conflitos. Revise os arquivos antes de integrar.

## Validação

Enquanto o port nativo não existe, o único build funcional documentado é o da decompilação para PS1. Não anuncie builds nativos ou suporte a plataformas que ainda não foram testados.
