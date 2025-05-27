#include <iostream>
#include <string>
#include <assert.h>
#include <vector>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;

#include <cmath>
#include <ctime>

// Protótipo de funções callback:
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode); // --> Fechar janela com tecla ESC
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods); // --> Selecionar quadrado na tela

// Protótipos de funções:
GLuint createQuad(); // --> Criar quadrado
int setupShader(); // --> Retornar o identificador do programa de shader
void eliminarSimilares(float tolerancia); // --> Apagar quadrados semelhantes ao selecionado
bool existeParSimilar(float tolerancia);

// Varáveis constantes que determinam tamanho da janela, grade e quadrados
const GLuint WIDTH = 800, HEIGHT = 600;
const GLuint ROWS = 6, COLS = 8;
const GLuint QUAD_WIDTH = 100, QUAD_HEIGHT = 100;

const float dMax = sqrt(3.0); // --> Maior distência possível entre 2 cores

// Código fonte do Vertex Shader (em GLSL): ainda hardcoded
const GLchar *vertexShaderSource = R"(
#version 400
layout (location = 0) in vec3 position;
uniform mat4 projection;
uniform mat4 model;
void main()	
{
	//...pode ter mais linhas de código aqui!
	gl_Position = projection * model * vec4(position.x, position.y, position.z, 1.0);
}
)";

// Código fonte do Fragment Shader (em GLSL): ainda hardcoded
const GLchar *fragmentShaderSource = R"(
#version 400
uniform vec4 inputColor;
out vec4 color;
void main()
{
	color = inputColor;
}
)";

struct Quad
{
	vec3 position;
	vec3 dimensions;
	vec3 color;
	bool eliminated;
};

int iColor = 0;
//int iSelected = -1; 
int selectedCount = 0; // --> Verificar quantos quadrados foram selecionados
int selectedIndices[2]; // --> Armazenar os quadrados selecionados

int score = 0;

Quad grid[ROWS][COLS]; // --> Criar grade de quadrados

int main()
{
    /*  Ela define a "semente" (seed) que o gerador vai usar para criar números aleatórios
        Como o tempo está sempre mudando, a semente também muda a cada execução do programa
        Isso faz com que os números aleatórios gerados sejam diferentes em cada execução */
    srand(time(0));

    glfwInit();
    // Criação da janela GLFW
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Jogo das cores", nullptr, nullptr);
	glfwMakeContextCurrent(window);

    // Fazendo o registro da função de callback para a janela GLFW
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);

    // GLAD: carrega todos os ponteiros d funções da OpenGL
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
	}

	// Obtendo as informações de versão
	const GLubyte *renderer = glGetString(GL_RENDERER); /* get renderer string */
	const GLubyte *version = glGetString(GL_VERSION);	/* version as a string */
	cout << "Renderer: " << renderer << endl;
	cout << "OpenGL version supported " << version << endl;

    // Definindo as dimensões da viewport com as mesmas dimensões da janela da aplicação
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

    // Compilando e buildando o programa de shader
	GLuint shaderID = setupShader();

	GLuint VAO = createQuad();

    // Inicializar a grid
	for (int i = 0; i < ROWS; i++)
	{
		for (int j = 0; j < COLS; j++)
		{
			Quad quad;
			vec2 ini_pos = vec2(QUAD_WIDTH / 2, QUAD_HEIGHT / 2);
			quad.position = vec3(ini_pos.x + j * QUAD_WIDTH, ini_pos.y + i * QUAD_HEIGHT, 0.0);
			quad.dimensions = vec3(QUAD_WIDTH, QUAD_HEIGHT, 1.0);
			float r, g, b;
			r = rand() % 256 / 255.0;
			g = rand() % 256 / 255.0;
			b = rand() % 256 / 255.0;
			quad.color = vec3(r, g, b);
			quad.eliminated = false;
			grid[i][j] = quad;
		}
	}
    glUseProgram(shaderID);
	cout << "Pontuacao: " << score << endl;

    // Enviando a cor desejada (vec4) para o fragment shader
    GLint colorLoc = glGetUniformLocation(shaderID, "inputColor");

    // Matriz de projeção paralela ortográfica
    mat4 projection = ortho(0.0, 800.0, 600.0, 0.0, -1.0, 1.0);
	glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    // Loop da aplicação - "game loop"
	while (!glfwWindowShouldClose(window))
    {
        // Checa se houveram eventos de input (key pressed, mouse moved etc.) e chama as funções de callback correspondentes
		glfwPollEvents();

        // Limpa o buffer de cor
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // cor de fundo
		glClear(GL_COLOR_BUFFER_BIT);

		glLineWidth(10);
		glPointSize(20);

        glBindVertexArray(VAO); // Conectando ao buffer de geometria

		// if (iSelected > -1)
        if (selectedCount == 2)
		{
    		eliminarSimilares(0.2);
    		selectedCount = 0;

    		if (!existeParSimilar(0.2))
    		{
        		cout << "Fim de jogo! Nenhum par semelhante restante.\nPontuacao Final: "<< score <<". Parabens!\n";
        		break;
    		}
		}

        for (int i = 0; i < ROWS; i++)
		{
			for (int j = 0; j < COLS; j++)
			{
				if (!grid[i][j].eliminated)
				{
					// Matriz de modelo: transformações na geometria (objeto)
					mat4 model = mat4(1); // matriz identidade
					// Translação
					model = translate(model, grid[i][j].position);
					//  Escala
					model = scale(model, grid[i][j].dimensions);
					glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));
					glUniform4f(colorLoc, grid[i][j].color.r, grid[i][j].color.g, grid[i][j].color.b, 1.0f); // enviando cor para variável uniform inputColor
					// Chamada de desenho - drawcall
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
				}
			}
		}
        glBindVertexArray(0); // Desconectando o buffer de geometria
		glfwSwapBuffers(window); // Troca os buffers da tela
    }
    // Finaliza a execução da GLFW, limpando os recursos alocados por ela
	glfwTerminate();
	return 0;
}

