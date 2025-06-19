#include <iostream>
#include <string>
#include <assert.h>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Protótipo de funções:
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
GLuint loadTexture(const char* path); // Carregar textura
GLuint createQuad(); // Criar quadrado
int setupShader(); // Retornar o identificador do programa de shader

const GLuint WIDTH = 536, HEIGHT = 836;

bool keyW = false, keyA = false, keyS = false, keyD = false;

// Configuração do spritesheet
int nAnimations = 4; // número de linhas
int nFrames = 6;     // número de colunas

float ds = 1.0f / nFrames;
float dt = 1.0f / nAnimations;

// Estado atual da animação
int iAnimation = 0; // linha atual
int iFrame = 0;     // frame atual na linha

float timeSinceLastFrame = 0.0f;
float frameTime = 1.0f / 12.0f; // controla a velocidade da animação

// Código fonte do Vertex Shader
const GLchar *vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 model;

// Novos uniforms para controle do frame do spritesheet
uniform vec2 texOffset;
uniform vec2 texScale;

void main()
{
    gl_Position = projection * model * vec4(position, 1.0);
    TexCoord = texOffset + texCoord * texScale;
}
)";

// Código fonte do Fragment Shader
const GLchar *fragmentShaderSource = R"(
#version 400
in vec2 TexCoord;

out vec4 color;

uniform sampler2D ourTexture;

void main()
{
    color = texture(ourTexture, TexCoord);
}
)";

class Sprite
{
    protected:
        GLuint vao;
        GLuint textureID;
        GLuint shaderID;

        glm::vec2 position;
        glm::vec2 scale;
        float rotation;

        glm::vec2 texOffset = glm::vec2(0.0f, 0.0f);
        glm::vec2 texScale = glm::vec2(1.0f, 1.0f);

        // Animação por spritesheet
        int rows = 1, cols = 1;
        int currentFrame = 0;
        float frameTime = 0.1f; // tempo entre frames
        float frameTimer = 0.0f;

    public:
        Sprite(GLuint vao, GLuint textureID, GLuint shaderID)
            : vao(vao), textureID(textureID), shaderID(shaderID),
            position(0.0f), scale(1.0f), rotation(0.0f) {}

        void setPosition(float x, float y) {
            position = glm::vec2(x, y);
        }

        glm::vec2 getPosition() const {
            return position;
        }

        void setScale(float sx, float sy) {
            scale = glm::vec2(sx, sy);
        }

        glm::vec2 getScale() const
        {
            return scale;
        }

        void setRotation(float angleDegrees) {
            rotation = angleDegrees;
        }

        void setTexOffset(float x, float y) {
        texOffset = glm::vec2(x, y);
        }

        void setTexScale(float x, float y) {
            texScale = glm::vec2(x, y);
        }

        void setAnimation(int rows, int cols, float frameTime = 0.1f) {
            this->rows = rows;
            this->cols = cols;
            this->frameTime = frameTime;
            this->currentFrame = 0;
        }

        void updateAnimation(float deltaTime) {
            frameTimer += deltaTime;
            if (frameTimer >= frameTime) {
                frameTimer = 0.0f;
                currentFrame = (currentFrame + 1) % (rows * cols);
            }
        }

        void draw(const glm::mat4& projection)
        {
            glUseProgram(shaderID);

            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(position, 0.0f));
            model = glm::rotate(model, glm::radians(rotation), glm::vec3(0,0,1));
            model = glm::scale(model, glm::vec3(scale, 1.0f));

            GLint locModel = glGetUniformLocation(shaderID, "model");
            GLint locProjection = glGetUniformLocation(shaderID, "projection");
            GLint locTexOffset = glGetUniformLocation(shaderID, "texOffset");
            GLint locTexScale = glGetUniformLocation(shaderID, "texScale");

            glUniformMatrix4fv(locModel, 1, GL_FALSE, &model[0][0]);
            glUniformMatrix4fv(locProjection, 1, GL_FALSE, &projection[0][0]);

            // Passa as variáveis do spritesheet
            glUniform2fv(locTexOffset, 1, &texOffset[0]);
            glUniform2fv(locTexScale, 1, &texScale[0]);

            // Passa a textura para o shader
            glUniform1i(glGetUniformLocation(shaderID, "ourTexture"), 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureID);

            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            glUseProgram(0);
        }
};

