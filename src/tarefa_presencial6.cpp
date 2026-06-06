// =============================================================================
// Tarefa Presencial 6 - Tilemap Isometrico Diamond
//
// Estrutura geral do arquivo:
//   1. Variaveis globais (janela, dimensoes, tileset)
//   2. Funcoes auxiliares (OpenGL, shader, textura, movimento)
//   3. main(): monta o mapa, cria geometria na GPU e roda o loop
//
// O mapa e desenhado em projecao isometrica diamond:
//   - tiles sao losangos com proporcao 2:1 (largura = 2x altura)
//   - a posicao na tela de cada tile depende de sua coluna e linha na matriz
//
// Coordenadas do mundo: NDC (Normalized Device Coordinates)
//   - eixo X vai de -1 (esquerda) a +1 (direita)
//   - eixo Y vai de -1 (baixo)   a +1 (cima)
//   - o vertex shader converte as posicoes locais de tile para NDC final
// =============================================================================

#include <cstdio>
#include <cstring>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// DiamondView: calcula onde cada tile deve ser desenhado na tela
//              e como o cursor se move entre os tiles
// TileMap:     armazena a grade de tiles (qual tile esta em cada celula)
#include "DiamondView.h"
#include "TileMap.h"

using namespace std;

// -----------------------------------------------------------------------------
// Variaveis globais
// -----------------------------------------------------------------------------

// dimensoes da janela em pixels
int g_gl_width  = 800;
int g_gl_height = 400;
GLFWwindow *g_window = NULL;

// limites da area do mapa em pixels
// com ortho(0, 800, 0, 400) cada unidade do mundo = 1 pixel na tela
float xi = 0.0f;            // borda esquerda (pixels)
float xf = 800.0f;          // borda direita  (pixels)
float yi = 0.0f;            // borda de baixo (pixels)
float yf = 400.0f;          // borda de cima  (pixels)
float w  = xf - xi;        // largura total = 800.0 pixels
float h  = yf - yi;        // altura total  = 400.0 pixels

// tamanho de um tile em pixels
// tw: largura do tile  (calculado em main com base no numero de colunas)
// th: altura do tile   (sempre tw/2 para manter proporcao 2:1)
float tw = 0.0f;
float th = 0.0f;

// o tileset e uma imagem dividida em grade de tiles
// TILESET_COLS x TILESET_ROWS = total de tiles disponiveis
const int TILESET_COLS = 9;
const int TILESET_ROWS = 9;

// fracao do espaco UV que cada tile ocupa na textura
// ex: com 9 colunas, cada tile ocupa 1/9 da largura da textura
float tileW  = 0.0f; // largura de um tile em UV (0.0 a 1.0)
float tileH  = 0.0f; // altura  de um tile em UV (0.0 a 1.0)
float tileW2 = 0.0f; // metade de tileW (usado nos vertices do losango)
float tileH2 = 0.0f; // metade de tileH (usado nos vertices do losango)

// tview: sabe as regras do mapa diamond (como desenhar e como navegar)
// tmap:  a grade de tiles em si (qual tile esta em [col][row])
DiamondView tview(xi, yi);
TileMap *tmap = NULL;

// -----------------------------------------------------------------------------
// KeyLatch: impede que segurar uma tecla mova o cursor multiplas vezes
// guarda se a tecla JA estava pressionada no frame anterior
// o movimento so acontece na transicao "nao pressionada -> pressionada"
// -----------------------------------------------------------------------------
struct KeyLatch {
    bool up    = false;
    bool down  = false;
    bool left  = false;
    bool right = false;
};

// mapa do jogo: cada valor e o id de um tile no tileset
// linha 0 e a linha do topo da tela
const int MAP_COLS = 3;
const int MAP_ROWS = 3;
int map[MAP_ROWS][MAP_COLS] = {
    { 1, 1, 1 },
    { 1, 1, 1 },
    { 1, 1, 1 },
};

// -----------------------------------------------------------------------------
// Funcoes auxiliares
// -----------------------------------------------------------------------------

// retorna o caminho da pasta onde este arquivo .cpp esta salvo em disco
// usado para montar caminhos de assets relativos ao codigo-fonte
string sourceDirectory() {
    string filePath = __FILE__;
    const size_t separator = filePath.find_last_of("\\/");
    if (separator == string::npos) {
        return string();
    }
    return filePath.substr(0, separator + 1);
}

