#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>

const float PI = 3.14159265358979f;

// Sphere Mesh Generation: creates vertices with position, normal, UV, and tangent for a UV sphere
void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                    float radius, int sectorCount, int stackCount) {
    float sectorStep = 2 * PI / sectorCount;
    float stackStep  = PI / stackCount;

    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2 - i * stackStep;
        float xy = radius * cosf(stackAngle);
        float z  = radius * sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * sectorStep;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            // position
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            // normal
            vertices.push_back(x/radius); vertices.push_back(y/radius); vertices.push_back(z/radius);
            // UV
            vertices.push_back((float)j / sectorCount);
            vertices.push_back((float)i / stackCount);
            // tangent (dPos/dSectorAngle, normalised)
            vertices.push_back(-sinf(sectorAngle));
            vertices.push_back( cosf(sectorAngle));
            vertices.push_back(0.0f);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1), k2 = k1 + sectorCount + 1;
        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0)            { indices.push_back(k1); indices.push_back(k2); indices.push_back(k1+1); }
            if (i != stackCount-1) { indices.push_back(k1+1); indices.push_back(k2); indices.push_back(k2+1); }
        }
    }
}

// Matrix utilities: perspective projection and translation (for model matrix)
void perspective(float* m, float fov, float aspect, float n, float f) {
    float t = tanf(fov / 2.0f);
    m[0]=1/(aspect*t); m[1]=0; m[2]=0; m[3]=0;
    m[4]=0; m[5]=1/t;  m[6]=0; m[7]=0;
    m[8]=0; m[9]=0; m[10]=-(f+n)/(f-n); m[11]=-1;
    m[12]=0; m[13]=0; m[14]=-(2*f*n)/(f-n); m[15]=0;
}

void translate(float* m, float tx, float ty, float tz) {
    m[0]=1; m[1]=0; m[2]=0; m[3]=0;
    m[4]=0; m[5]=1; m[6]=0; m[7]=0;
    m[8]=0; m[9]=0; m[10]=1; m[11]=0;
    m[12]=tx; m[13]=ty; m[14]=tz; m[15]=1;
}

// Texture loading utility using stb_image.h
unsigned int loadTexture(const char* path) {
    unsigned int id;
    glGenTextures(1, &id);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return id;
}

// Camera position
const float CAM_X = 0.0f, CAM_Y = 1.8f, CAM_Z = 7.0f;


