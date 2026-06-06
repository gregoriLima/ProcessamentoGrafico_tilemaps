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

#include "DiamondView.h"
#include "TileMap.h"

using namespace std;

// tamanho da janela
int g_gl_width = 800;
int g_gl_height = 400;
GLFWwindow *g_window = NULL;

// limites do espaco que o mapa vai ocupar no NDC (-1 a 1)
float xi = -1.0f;
float xf = 1.0f;
float yi = -1.0f;
float yf = 1.0f;
float w = xf - xi;
float h = yf - yi;

// tamanho de cada tile no espaco do mundo
float tw = 0.0f;
float th = 0.0f;

// o tileset tem 9 colunas e 9 linhas de tiles
const int TILESET_COLS = 9;
const int TILESET_ROWS = 9;

// fracao de UV que cada tile ocupa na textura
float tileW  = 0.0f;
float tileH  = 0.0f;
float tileW2 = 0.0f;
float tileH2 = 0.0f;

// vista isometrica diamond e o mapa
DiamondView tview(xi, yi);
TileMap *tmap = NULL;

// guarda se cada tecla ja estava pressionada no frame anterior
// serve pra mover so um passo por pressionamento
struct KeyLatch {
    bool up    = false;
    bool down  = false;
    bool left  = false;
    bool right = false;
};

// descobre a pasta onde o .cpp esta, usando o caminho do proprio arquivo
string sourceDirectory() {
    string filePath = __FILE__;
    const size_t separator = filePath.find_last_of("\\/");
    if (separator == string::npos) {
        return string();
    }
    return filePath.substr(0, separator + 1);
}

// monta o caminho completo de um asset relativo a pasta do codigo
string sourceAssetPath(const char *relativePath) {
    return sourceDirectory() + relativePath;
}

// abre a janela e liga o OpenGL
bool initialiseOpenGL() {
    if (!glfwInit()) {
        printf("Falha ao inicializar GLFW.\n");
        return false;
    }

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
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Falha ao inicializar GLAD.\n");
        glfwDestroyWindow(g_window);
        g_window = NULL;
        glfwTerminate();
        return false;
    }

    return true;
}

// compila um shader e devolve o id; se der erro mostra a mensagem
GLuint compileShader(GLenum shaderType, const char *shaderSource) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    string shaderLog(static_cast<size_t>(logLength), '\0');
    glGetShaderInfoLog(shader, logLength, NULL, shaderLog.data());
    printf("Falha ao compilar shader: %s\n", shaderLog.c_str());
    glDeleteShader(shader);
    return 0;
}

// junta vertex e fragment shader num programa e devolve o id
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

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// carrega uma imagem do disco e manda ela pra GPU como textura
bool loadTexture(GLuint &texture, const string &filename) {
    int imageWidth  = 0;
    int imageHeight = 0;
    int channels    = 0;
    unsigned char *data = stbi_load(filename.c_str(), &imageWidth, &imageHeight, &channels, 0);
    if (data == NULL) {
        printf("Falha ao carregar textura: %s\n", filename.c_str());
        return false;
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    const GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, imageWidth, imageHeight, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return true;
}

// checa se uma posicao esta dentro dos limites do mapa
bool isInsideMap(int col, int row) {
    return tmap != NULL
        && col >= 0 && col < tmap->getWidth()
        && row >= 0 && row < tmap->getHeight();
}

// tenta mover o cursor numa direcao; se sair do mapa nao faz nada
bool moveIfValid(int &col, int &row, int direction) {
    int nextCol = col;
    int nextRow = row;
    tview.computeTileWalking(nextCol, nextRow, direction);
    if (!isInsideMap(nextCol, nextRow)) {
        return false;
    }
    col = nextCol;
    row = nextRow;
    return true;
}

// move somente quando a tecla acabou de ser pressionada, nao enquanto segura
void processMovementKey(int key, bool &wasPressed, int direction, int &col, int &row) {
    const bool isPressed = glfwGetKey(g_window, key) == GLFW_PRESS;
    if (isPressed && !wasPressed) {
        moveIfValid(col, row, direction);
    }
    wasPressed = isPressed;
}

// desenha um cubinho isometrico em cima de um tile para marcar a posicao atual
// as tres faces tem brilhos diferentes pra parecer 3D
void drawMarker(GLuint markerProgram, GLuint markerVAO, int col, int row, float red, float green, float blue) {
    float tileX = 0.0f;
    float tileY = 0.0f;
    tview.computeDrawPosition(col, row, tw, th, tileX, tileY);

    glUseProgram(markerProgram);
    glBindVertexArray(markerVAO);
    // centraliza o cubo no meio do tile
    glUniform2f(glGetUniformLocation(markerProgram, "offset"), tileX + tw * 0.5f, tileY + th * 0.5f);

    // topo: cor original
    glUniform4f(glGetUniformLocation(markerProgram, "color"), red, green, blue, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);

    // face esquerda: mais escura
    glUniform4f(glGetUniformLocation(markerProgram, "color"), red * 0.6f, green * 0.6f, blue * 0.6f, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(unsigned int)));

    // face direita: mais escura ainda
    glUniform4f(glGetUniformLocation(markerProgram, "color"), red * 0.4f, green * 0.4f, blue * 0.4f, 1.0f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(12 * sizeof(unsigned int)));
}