// junta a pasta do codigo com um caminho relativo
// ex: sourceAssetPath("exemplo/terrain.png") -> "C:/projeto/src/exemplo/terrain.png"
string sourceAssetPath(const char *relativePath) {
    return sourceDirectory() + relativePath;
}

// inicializa GLFW e GLAD, cria a janela e ativa o contexto OpenGL
bool initialiseOpenGL() {
    if (!glfwInit()) {
        printf("Falha ao inicializar GLFW.\n");
        return false;
    }

    // pede OpenGL 3.3 no perfil core (sem funcoes legadas)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(g_gl_width, g_gl_height, "Tarefa Parte 6 - Tilemap", NULL, NULL);
    if (g_window == NULL) {
        printf("Falha ao criar a janela OpenGL.\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1); // sincroniza com o monitor (vsync)

    // GLAD carrega os ponteiros das funcoes do OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Falha ao inicializar GLAD.\n");
        glfwDestroyWindow(g_window);
        g_window = NULL;
        glfwTerminate();
        return false;
    }

    return true;
}

// compila um shader (vertex ou fragment) a partir do codigo-fonte em texto
// retorna o id do shader criado na GPU, ou 0 se falhou
GLuint compileShader(GLenum shaderType, const char *shaderSource) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    // se falhou, imprime o log de erro do compilador GLSL
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    string shaderLog(static_cast<size_t>(logLength), '\0');
    glGetShaderInfoLog(shader, logLength, NULL, shaderLog.data());
    printf("Falha ao compilar shader: %s\n", shaderLog.c_str());
    glDeleteShader(shader);
    return 0;
}

// cria um programa OpenGL juntando vertex shader e fragment shader
// o programa e o que roda na GPU quando chamamos glDrawElements
// retorna o id do programa, ou 0 se falhou
GLuint createProgram(const char *vertexShaderSource, const char *fragmentShaderSource) {
    GLuint vertexShader   = compileShader(GL_VERTEX_SHADER,   vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader   != 0) glDeleteShader(vertexShader);
        if (fragmentShader != 0) glDeleteShader(fragmentShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        string programLog(static_cast<size_t>(logLength), '\0');
        glGetProgramInfoLog(program, logLength, NULL, programLog.data());
        printf("Falha ao linkar programa: %s\n", programLog.c_str());
        glDeleteProgram(program);
        program = 0;
    }

    // shaders individuais nao sao mais necessarios apos o link
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// carrega uma imagem PNG/JPG do disco e envia para a GPU como textura 2D
// retorna true em caso de sucesso; o id da textura fica em 'texture'
bool loadTexture(GLuint &texture, const string &filename) {
    int imageWidth  = 0;
    int imageHeight = 0;
    int channels    = 0;
    // stbi_load decodifica a imagem e devolve os pixels em memoria
    unsigned char *data = stbi_load(filename.c_str(), &imageWidth, &imageHeight, &channels, 0);
    if (data == NULL) {
        printf("Falha ao carregar textura: %s\n", filename.c_str());
        return false;
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // CLAMP_TO_EDGE: nas bordas nao repete a textura (evita artefatos)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // LINEAR: suaviza quando a textura e ampliada ou reduzida
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // detecta se a imagem tem canal alpha (PNG com transparencia = 4 canais)
    const GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, imageWidth, imageHeight, 0, format, GL_UNSIGNED_BYTE, data);
    // mipmaps: versoes menores da textura para quando o tile aparece pequeno na tela
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data); // pixels ja estao na GPU, libera da RAM
    return true;
}

// retorna true se (col, row) e uma posicao valida dentro dos limites do mapa
bool isInsideMap(int col, int row) {
    return tmap != NULL
        && col >= 0 && col < tmap->getWidth()
        && row >= 0 && row < tmap->getHeight();
}

// tenta mover o cursor na direcao pedida
// se a nova posicao estiver fora do mapa, nao move e retorna false
bool moveIfValid(int &col, int &row, int direction) {
    int nextCol = col;
    int nextRow = row;
    // computeTileWalking aplica as regras diamond: qual celula fica em cada direcao
    tview.computeTileWalking(nextCol, nextRow, direction);
    if (!isInsideMap(nextCol, nextRow)) {
        return false;
    }
    col = nextCol;
    row = nextRow;
    return true;
}