class CharacterController : public Sprite
{
    private:
        float speed;

        // Fundo
        float backgroundOffsetX = 0.0f;
        float backgroundScrollSpeed = 0.1f;

        // Animação
        int frameX = 0;
        int frameY = 0;
        int maxFrames = 6;
        int totalRows = 4;
        float frameTime = 0.15f;
        float timeSinceLastFrame = 0.0f;

    public:
        CharacterController(GLuint vao, GLuint textureID, GLuint shaderID)
            : Sprite(vao, textureID, shaderID), speed(100.0f)
        {
            setTexScale(1.0f / maxFrames, 1.0f / totalRows);
        }

        // Atualizado: Recebe a parede como glm::vec4 (x, y, largura, altura)
        void updateMovement(float deltaTime, bool up, bool down, bool left, bool right, std::vector<glm::vec4>& paredes)
        {
            glm::vec2 pos = getPosition();
            glm::vec2 scale = getScale();
            glm::vec2 newPos = pos;

            float leftLimit = 100.0f;
            float rightLimit = 436.0f;

            if (down) {
                newPos.y -= speed * deltaTime;
                iAnimation = 0;
            }
            else if (up) {
                newPos.y += speed * deltaTime;
                iAnimation = 1;
            }

            if (left) {
                iAnimation = 2;
                if (pos.x > leftLimit) {
                    newPos.x -= speed * deltaTime;
                } else {
                    backgroundOffsetX -= backgroundScrollSpeed * deltaTime;
                    for (auto& parede : paredes) {
                        parede.x += backgroundScrollSpeed * deltaTime;
                    }
                }
            }
            else if (right) {
                iAnimation = 3;
                if (pos.x < rightLimit) {
                    newPos.x += speed * deltaTime;
                } else {
                    backgroundOffsetX += backgroundScrollSpeed * deltaTime;
                    for (auto& parede : paredes) {
                        parede.x -= backgroundScrollSpeed * deltaTime;
                    }
                }
            }

            // Corrige overflow do offset do fundo
            if (backgroundOffsetX >= 1.0f)
                backgroundOffsetX -= 1.0f;
            else if (backgroundOffsetX < 0.0f)
                backgroundOffsetX += 1.0f;

            for (const auto& parede : paredes)
            {
                float charLeft   = newPos.x - scale.x / 2.0f;
                float charRight  = newPos.x + scale.x / 2.0f;
                float charBottom = newPos.y - scale.y / 2.0f;
                float charTop    = newPos.y + scale.y / 2.0f;

                float wallLeft   = parede.x;
                float wallBottom = parede.y;
                float wallRight  = parede.x + parede.z;
                float wallTop    = parede.y + parede.w;

                bool collision = !(charRight < wallLeft ||
                                charLeft  > wallRight ||
                                charTop   < wallBottom ||
                                charBottom > wallTop);

                if (collision) {
                    newPos = pos;
                    break;
                }
            }

            setPosition(newPos.x, newPos.y);
        }

        void updateAnimation(float deltaTime)
        {
            timeSinceLastFrame += deltaTime;

            if (timeSinceLastFrame >= frameTime) {
                timeSinceLastFrame = 0.0f;
                iFrame = (iFrame + 1) % nFrames;

                float offsetS = iFrame * ds;
                float offsetT = 1.0f - (iAnimation + 1) * dt;
                setTexOffset(offsetS, offsetT);
            }
        }

        float getBackgroundOffsetX() const
        {
            return backgroundOffsetX;
        }
};