int main() {
    if (!initialiseOpenGL()) {
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // mapa hardcoded: cada numero e o id do tile no tileset
    // linha 0 e a linha de cima da tela
    const int MAP_COLS = 5;
    const int MAP_ROWS = 5;
    int mapData[MAP_ROWS][MAP_COLS] = {
        {  1,  1,  4,  1,  1 },
        {  4,  1,  4,  1,  4 },
        {  1,  4,  0,  4,  1 },
        {  4,  1,  4,  1,  4 },
        {  1,  1,  4,  1,  1 },
    };

    // cria o TileMap e preenche com os dados do array
    // invertemos a linha porque o eixo Y do OpenGL cresce pra cima,
    // mas no array a linha 0 representa o topo da tela
    tmap = new TileMap(MAP_COLS, MAP_ROWS, 0);
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            tmap->setTile(c, MAP_ROWS - r - 1, (unsigned char)mapData[r][c]);
        }
    }

    // calcula o tamanho de cada tile com base no tamanho do mapa
    tw = w / (float)tmap->getWidth();
    th = tw / 2.0f; // proporcao isometrica 2:1
    tileW  = 1.0f / (float)TILESET_COLS;
    tileH  = 1.0f / (float)TILESET_ROWS;
    tileW2 = tileW / 2.0f;
    tileH2 = tileH / 2.0f;
    tview.setMapSize(tmap->getWidth(), tmap->getHeight());

    // carrega o tileset
    const string texturePath = sourceAssetPath("exemplo/terrain.png");
    GLuint tilesetTexture = 0;
    if (!loadTexture(tilesetTexture, texturePath)) {
        delete tmap;
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return 1;
    }
    tmap->setTid(tilesetTexture);

    // shader do tilemap: posiciona o losango no lugar certo e mostra o tile da textura
    const char *tileVertexShader =
        "#version 330 core\n"
        "layout (location = 0) in vec2 vertex_position;\n"
        "layout (location = 1) in vec2 texture_mapping;\n"
        "out vec2 texture_coords;\n"
        "uniform float layer_z;\n"
        "uniform float tx;\n"
        "uniform float ty;\n"
        "void main() {\n"
        "  texture_coords = texture_mapping;\n"
        "  float raw_y = vertex_position.y + ty;\n"
        "  gl_Position = vec4(vertex_position.x + tx, raw_y * 2.0 + 1.0, layer_z, 1.0);\n"
        "}\n";

    const char *tileFragmentShader =
        "#version 330 core\n"
        "in vec2 texture_coords;\n"
        "uniform sampler2D sprite;\n"
        "uniform float offsetx;\n"
        "uniform float offsety;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  vec4 texel = texture(sprite, vec2(texture_coords.x + offsetx, texture_coords.y + offsety));\n"
        "  if (texel.a < 0.1) {\n"
        "    discard;\n"
        "  }\n"
        "  frag_color = texel;\n"
        "}\n";

    // shader do marcador: posiciona e pinta com cor solida
    const char *markerVertexShader =
        "#version 330 core\n"
        "layout (location = 0) in vec2 vertex_position;\n"
        "uniform vec2 offset;\n"
        "void main() {\n"
        "  float raw_y = vertex_position.y + offset.y;\n"
        "  gl_Position = vec4(vertex_position.x + offset.x, raw_y * 2.0 + 1.0, 0.1, 1.0);\n"
        "}\n";

    const char *markerFragmentShader =
        "#version 330 core\n"
        "uniform vec4 color;\n"
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

    // forma de losango de cada tile (4 pontos: cima, direita, baixo, esquerda)
    // cada vertice: posicao XY + coordenada UV pra textura
    const float tileVertices[] = {
        0.0f,      th * 0.5f, 0.0f,   tileH2,   // cima
        tw * 0.5f, 0.0f,      tileW2, 0.0f,      // direita
        tw,        th * 0.5f, tileW,  tileH2,    // baixo
        tw * 0.5f, th,        tileW2, tileH,     // esquerda
    };
    // dois triangulos formam o losango
    const unsigned int tileIndices[] = { 0, 1, 3,  3, 1, 2 };

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
    // atributo 0: posicao xy (2 floats)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // atributo 1: coordenada uv (2 floats)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // geometria do cubinho isometrico com 7 pontos
    // topo e um losango; faces laterais descem pra dar volume
    const float hs = tw * 0.25f; // meia largura do topo
    const float vs = hs * 0.5f;  // meia altura do topo
    const float ch = vs;          // altura das faces laterais
    const float cubeVertices[] = {
         0.0f,        vs,   // 0: ponta de cima
         hs,        0.0f,   // 1: ponta direita
         0.0f,       -vs,   // 2: ponta de baixo
        -hs,        0.0f,   // 3: ponta esquerda
         0.0f, -vs - ch,   // 4: base central
        -hs,        -ch,   // 5: base esquerda
         hs,        -ch,   // 6: base direita
    };
    // topo + face esquerda + face direita, cada face = 2 triangulos
    const unsigned int cubeIndices[] = {
        0, 1, 2,  0, 2, 3,   // topo
        3, 2, 4,  3, 4, 5,   // face esquerda
        1, 2, 4,  1, 4, 6,   // face direita
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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // posicao inicial do cursor no mapa
    int cursor_c = 0;
    int cursor_r = 0;

    KeyLatch keyLatch;

    while (!glfwWindowShouldClose(g_window)) {
        glfwPollEvents();

        if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(g_window, 1);
        }

        // movimento do cursor com as setas (4 direcoes por enquanto)
        processMovementKey(GLFW_KEY_UP,    keyLatch.up,    DIRECTION_SOUTH, cursor_c, cursor_r);
        processMovementKey(GLFW_KEY_DOWN,  keyLatch.down,  DIRECTION_NORTH, cursor_c, cursor_r);
        processMovementKey(GLFW_KEY_LEFT,  keyLatch.left,  DIRECTION_WEST,  cursor_c, cursor_r);
        processMovementKey(GLFW_KEY_RIGHT, keyLatch.right, DIRECTION_EAST,  cursor_c, cursor_r);

        glViewport(0, 0, g_gl_width, g_gl_height);
        glClearColor(0.12f, 0.18f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // desenha o mapa tile por tile
        glUseProgram(tileProgram);
        glBindVertexArray(tileVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tmap->getTileSet());
        glUniform1i(glGetUniformLocation(tileProgram, "sprite"), 0);

        for (int r = 0; r < tmap->getHeight(); r++) {
            for (int c = 0; c < tmap->getWidth(); c++) {
                // descobre qual tile do tileset usar
                int tileId = tmap->getTile(c, r);
                int u = tileId % TILESET_COLS; // coluna no tileset
                int v = tileId / TILESET_COLS; // linha no tileset

                // calcula a posicao na tela
                float tileX = 0.0f;
                float tileY = 0.0f;
                tview.computeDrawPosition(c, r, tw, th, tileX, tileY);

                // envia pra GPU: offset UV do tile no tileset e posicao na tela
                glUniform1f(glGetUniformLocation(tileProgram, "offsetx"), u * tileW);
                glUniform1f(glGetUniformLocation(tileProgram, "offsety"), v * tileH);
                glUniform1f(glGetUniformLocation(tileProgram, "tx"), tileX);
                glUniform1f(glGetUniformLocation(tileProgram, "ty"), tileY);
                glUniform1f(glGetUniformLocation(tileProgram, "layer_z"), tmap->getZ());
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }

        // desliga o depth test pra o marcador aparecer sempre em cima do mapa
        glDisable(GL_DEPTH_TEST);
        drawMarker(markerProgram, markerVAO, cursor_c, cursor_r, 0.0f, 0.6f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(g_window);
    }

    // libera recursos
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