// processa uma tecla de movimento: so move quando a tecla acaba de ser pressionada
// 'wasPressed' guarda o estado da tecla no frame anterior (vem do KeyLatch)
void processMovementKey(int key, bool &wasPressed, int direction, int &col, int &row) {
    const bool isPressed = glfwGetKey(g_window, key) == GLFW_PRESS;
    if (isPressed && !wasPressed) {
        // tecla acabou de ser pressionada neste frame: move o cursor
        moveIfValid(col, row, direction);
    }
    wasPressed = isPressed; // atualiza para o proximo frame
}

// desenha um cubinho isometrico em cima do tile (col, row)
// as tres faces recebem tons diferentes da cor escolhida para dar sensacao de 3D:
//   topo = cor original, face esquerda = 60%, face direita = 40%
// proj: matriz ortografica (pixels -> clip space), a mesma usada nos tiles
void drawMarker(GLuint markerProgram, GLuint markerVAO, glm::mat4 proj, int col, int row, float red, float green, float blue) {
    // descobre onde na tela este tile e desenhado (em pixels)
    float tileX = 0.0f;
    float tileY = 0.0f;
    tview.computeDrawPosition(col, row, tw, th, tileX, tileY);

    glUseProgram(markerProgram);
    glBindVertexArray(markerVAO);

    // envia a matriz de projecao para o shader do marcador
    glUniformMatrix4fv(glGetUniformLocation(markerProgram, "proj"), 1, GL_FALSE, glm::value_ptr(proj));

    // 'offset' desloca o cubinho para o centro do tile em pixels
    // sem isso o cubinho apareceria no canto inferior esquerdo do tile
    glUniform2f(glGetUniformLocation(markerProgram, "offset"), tileX + tw * 0.5f, tileY + th * 0.5f);

    // desenha o topo (indices 0..5 no cubeIndices)
    glUniform4f(glGetUniformLocation(markerProgram, "color"), red, green, blue, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);

    // desenha a face esquerda (indices 6..11)
    glUniform4f(glGetUniformLocation(markerProgram, "color"), red * 0.6f, green * 0.6f, blue * 0.6f, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(unsigned int)));

    // desenha a face direita (indices 12..17)
    glUniform4f(glGetUniformLocation(markerProgram, "color"), red * 0.4f, green * 0.4f, blue * 0.4f, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(12 * sizeof(unsigned int)));
}

