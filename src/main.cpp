#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>

const float PI = 3.14159265358979f;

void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices, float radius, int sectorCount, int stackCount) {
    float x, y, z, xy;
    float sectorStep = 2 * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;
    for (int i = 0; i <= stackCount; ++i) {
        stackAngle = PI / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);
        for (int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            vertices.push_back(x/radius); vertices.push_back(y/radius); vertices.push_back(z/radius);
        }
    }
    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1), k2 = k1 + sectorCount + 1;
        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) { indices.push_back(k1); indices.push_back(k2); indices.push_back(k1+1); }
            if (i != stackCount-1) { indices.push_back(k1+1); indices.push_back(k2); indices.push_back(k2+1); }
        }
    }
}

void perspective(float* m, float fov, float aspect, float n, float f) {
    float t = tanf(fov / 2.0f);
    m[0]=1/(aspect*t); m[1]=0; m[2]=0; m[3]=0;
    m[4]=0; m[5]=1/t; m[6]=0; m[7]=0;
    m[8]=0; m[9]=0; m[10]=-(f+n)/(f-n); m[11]=-1;
    m[12]=0; m[13]=0; m[14]=-(2*f*n)/(f-n); m[15]=0;
}

void translate(float* m, float tx, float ty, float tz) {
    m[0]=1; m[1]=0; m[2]=0; m[3]=0;
    m[4]=0; m[5]=1; m[6]=0; m[7]=0;
    m[8]=0; m[9]=0; m[10]=1; m[11]=0;
    m[12]=tx; m[13]=ty; m[14]=tz; m[15]=1;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Metal Ball Sim", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    const char* vertSrc = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        out vec3 Normal;
        out vec3 FragPos;
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        void main(){
            Normal = aNormal;
            FragPos = vec3(model * vec4(aPos, 1.0));
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

    const char* fragSrc = R"(
        #version 330 core
        in vec3 Normal;
        in vec3 FragPos;
        out vec4 FragColor;
        uniform vec3 color;
        void main(){
            vec3 ld = normalize(vec3(1.0, 2.0, 3.0));
            float d = max(dot(normalize(Normal), ld), 0.0);
            vec3 c = (0.2 + 0.8*d) * color;
            FragColor = vec4(c, 1.0);
        }
    )";

    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, NULL); glCompileShader(vert);
    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragSrc, NULL); glCompileShader(frag);
    unsigned int program = glCreateProgram();
    glAttachShader(program, vert); glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert); glDeleteShader(frag);

    std::vector<float> sphereVerts;
    std::vector<unsigned int> sphereIdx;
    generateSphere(sphereVerts, sphereIdx, 0.5f, 36, 18);

    unsigned int sphereVAO, sphereVBO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO); glGenBuffers(1, &sphereVBO); glGenBuffers(1, &sphereEBO);
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVerts.size()*sizeof(float), sphereVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIdx.size()*sizeof(unsigned int), sphereIdx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    float floorVerts[] = {
        -2.0f, 0.0f, -2.0f,  0.0f, 1.0f, 0.0f,
         2.0f, 0.0f, -2.0f,  0.0f, 1.0f, 0.0f,
         2.0f, 0.0f,  2.0f,  0.0f, 1.0f, 0.0f,
        -2.0f, 0.0f,  2.0f,  0.0f, 1.0f, 0.0f,
    };
    unsigned int floorIdx[] = { 0,1,2, 0,2,3 };

    unsigned int floorVAO, floorVBO, floorEBO;
    glGenVertexArrays(1, &floorVAO); glGenBuffers(1, &floorVBO); glGenBuffers(1, &floorEBO);
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVerts), floorVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floorIdx), floorIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    float proj[16];
    perspective(proj, PI/4.0f, 800.0f/600.0f, 0.1f, 100.0f);
     float view[16] = {
        1, 0, 0, 0,
        0, 0.7f, -0.7f, 0,
        0, 0.7f,  0.7f, 0,
        0, -2.0f, -5.0f, 1
    };

    float ballY = 1.5f;
    float ballVY = 0.0f;
    float gravity = -9.8f;
    float damping = 0.7f;
    float floorY = -0.0f;
    float radius = 0.5f;
    double lastTime = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        ballVY += gravity * dt;
        ballY += ballVY * dt;

        if (ballY - radius < floorY) {
            ballY = floorY + radius;
            ballVY = -ballVY * damping;
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, view);

        float floorModel[16];
        translate(floorModel, 0.0f, -0.5f, 0.0f);
        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, floorModel);
        glUniform3f(glGetUniformLocation(program, "color"), 0.4f, 0.6f, 0.4f);
        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        float sphereModel[16];
        translate(sphereModel, 0.0f, ballY, 0.0f);
        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, sphereModel);
        glUniform3f(glGetUniformLocation(program, "color"), 0.7f, 0.7f, 0.85f);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, sphereIdx.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
    return 0;
}