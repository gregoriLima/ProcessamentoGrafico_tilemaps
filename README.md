# ProcessamentoGrafico

Este projeto contem a atividade vivencial 2, um jogo 2D simples em OpenGL com personagem, cenario em parallax e movimentacao por teclado e mouse.

## Arquivo principal

- `src/atividade_vivencial_2.cpp`: jogo com personagem, camadas `sky`, `clouds`, `mountains` e `pixelWall`.

## Dependencias locais

- `assets/`: texturas usadas no jogo.
- `common/glad.c`: loader OpenGL.
- `include/`: headers locais de GLAD, GLM e `stb_image.h`.
- `third_party/glfw/`: codigo-fonte local do GLFW para build offline.

## Como compilar

No PowerShell, entre na pasta `build` e execute:

```powershell
cmake --build . --target atividade_vivencial_2
```

## Como executar

Ainda na pasta `build`, execute:

```powershell
.\atividade_vivencial_2.exe
```

O executavel deve ser iniciado a partir da pasta `build` para que os arquivos em `..\assets\` sejam encontrados corretamente.

## Controles

- Setas direcionais: movem o personagem.
- Clique esquerdo do mouse: reposiciona o personagem dentro da area jogavel.
- `Esc`: fecha a janela.