// =============================================================================
// main
// =============================================================================
int main() {
    if (!initialiseOpenGL()) {
        return 1;
    }

    // depth test: tiles desenhados depois so aparecem se estiverem "na frente"
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // blend: permite transparencia (pixels com alpha < 1 deixam ver o que esta atras)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // copia o mapa global (definido acima do main) para o TileMap
    // TileMap usa Y crescendo para cima, por isso lemos de baixo para cima (MAP_ROWS - r - 1)
    tmap = new TileMap(MAP_COLS, MAP_ROWS, 0);
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            tmap->setTile(c, MAP_ROWS - r - 1, (unsigned char)map[r][c]);
        }
    }

    // -------------------------------------------------------------------------
    // Tamanho dos tiles em pixels
    //
    // tw: divide a largura da tela pelo numero de colunas do mapa
    //     ex: mapa 3 colunas -> tw = 800 / 3 = 266.7 pixels
    // th: sempre metade de tw para manter a proporcao isometrica 2:1
    //     ex: th = 266.7 / 2 = 133.3 pixels
    // -------------------------------------------------------------------------
    tw = w / (float)tmap->getWidth();
    th = tw / 2.0f;

    // fracao UV de cada tile na textura do tileset
    // ex: 9 colunas -> tileW = 1/9 ~= 0.111
    tileW  = 1.0f / (float)TILESET_COLS;
    tileH  = 1.0f / (float)TILESET_ROWS;
    tileW2 = tileW / 2.0f; // metade da largura UV (ponta central do losango)
    tileH2 = tileH / 2.0f; // metade da altura UV  (ponta central do losango)

    // informa ao DiamondView o tamanho do mapa para que ele calcule
    // o offset horizontal que centraliza o losango de tiles na tela
    tview.setMapSize(tmap->getWidth(), tmap->getHeight());

    // -------------------------------------------------------------------------
    // Carregamento do tileset
    // -------------------------------------------------------------------------
    const string texturePath = sourceAssetPath("exemplo/terrain.png");
    GLuint tilesetTexture = 0;
    if (!loadTexture(tilesetTexture, texturePath)) {
        delete tmap;
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return 1;
    }
    tmap->setTid(tilesetTexture); // associa a textura ao mapa

    // -------------------------------------------------------------------------
    // Shaders
    //
    // Shader do tile (tileProgram):
    //   - vertex: recebe posicao local do vertice em pixels (0..tw, 0..th),
    //             soma o offset do tile (tx, ty) e multiplica pela matriz proj
    //             que converte pixels para clip space automaticamente
    //   - fragment: le o pixel certo do tileset usando as coordenadas UV
    //               ajustadas por offsetx/offsety (que selecionam o tile)
    //
    // Shader do marcador (markerProgram):
    //   - vertex: mesmo esquema, usa offset 2D em pixels e matriz proj
    //   - fragment: pinta com a cor uniforme recebida (sem textura)
    //
    // A matriz proj e criada com glm::ortho(0, 800, 0, 400):
    //   - mapeia x=0 para a esquerda, x=800 para a direita
    //   - mapeia y=0 para baixo,      y=400 para cima
    //   - assim cada unidade = 1 pixel, sem precisar de formulas manuais
    // -------------------------------------------------------------------------
    const char *tileVertexShader =
        "#version 330 core\n"
        "layout (location = 0) in vec2 vertex_position;\n" // posicao local em pixels (0..tw, 0..th)
        "layout (location = 1) in vec2 texture_mapping;\n" // UV local do tile
        "out vec2 texture_coords;\n"
        "uniform mat4 proj;\n"      // matriz ortografica: pixels -> clip space
        "uniform float layer_z;\n"  // profundidade para depth test
        "uniform float tx;\n"       // posicao X do tile na tela em pixels
        "uniform float ty;\n"       // posicao Y do tile na tela em pixels
        "void main() {\n"
        "  texture_coords = texture_mapping;\n"
        // soma a posicao local com o offset do tile e projeta para clip space
        "  gl_Position = proj * vec4(vertex_position.x + tx, vertex_position.y + ty, layer_z, 1.0);\n"
        "}\n";

    const char *tileFragmentShader =
        "#version 330 core\n"
        "in vec2 texture_coords;\n"
        "uniform sampler2D sprite;\n"
        "uniform float offsetx;\n" // deslocamento UV horizontal para selecionar o tile certo
        "uniform float offsety;\n" // deslocamento UV vertical   para selecionar o tile certo
        "out vec4 frag_color;\n"
        "void main() {\n"
        // soma o UV local com o offset do tile no tileset para buscar o pixel certo
        "  vec4 texel = texture(sprite, vec2(texture_coords.x + offsetx, texture_coords.y + offsety));\n"
        // descarta pixels transparentes (bordas do tile no tileset)
        "  if (texel.a < 0.1) {\n"
        "    discard;\n"
        "  }\n"
        "  frag_color = texel;\n"
        "}\n";

    const char *markerVertexShader =
        "#version 330 core\n"
        "layout (location = 0) in vec2 vertex_position;\n"
        "uniform mat4 proj;\n"    // matriz ortografica: pixels -> clip space
        "uniform vec2 offset;\n"  // centro do tile em pixels (onde o cubinho aparece)
        "void main() {\n"
        "  gl_Position = proj * vec4(vertex_position.x + offset.x, vertex_position.y + offset.y, 0.1, 1.0);\n"
        "}\n";

    const char *markerFragmentShader =
        "#version 330 core\n"
        "uniform vec4 color;\n" // cor da face atual (passada a cada glDrawElements)
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = color;\n"
        "}\n";

    GLuint tileProgram   = createProgram(tileVertexShader,   tileFragmentShader);
    GLuint markerProgram = createProgram(markerVertexShader, markerFragmentShader);
    if (tileProgram == 0 || markerProgram == 0) {
        if (tileProgram   != 0) glDeleteProgram(tileProgram);
        if (markerProgram != 0) glDeleteProgram(markerProgram);
        glDeleteTextures(1, &tilesetTexture);
        delete tmap;
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return 1;
    }

    // -------------------------------------------------------------------------
    // Geometria do tile (losango)
    //
    // O tile eh um losango com 4 vertices:
    //
    // Cada vertice tem: posicao XY local  +  coordenada UV no tileset
    //
    // Os dois triangulos [0,1,3] e [3,1,2] formam o losango completo.
    //
    // As coordenadas UV locais vao de 0 ate tileW/tileH (fracao de um tile).
    // O fragment shader some isso com o offsetx/offsety do tile escolhido
    // para buscar o pixel certo dentro do tileset.
    // -------------------------------------------------------------------------
    const float tileVertices[] = {
    //   pos X       pos Y       UV X    UV Y
        0.0f,      th * 0.5f,   0.0f,   tileH2,  // [0] topo    (centro horizontal, meio da altura)
        tw * 0.5f, 0.0f,        tileW2, 0.0f,    // [1] direita (meio da largura,    base)
        tw,        th * 0.5f,   tileW,  tileH2,  // [2] base    (extremo direito,    meio da altura)
        tw * 0.5f, th,          tileW2, tileH,   // [3] esquer. (meio da largura,    topo)
    };
    // dois triangulos: [0,1,3] e [3,1,2]
    const unsigned int tileIndices[] = { 0, 1, 3,  3, 1, 2 };

    // VAO guarda a configuracao dos atributos; VBO guarda os vertices; EBO guarda os indices
    GLuint tileVAO = 0;
    GLuint tileVBO = 0;
    GLuint tileEBO = 0;
    glGenVertexArrays(1, &tileVAO);
    glGenBuffers(1, &tileVBO);
    glGenBuffers(1, &tileEBO);

    glBindVertexArray(tileVAO);

    glBindBuffer(GL_ARRAY_BUFFER, tileVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tileVertices), tileVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tileEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(tileIndices), tileIndices, GL_STATIC_DRAW);

    // atributo 0 (location=0 no shader): posicao XY — 2 floats, stride de 4 floats, offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // atributo 1 (location=1 no shader): coordenada UV — 2 floats, stride de 4 floats, offset 2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // -------------------------------------------------------------------------
    // Geometria do marcador (cubinho isometrico)
    //
    // 7 pontos formam o cubo visto de cima e de frente:
    //
    // 3 faces, cada uma com 2 triangulos:
    //   topo:         [0,1,2] e [0,2,3]
    //   face esquerda:[3,2,4] e [3,4,5]
    //   face direita: [1,2,4] e [1,4,6]
    // -------------------------------------------------------------------------
    const float hs = tw * 0.25f; // meia largura do topo do cubo
    const float vs = hs * 0.5f;  // meia altura do topo (proporcao 2:1)
    const float ch = vs;          // altura das faces laterais (igual a vs)
    const float cubeVertices[] = {
         0.0f,        vs,  // [0] ponta de cima
         hs,        0.0f,  // [1] ponta direita
         0.0f,       -vs,  // [2] ponta de baixo
        -hs,        0.0f,  // [3] ponta esquerda
         0.0f, -vs - ch,  // [4] base central (abaixo do centro do topo)
        -hs,        -ch,  // [5] base esquerda
         hs,        -ch,  // [6] base direita
    };
    const unsigned int cubeIndices[] = {
        0, 1, 2,  0, 2, 3,   // topo        (6 indices = 2 triangulos)
        3, 2, 4,  3, 4, 5,   // face esq.   (6 indices)
        1, 2, 4,  1, 4, 6,   // face dir.   (6 indices)
    };

    GLuint markerVAO = 0;
    GLuint markerVBO = 0;
    GLuint markerEBO = 0;
    glGenVertexArrays(1, &markerVAO);
    glGenBuffers(1, &markerVBO);
    glGenBuffers(1, &markerEBO);

    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, markerEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    // so um atributo: posicao XY (2 floats, sem stride extra)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // -------------------------------------------------------------------------
    // Estado inicial
    // -------------------------------------------------------------------------

    // posicao do cursor na grade do mapa (coluna, linha)
    int cursor_c = 0;
    int cursor_r = 0;

    KeyLatch keyLatch;

    // =========================================================================
    // Loop principal
    // =========================================================================
    while (!glfwWindowShouldClose(g_window)) {
        glfwPollEvents(); // processa eventos de teclado, mouse, janela

        if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(g_window, 1);
        }

        // ---------------------------------------------------------------------
        // Entrada: movimento do cursor com as setas (4 direcoes por enquanto)
        //
        // As direcoes NORTH/SOUTH/EAST/WEST sao definidas pelo DiamondView:
        //   NORTH: diminui coluna  (sobe-esquerda na tela)
        //   SOUTH: aumenta coluna  (desce-direita na tela)
        //   EAST:  diminui linha   (sobe-direita na tela)
        //   WEST:  aumenta linha   (desce-esquerda na tela)
        // ---------------------------------------------------------------------
        processMovementKey(GLFW_KEY_UP,    keyLatch.up,    DIRECTION_SOUTH, cursor_c, cursor_r);
        processMovementKey(GLFW_KEY_DOWN,  keyLatch.down,  DIRECTION_NORTH, cursor_c, cursor_r);
        processMovementKey(GLFW_KEY_LEFT,  keyLatch.left,  DIRECTION_WEST,  cursor_c, cursor_r);
        processMovementKey(GLFW_KEY_RIGHT, keyLatch.right, DIRECTION_EAST,  cursor_c, cursor_r);

        // ---------------------------------------------------------------------
        // Renderizacao
        // ---------------------------------------------------------------------
        glViewport(0, 0, g_gl_width, g_gl_height);
        glClearColor(0.12f, 0.18f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Desenho do mapa -------------------------------------------------
        // Percorre cada celula da grade e desenha o tile correspondente.
        // A ordem de baixo para cima (r crescente) garante que tiles mais
        // ao fundo sejam desenhados primeiro (painter's algorithm natural).

        // matriz ortografica: mapeia pixels (0..800, 0..400) para clip space (-1..1)
        // com isso cada unidade enviada ao shader = 1 pixel na tela
        glm::mat4 proj = glm::ortho(0.0f, (float)g_gl_width, 0.0f, (float)g_gl_height, -1.0f, 1.0f);

        glUseProgram(tileProgram);
        glBindVertexArray(tileVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tmap->getTileSet());
        glUniform1i(glGetUniformLocation(tileProgram, "sprite"), 0);
        // envia a matriz de projecao uma unica vez para todos os tiles
        glUniformMatrix4fv(glGetUniformLocation(tileProgram, "proj"), 1, GL_FALSE, glm::value_ptr(proj));

        for (int r = 0; r < tmap->getHeight(); r++) {
            for (int c = 0; c < tmap->getWidth(); c++) {
                // tileId: qual tile esta nesta celula
                int tileId = tmap->getTile(c, r);

                // mapeia o id para linha e coluna dentro do tileset
                // ex: id=10, TILESET_COLS=9 -> u=1, v=1 (segunda linha, segunda coluna)
                int u = tileId % TILESET_COLS; // coluna no tileset
                int v = tileId / TILESET_COLS; // linha   no tileset

                // posicao na tela onde este tile deve ser desenhado
                float tileX = 0.0f;
                float tileY = 0.0f;
                tview.computeDrawPosition(c, r, tw, th, tileX, tileY);

                // envia os uniforms para o shader
                glUniform1f(glGetUniformLocation(tileProgram, "offsetx"), u * tileW); // deslocamento UV horizontal
                glUniform1f(glGetUniformLocation(tileProgram, "offsety"), v * tileH); // deslocamento UV vertical
                glUniform1f(glGetUniformLocation(tileProgram, "tx"), tileX);          // posicao X na tela
                glUniform1f(glGetUniformLocation(tileProgram, "ty"), tileY);          // posicao Y na tela
                glUniform1f(glGetUniformLocation(tileProgram, "layer_z"), tmap->getZ());
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }

        // --- Desenho do marcador ---------------------------------------------
        // Desabilitamos o depth test para o cubinho aparecer sempre
        // em cima dos tiles, independente da profundidade calculada
        glDisable(GL_DEPTH_TEST);
        drawMarker(markerProgram, markerVAO, proj, cursor_c, cursor_r, 0.0f, 0.6f, 1.0f); // azul
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(g_window); // exibe o frame renderizado
    }

    // -------------------------------------------------------------------------
    // Liberacao de recursos
    // -------------------------------------------------------------------------
    glDeleteVertexArrays(1, &tileVAO);
    glDeleteBuffers(1, &tileVBO);
    glDeleteBuffers(1, &tileEBO);
    glDeleteVertexArrays(1, &markerVAO);
    glDeleteBuffers(1, &markerVBO);
    glDeleteBuffers(1, &markerEBO);
    glDeleteProgram(tileProgram);
    glDeleteProgram(markerProgram);
    glDeleteTextures(1, &tilesetTexture);
    delete tmap;
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}