// Retorna o identificador do programa de shader
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

// Se a tecla ESC for apertada, janela deve fechar
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

/*  Se mouse clicar, guarda a posição do cursor, verifica qual quadrado corresponde às coordenadas e o elimina

    | Selected = x + y * COLS |

    Transforma a coordenada 2D (x, y) em um número único iSelected,
    que é a posição daquele quadrado dentro do vetor (lista) que guarda todos os quadrados */
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
        //cout << xpos << "  " << ypos << " ----- ";
		//cout << xpos / QUAD_WIDTH << " " << ypos / QUAD_HEIGHT << endl;
        int x = xpos / QUAD_WIDTH;
		int y = ypos / QUAD_HEIGHT;
        //grid[y][x].eliminated = true;
        //iSelected = x + y * COLS;

		if (x >= 0 && x < COLS && y >= 0 && y < ROWS)
        {
            int index = x + y * COLS;

            if (selectedCount < 2)
            {
                selectedIndices[selectedCount] = index;
                selectedCount++;
            }
        }
    }
}

// Cria quadrados, mesma lógica do createTriangle(), mas com um vértice a mais
GLuint createQuad()
{
    GLuint VAO, VBO;
    GLfloat vertices[] = {
        -0.5, 0.5, 0.0,	 // v0
		-0.5, -0.5, 0.0, // v1
		0.5, 0.5, 0.0,	 // v2
		0.5, -0.5, 0.0	 // v3
    };

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}

/*  Verifica a cor do quadrado selecionado e
    varre o vetor de quadrados procurando por aqueles com cores semelhantes:
    float d = sqrt(pow(C.r-O.r,2) + pow(C.g-O.g,2) + pow(C.b-O.b,2)) --> cálculo da tolerância de cor

    Remove quadrado selecionado e semelhantes */
void eliminarSimilares(float tolerancia)
{	
	int idx1 = selectedIndices[0];
    int idx2 = selectedIndices[1];

	if (idx1 == idx2) {
        cout << "Mesmo quadrado selecionado duas vezes." << endl;
        // Resetar seleção para evitar loop infinito
        selectedCount = 0;
        selectedIndices[0] = -1;
        selectedIndices[1] = -1;
        return;
    }

	int x1 = idx1 % COLS, y1 = idx1 / COLS;
    int x2 = idx2 % COLS, y2 = idx2 / COLS;

	vec3 C1 = grid[y1][x1].color;
    vec3 C2 = grid[y2][x2].color;

	float d = sqrt(pow(C1.r - C2.r, 2) + pow(C1.g - C2.g, 2) + pow(C1.b - C2.b, 2));
    float dd = d / dMax;

    if (dd <= tolerancia)
    {
        grid[y1][x1].eliminated = true;
        grid[y2][x2].eliminated = true;
        cout << "Cores parecidas! Eliminadas.\n";
		cout << "Pontuacao: " << ++score << endl;
    }
    else
    {
		if(score == 0){
			cout << "Pontuacao: " << score << endl;
		}
		else{
			cout << "Cores diferentes!\n";
			cout << "Pontuacao: " << --score << endl;
		}
    }

    selectedCount = 0; // limpa para a próxima comparação
}

/*	Verificar se ainda existe pelo menos um par de quadrados com cores semelhantes o suficiente (dentro da tolerância)
	Percorre toda a grade, seleciona primeiro quadrado e verifica os seguintes, fazendo isso consecutivamente
	Se existir par semelhante, o jogo continua
	Se não existir, o jogo acabou */
bool existeParSimilar(float tolerancia)
{
	// Percorre todos os quadrados da grade
    for (int i = 0; i < ROWS * COLS; i++)
    {
		// Converte o índice linear "i" para coordenadas 2D
        int x1 = i % COLS, y1 = i / COLS;
		// Se esse quadrado já foi eliminado, pula ele
        if (grid[y1][x1].eliminated) continue;

		// Compara esse quadrado com todos os seguintes
        for (int j = i + 1; j < ROWS * COLS; j++)
        {
            int x2 = j % COLS, y2 = j / COLS;
			// Se já foi eliminado, pula ele
            if (grid[y2][x2].eliminated) continue;

            vec3 cor1 = grid[y1][x1].color;
            vec3 cor2 = grid[y2][x2].color;

			// Calcula a distância entre as cores do quadrado "i" com o quadrado "j"
            float d = sqrt(pow(cor1.r - cor2.r, 2) + pow(cor1.g - cor2.g, 2) + pow(cor1.b - cor2.b, 2));
            float dd = d / dMax;

            if (dd <= tolerancia)
                return true; // Ainda existe par parecido
        }
    }
    return false; // Nenhum par parecido encontrado
}