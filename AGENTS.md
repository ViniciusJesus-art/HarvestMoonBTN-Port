# Trabalho neste repositório

- O foco atual é o porte nativo para Linux e Windows. Android fica para depois.
- Preserve `src/`, `sdk/`, `disc/`, `tools/` e o fluxo CMake PS1 ao preparar o porte.
- Implemente adaptações em `port/`; documentação técnica em `docs/port/`.
- Use builds separados: `build/native-*` para host, `build/psx` para PS1.
- O bootstrap nativo não executa o jogo. Mantenha esse estado explícito na documentação.
- Recursos do PS1 são little-endian e usam endereços de 32 bits. Não converta endereços serializados diretamente para ponteiros host; não use `long` como inteiro PS1.
- Evite stubs que aparentem implementar gráficos, áudio ou gameplay sem comportamento real.
- Não adicione discos, recursos proprietários ou SDKs ao Git.
- Para validar a base: `cmake --preset native-debug`, `cmake --build --preset native-debug`, `ctest --preset native-debug`.
- O build original exige toolchain PS1 e validação SHA-1; o build host não substitui essa verificação.