int main()
{
    // Inicializa GLFW
    if (!glfwInit())
    {
        cout << "Falha ao inicializar GLFW" << endl;
        return -1;
    }

    // Configura a versão do OpenGL (4.0)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // Cria a janela
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vampiro no Esgoto", nullptr, nullptr);
    if (!window)
    {
        cout << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Inicializa GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    // Define o viewport
    glViewport(0, 0, WIDTH, HEIGHT);

    // Registra callback do teclado
    glfwSetKeyCallback(window, key_callback);

    // Compila e linka shader
    GLuint shaderProgram = setupShader();

    // Cria geometria do quad
    GLuint quadVAO = createQuad();

    // Carrega texturas
    GLuint backgroundTex = loadTexture("../assets/sprites/esgoto.jpg");
    GLuint vampiroTex = loadTexture("../assets/sprites/Vampires3_Walk_full.png");

    // Cria sprites
    Sprite background(quadVAO, backgroundTex, shaderProgram);
    CharacterController vampiro(quadVAO, vampiroTex, shaderProgram);

    // Define projeção ortográfica
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(WIDTH),
                                      0.0f, static_cast<float>(HEIGHT),
                                      -1.0f, 1.0f);

    // Ajusta background e personagem
    background.setPosition(WIDTH / 2.0f, HEIGHT / 2.0f);
    background.setScale(WIDTH, HEIGHT);

    vampiro.setPosition(WIDTH / 2.0f, HEIGHT / 2.0f);
    vampiro.setScale(145.0f, 145.0f); // Ajuste o tamanho conforme necessário

    // Ativar blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Controle de tempo para deltaTime
    float lastFrameTime = 0.0f;

    std::vector<glm::vec4> paredes;
    paredes.emplace_back(0.0f, 550.0f, WIDTH, HEIGHT - 425.0f);
    paredes.emplace_back(0.0f, 0.0f, WIDTH, 165.0f);

    while (!glfwWindowShouldClose(window))
    {
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // Atualiza estado das teclas
        bool keyW = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool keyS = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool keyA = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool keyD = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

        background.setTexOffset(vampiro.getBackgroundOffsetX(), 0.0f);

        // Limpa tela
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Desenha background com offset atualizado
        background.draw(projection);

        // Atualiza movimento e animação do personagem
        vampiro.updateMovement(deltaTime, keyW, keyS, keyA, keyD, paredes);
        vampiro.updateAnimation(deltaTime);

        // Desenha personagem
        vampiro.draw(projection);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    // Fechar janela ao pressionar ESC
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    // Atualizar flags ao pressionar ou soltar teclas WASD e setas
    bool isPressed = (action == GLFW_PRESS || action == GLFW_REPEAT);
    bool isReleased = (action == GLFW_RELEASE);

    switch (key)
    {
        case GLFW_KEY_W:
        case GLFW_KEY_UP:
            if (isPressed) keyW = true;
            if (isReleased) keyW = false;
            break;

        case GLFW_KEY_A:
        case GLFW_KEY_LEFT:
            if (isPressed) keyA = true;
            if (isReleased) keyA = false;
            break;

        case GLFW_KEY_S:
        case GLFW_KEY_DOWN:
            if (isPressed) keyS = true;
            if (isReleased) keyS = false;
            break;

        case GLFW_KEY_D:
        case GLFW_KEY_RIGHT:
            if (isPressed) keyD = true;
            if (isReleased) keyD = false;
            break;

        default:
            break;
    }
}

int setupShader()
{
	// Vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	// Checando erros de compilação (exibição via log no terminal)
	GLint success;
	GLchar infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
				  << infoLog << std::endl;
	}
	// Fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
    
	// Checando erros de compilação (exibição via log no terminal)
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
				  << infoLog << std::endl;
	}
	// Linkando os shaders e criando o identificador do programa de shader
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
    
	// Checando por erros de linkagem
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
				  << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

GLuint createQuad()
{
    GLuint VAO, VBO, EBO;

    // Vertices: posição (x,y,z) + textura (s,t)
    GLfloat vertices[] = {
        // positions        // texture coords
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  // top-left
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // bottom-left
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // bottom-right
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f   // top-right
    };

    GLuint indices[] = {
        0, 1, 2,  // first triangle
        0, 2, 3   // second triangle
    };

    // Gera buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind VAO
    glBindVertexArray(VAO);

    // VBO - vertices
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO - indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Posição - layout location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    // Textura - layout location 1
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Desvincula VAO (mas mantém EBO vinculado ao VAO!)
    glBindVertexArray(0);

    // Desvincula buffers para evitar bugs
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return VAO;
}

GLuint loadTexture(const char* path)
{
    GLuint textureID;
    glGenTextures(1, &textureID);

    stbi_set_flip_vertically_on_load(true); // Inverte verticalmente a imagem

    int width, height, nrChannels;
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // **Alteração para permitir repetição no wrap**
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Erro ao carregar textura: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}