// Main
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Metal Ball Sim", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    // Shader sources: simple Blinn-Phong with normal mapping, 3 point lights, and a tint factor to darken the ball
    const char* vertSrc = R"(
        #version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;
        layout(location=3) in vec3 aTangent;

        out vec3 FragPos;
        out vec2 TexCoord;
        out mat3 TBN;

        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;

        void main(){
            FragPos  = vec3(model * vec4(aPos, 1.0));
            TexCoord = aUV;

            vec3 N = normalize(aNormal);
            vec3 T = normalize(aTangent);
            T = normalize(T - dot(T, N) * N);
            vec3 B = cross(N, T);
            TBN = mat3(T, B, N);

            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

    const char* fragSrc = R"(
        #version 330 core
        in vec3 FragPos;
        in vec2 TexCoord;
        in mat3 TBN;

        out vec4 FragColor;

        uniform sampler2D colorMap;
        uniform sampler2D normalMap;
        uniform vec3 viewPos;
        uniform float shininess;
        uniform float uvScale;
        uniform float tint;

        void main(){
            vec3 lightPos   = vec3(0.0, 5.0, 3.0);
            vec3 lightPos2  = vec3(-2.0, 2.0, 3.0);
            vec3 lightPos3  = vec3(0.0, 8.0, 0.0);
            vec3 lightColor  = vec3(0.8, 0.8, 0.8);
            vec3 lightColor2 = vec3(0.3, 0.3, 0.3);
            vec3 lightColor3 = vec3(0.3, 0.3, 0.3);

            vec3 albedo = texture(colorMap, TexCoord * uvScale).rgb * tint;

            vec3 normSample = texture(normalMap, TexCoord * uvScale).rgb * 2.0 - 1.0;
            normSample.xy *= 0.4;
            vec3 norm = normalize(TBN * normSample);

            vec3 ambient  = 0.4 * albedo;

            vec3 lightDir = normalize(lightPos - FragPos);
            float diff    = max(dot(norm, lightDir), 0.0);
            vec3 diffuse  = diff * lightColor * albedo;

            vec3 lightDir2 = normalize(lightPos2 - FragPos);
            float diff2    = max(dot(norm, lightDir2), 0.0);
            vec3 diffuse2  = diff2 * lightColor2 * albedo;

            vec3 lightDir3 = normalize(lightPos3 - FragPos);
            float diff3    = max(dot(norm, lightDir3), 0.0);
            vec3 diffuse3  = diff3 * lightColor3 * albedo;

            vec3 viewDir  = normalize(viewPos - FragPos);
            vec3 halfDir  = normalize(lightDir + viewDir);
            float spec    = pow(max(dot(norm, halfDir), 0.0), shininess);
            vec3 specular = spec * lightColor;

            FragColor = vec4(ambient + diffuse + diffuse2 + diffuse3 + specular, 1.0);
        }
    )";

    auto compileShader = [](GLenum type, const char* src) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(s, 512, NULL, log);
            std::cerr << "Shader error: " << log << "\n";
        }
        return s;
    };

    unsigned int vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    unsigned int program = glCreateProgram();
    glAttachShader(program, vert); glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert); glDeleteShader(frag);

    // Sphere VAO — generated UV sphere with position, normal, UV, and tangent
    std::vector<float> sphereVerts;
    std::vector<unsigned int> sphereIdx;
    generateSphere(sphereVerts, sphereIdx, 0.5f, 36, 18);

    const int stride = 11 * sizeof(float);

    unsigned int sphereVAO, sphereVBO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO); glGenBuffers(1, &sphereVBO); glGenBuffers(1, &sphereEBO);
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVerts.size()*sizeof(float), sphereVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIdx.size()*sizeof(unsigned int), sphereIdx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                 glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float))); glEnableVertexAttribArray(3);

    // Floor VAO — simple quad with position, normal, UV, and tangent (tangent is just +X since it's a flat plane)
    float floorVerts[] = {
        -2.0f, 0.0f, -2.0f,  0,1,0,  0,1,  1,0,0,
         2.0f, 0.0f, -2.0f,  0,1,0,  1,1,  1,0,0,
         2.0f, 0.0f,  2.0f,  0,1,0,  1,0,  1,0,0,
        -2.0f, 0.0f,  2.0f,  0,1,0,  0,0,  1,0,0,
    };
    unsigned int floorIdx[] = { 0,1,2, 0,2,3 };

    unsigned int floorVAO, floorVBO, floorEBO;
    glGenVertexArrays(1, &floorVAO); glGenBuffers(1, &floorVBO); glGenBuffers(1, &floorEBO);
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVerts), floorVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floorIdx), floorIdx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                 glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float))); glEnableVertexAttribArray(3);

    // Load textures: 3 floor types + ball texture
    unsigned int metal047Color  = loadTexture("textures/Metal047A_Color.jpg");
    unsigned int metal047Normal = loadTexture("textures/Metal047A_Normal.jpg");
    unsigned int metal038Color  = loadTexture("textures/Metal038_Color.jpg");
    unsigned int metal038Normal = loadTexture("textures/Metal038_Normal.jpg");
    unsigned int woodColor    = loadTexture("textures/Wood058_Color.jpg");
    unsigned int woodNormal   = loadTexture("textures/Wood058_Normal.jpg");
    unsigned int rubberColor  = loadTexture("textures/Rubber001_Color.jpg");
    unsigned int rubberNormal = loadTexture("textures/Rubber001_Normal.jpg");

    // Floor textures per surface preset
    unsigned int floorTex[3][2] = {
        { metal038Color,  metal038Normal  },   // Hard  (Steel)
        { woodColor,      woodNormal      },   // Medium (Wood)
        { rubberColor,    rubberNormal    },   // Soft  (Rubber)
    };

    // Projection and view matrices
    float proj[16];
    perspective(proj, PI/4.0f, 800.0f/600.0f, 0.1f, 100.0f);
    float view[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        -CAM_X, -CAM_Y, -CAM_Z, 1
    };

    // Bind texture units once
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "colorMap"),  0);
    glUniform1i(glGetUniformLocation(program, "normalMap"), 1);

    // Simulation state
    float ballX = 0.0f, ballY = 2.0f;
    float ballVX = 0.0f, ballVY = 0.0f;
    float dropHeight     = 2.0f;
    const float gravity  = -9.8f;
    const float radius   = 0.5f;
    const float floorY   = 0.0f;
    const float floorMinX = -2.0f + radius;
    const float floorMaxX =  2.0f - radius;

    int   surface  = 0;
    float dampings[]   = { 0.9f, 0.6f, 0.3f };
    float shininess[]  = { 128.0f, 32.0f, 8.0f };
    const char* surfaceNames[] = { "Hard (Steel)", "Medium (Wood)", "Soft (Rubber)" };

    int   bounceCount = 0;
    float initialKE   = 0.0f, currentEnergy = 0.0f;
    bool  firstImpact = true, ballAtRest = false;

    bool k1L=false,k2L=false,k3L=false,kRL=false;

    auto resetBall = [&]() {
        ballX = 0.0f; ballY = dropHeight;
        ballVX = 0.0f; ballVY = 0.0f;
        bounceCount = 0; firstImpact = true; ballAtRest = false;
        initialKE = 0.0f; currentEnergy = 0.0f;
    };
    resetBall();

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        // --- Input ---
        bool k1 =glfwGetKey(window,GLFW_KEY_1)==GLFW_PRESS;
        bool k2 =glfwGetKey(window,GLFW_KEY_2)==GLFW_PRESS;
        bool k3 =glfwGetKey(window,GLFW_KEY_3)==GLFW_PRESS;
        bool kR =glfwGetKey(window,GLFW_KEY_R)==GLFW_PRESS;

        if (k1&&!k1L){ surface=0; resetBall(); }
        if (k2&&!k2L){ surface=1; resetBall(); }
        if (k3&&!k3L){ surface=2; resetBall(); }
        if (kR&&!kRL) resetBall();

        if (glfwGetKey(window,GLFW_KEY_UP)  ==GLFW_PRESS) dropHeight+=0.01f;
        if (glfwGetKey(window,GLFW_KEY_DOWN)==GLFW_PRESS) dropHeight-=0.01f;
        if (dropHeight<0.6f) dropHeight=0.6f;
        if (dropHeight>4.0f) dropHeight=4.0f;

        k1L=k1; k2L=k2; k3L=k3; kRL=kR;

        // --- Physics ---
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        if (dt > 0.05f) dt = 0.05f;
        lastTime = now;

        if (!ballAtRest) {
            ballVY += gravity * dt;
            ballY  += ballVY * dt;
            ballX  += ballVX * dt;

            if (ballY - radius < floorY) {
                ballY = floorY + radius;
                if (firstImpact) {
                    initialKE = 0.5f*(ballVX*ballVX + ballVY*ballVY);
                    firstImpact = false;
                }
                bounceCount++;
                float speed = sqrtf(ballVX*ballVX + ballVY*ballVY);
                if (speed < 0.15f) {
                    ballVX=0.f; ballVY=0.f; ballAtRest=true;
                } else {
                    ballVY = -ballVY * dampings[surface];
                    ballVX =  ballVX * dampings[surface];
                }
                currentEnergy = 0.5f*(ballVX*ballVX + ballVY*ballVY);
            }
            if (ballX<floorMinX){ ballX=floorMinX; ballVX=-ballVX*dampings[surface]; }
            if (ballX>floorMaxX){ ballX=floorMaxX; ballVX=-ballVX*dampings[surface]; }
        }

        // --- HUD ---
        {
            float energyPct = (initialKE>0.f)?(currentEnergy/initialKE)*100.f:0.f;
            float speed = sqrtf(ballVX*ballVX + ballVY*ballVY);
            std::ostringstream t;
            t << "Metal Ball Sim | " << surfaceNames[surface]
              << " | Bounces: " << bounceCount
              << " | Speed: " << std::fixed << std::setprecision(2) << speed << " u/s"
              << " | Energy: " << std::fixed << std::setprecision(1) << energyPct << "%"
              << " | Height: " << std::fixed << std::setprecision(1) << dropHeight
              << " | [1/2/3] Surface  [R] Reset  [Up/Dn] Height";
            glfwSetWindowTitle(window, t.str().c_str());
        }

        // --- Render ---
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program,"projection"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(program,"view"),       1, GL_FALSE, view);
        glUniform3f(glGetUniformLocation(program,"viewPos"), CAM_X, CAM_Y, CAM_Z);

        // Floor — per-surface UV scale: steel needs more tiling, rubber less
        float floorUVScale[] = { 0.5f, 0.15f, 0.4f };  // steel, wood, rubber
        float floorModel[16]; translate(floorModel, 0,0,0);
        glUniformMatrix4fv(glGetUniformLocation(program,"model"), 1, GL_FALSE, floorModel);
        glUniform1f(glGetUniformLocation(program,"shininess"), shininess[surface]);
        glUniform1f(glGetUniformLocation(program,"uvScale"), floorUVScale[surface]);
        glUniform1f(glGetUniformLocation(program,"tint"), 1.0f);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, floorTex[surface][0]);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, floorTex[surface][1]);
        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Ball — golden texture, high shininess
        float sphereModel[16]; translate(sphereModel, ballX, ballY, 0.0f);
        glUniformMatrix4fv(glGetUniformLocation(program,"model"), 1, GL_FALSE, sphereModel);
        glUniform1f(glGetUniformLocation(program,"shininess"), 128.0f);
        glUniform1f(glGetUniformLocation(program,"uvScale"), 1.0f);
        glUniform1f(glGetUniformLocation(program,"tint"), 0.6f);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, metal047Color);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, metal047Normal);
        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)sphereIdx.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
