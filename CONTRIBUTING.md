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

Para a base nativa, execute `cmake --preset native-debug`, `cmake --build --preset native-debug` e `ctest --preset native-debug`. Repita com `native-release` ao alterar formatos ou configurações de compilação.

A CI nativa está configurada para Linux e Windows. O bootstrap foi testado localmente em Linux; não anuncie o jogo como jogável ou Windows como validado sem evidência. A validação SHA-1 do PS1 permanece no workflow original e requer seu ambiente próprio. Consulte `docs/port/BUILDING.md`.
