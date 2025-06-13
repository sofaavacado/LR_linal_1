#define _USE_MATH_DEFINES
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <algorithm>

#define M_PI 3.14159265358979323846

const int WIDTH = 800;
const int HEIGHT = 600;

// Структура для 3D точки
struct Vec3 {
    float x, y, z;
    Vec3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 normalize() const {
        float length = sqrt(x * x + y * y + z * z);
        if (length == 0) return Vec3(0, 0, 0);
        return Vec3(x / length, y / length, z / length);
    }
};

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// Матрица 4x4 для преобразований
struct Matrix4 {
    float m[4][4];
    Matrix4() {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    // Умножение матрицы на вектор
    Vec3 multiply(const Vec3& v) const {
        float x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3];
        float y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3];
        float z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3];
        return Vec3(x, y, z);
    }

    // Умножение матриц
    Matrix4 operator*(const Matrix4& other) const {
        Matrix4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; ++k) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }
};

// Создание матрицы вращения вокруг оси X
Matrix4 createRotationX(float angle) {
    float rad = angle * static_cast<float>(M_PI) / 180.0f;
    float c = cos(rad);
    float s = sin(rad);
    Matrix4 mat;
    mat.m[1][1] = c;
    mat.m[1][2] = -s;
    mat.m[2][1] = s;
    mat.m[2][2] = c;
    return mat;
}

// Создание матрицы вращения вокруг оси Y
Matrix4 createRotationY(float angle) {
    float rad = angle * static_cast<float>(M_PI) / 180.0f;
    float c = cos(rad);
    float s = sin(rad);
    Matrix4 mat;
    mat.m[0][0] = c;
    mat.m[0][2] = s;
    mat.m[2][0] = -s;
    mat.m[2][2] = c;
    return mat;
}

// Создание матрицы трансляции
Matrix4 createTranslation(float x, float y, float z) {
    Matrix4 mat;
    mat.m[0][3] = x;
    mat.m[1][3] = y;
    mat.m[2][3] = z;
    return mat;
}

// Класс для параметрических поверхностей
class ParametricSurface {
protected:
    float uMin, uMax, vMin, vMax;
    int uSteps, vSteps;
    std::vector<float> parameters;

public:
    ParametricSurface(float uMin_, float uMax_, float vMin_, float vMax_, int uSteps_, int vSteps_)
        : uMin(uMin_), uMax(uMax_), vMin(vMin_), vMax(vMax_), uSteps(uSteps_), vSteps(vSteps_) {
        parameters = { 1.0f, 0.5f };
    }

    virtual ~ParametricSurface() {}

    virtual Vec3 computePoint(float u, float v) const = 0;

    float getParameter(int index) const { return parameters[index]; }
    void setParameter(int index, float value) { parameters[index] = value; }

    float getUMin() const { return uMin; }
    float getUMax() const { return uMax; }
    float getVMin() const { return vMin; }
    float getVMax() const { return vMax; }
    int getUSteps() const { return uSteps; }
    int getVSteps() const { return vSteps; }
};

// Класс для ленты Мёбиуса
class MoebiusStrip : public ParametricSurface {
public:
    MoebiusStrip(float uMin_, float uMax_, float vMin_, float vMax_, int uSteps_, int vSteps_)
        : ParametricSurface(uMin_, uMax_, vMin_, vMax_, uSteps_, vSteps_) {
        parameters[0] = 1.0f;
        parameters[1] = 0.5f;
    }

    Vec3 computePoint(float u, float v) const override {
        float alpha = parameters[0];
        float beta = parameters[1];
        float x = (alpha + v * cos(u / 2.0f)) * cos(u);
        float y = (alpha + v * cos(u / 2.0f)) * sin(u);
        float z = beta * v * sin(u / 2.0f);
        return Vec3(x, y, z);
    }
};

// Движок для отрисовки поверхностей
class SurfaceEngine {
private:
    ParametricSurface* surface;
    std::vector<Vec3> points;
    std::vector<std::vector<int>> triangles;
    std::vector<Vec3> normals;
    float angleX, angleY;

    struct ParameterAnimation {
        float value;
        float minVal, maxVal, step;
        float direction;
        ParameterAnimation(float min_, float max_, float step_)
            : value(min_), minVal(min_), maxVal(max_), step(step_), direction(1.0f) {}
    };
    std::vector<ParameterAnimation> paramAnimations;

public:
    SurfaceEngine(ParametricSurface* surface_)
        : surface(surface_), angleX(0.0f), angleY(0.0f) {
        paramAnimations.emplace_back(1.0f, 3.0f, 0.2f); // alpha
        paramAnimations.emplace_back(0.5f, 2.0f, 0.1f); // beta
        computePoints();
    }

    ~SurfaceEngine() { delete surface; }

    void computePoints() {
        points.clear();
        normals.clear();
        triangles.clear();

        for (size_t i = 0; i < paramAnimations.size(); ++i) {
            surface->setParameter(static_cast<int>(i), paramAnimations[i].value);
        }

        float du = (surface->getUMax() - surface->getUMin()) / (surface->getUSteps() - 1);
        float dv = (surface->getVMax() - surface->getVMin()) / (surface->getVSteps() - 1);

        for (size_t i = 0; i < static_cast<size_t>(surface->getUSteps()); i++) {
            float u = surface->getUMin() + i * du;
            for (size_t j = 0; j < static_cast<size_t>(surface->getVSteps()); j++) {
                float v = surface->getVMin() + j * dv;
                points.push_back(surface->computePoint(u, v));
            }
        }

        for (size_t i = 0; i < static_cast<size_t>(surface->getUSteps() - 1); i++) {
            for (size_t j = 0; j < static_cast<size_t>(surface->getVSteps() - 1); j++) {
                size_t idx = i * static_cast<size_t>(surface->getVSteps()) + j;
                triangles.push_back({ static_cast<int>(idx), static_cast<int>(idx + 1), static_cast<int>(idx + surface->getVSteps()) });
                triangles.push_back({ static_cast<int>(idx + 1), static_cast<int>(idx + surface->getVSteps() + 1), static_cast<int>(idx + surface->getVSteps()) });

                Vec3 p0 = points[idx];
                Vec3 p1 = points[idx + 1];
                Vec3 p2 = points[idx + surface->getVSteps()];
                Vec3 edge1 = p1 - p0;
                Vec3 edge2 = p2 - p0;
                normals.push_back(cross(edge1, edge2).normalize());

                p0 = points[idx + 1];
                p1 = points[idx + surface->getVSteps() + 1];
                p2 = points[idx + surface->getVSteps()];
                edge1 = p1 - p0;
                edge2 = p2 - p0;
                normals.push_back(cross(edge1, edge2).normalize());
            }
        }
    }

    void update(float deltaTime) {
        for (auto& param : paramAnimations) {
            param.value += param.direction * param.step * deltaTime * 2.0f;
            if (param.value >= param.maxVal) {
                param.value = param.maxVal;
                param.direction = -1.0f;
            }
            else if (param.value <= param.minVal) {
                param.value = param.minVal;
                param.direction = 1.0f;
            }
        }

        computePoints();

        angleX += 30.0f * deltaTime;
        angleY += 30.0f * deltaTime;
    }

    void draw() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();

        // Создаем матрицу преобразования
        Matrix4 transform = createTranslation(0.0f, 0.0f, -7.0f) *
            createRotationX(45.0f) *
            createRotationY(-30.0f) *
            createRotationX(angleX) *
            createRotationY(angleY);

        std::vector<std::pair<float, int>> sortedTriangles;
        for (size_t i = 0; i < triangles.size(); i++) {
            const auto& tri = triangles[i];
            Vec3 p0 = points[tri[0]];
            Vec3 p1 = points[tri[1]];
            Vec3 p2 = points[tri[2]];
            float depth = (p0.z + p1.z + p2.z) / 3.0f;
            sortedTriangles.push_back({ depth, static_cast<int>(i) });
        }

        std::sort(sortedTriangles.begin(), sortedTriangles.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        Vec3 lightDir = Vec3(5.0f, 5.0f, 5.0f).normalize();

        glBegin(GL_TRIANGLES);
        for (const auto& triPair : sortedTriangles) {
            int triIdx = triPair.second;
            const auto& tri = triangles[triIdx];
            Vec3 p0 = transform.multiply(points[tri[0]]);
            Vec3 p1 = transform.multiply(points[tri[1]]);
            Vec3 p2 = transform.multiply(points[tri[2]]);

            // Пересчитываем нормали с учетом трансформации
            Vec3 normal = transform.multiply(normals[triIdx]).normalize();
            float depth = (p0.z + p1.z + p2.z) / 3.0f;

            float lambert = dot(normal, lightDir);
            float diffuse = std::max(0.0f, fabs(lambert));
            diffuse = pow(diffuse, 1.5f);
            float ambient = 0.2f;
            float scattered = 0.05f * (1.0f - diffuse);
            float intensity = ambient + (1.0f - ambient) * (diffuse + scattered);
            intensity = std::max(0.0f, std::min(1.0f, intensity));

            float depthFactor = 1.0f - 0.05f * (depth + 3.0f);
            depthFactor = std::max(0.6f, std::min(1.0f, depthFactor));

            glColor3f(depthFactor * intensity * 1.0f, depthFactor * intensity * 0.5f, depthFactor * intensity * 0.2f);

            glVertex3f(p0.x, p0.y, p0.z);
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
        }
        glEnd();
    }
};

// Инициализация OpenGL
void initOpenGL() {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float fovy = 60.0f;
    float aspect = (float)WIDTH / HEIGHT;
    float zNear = 0.1f;
    float zFar = 100.0f;

    float top = zNear * tanf(fovy * static_cast<float>(M_PI) / 360.0f);
    float bottom = -top;
    float right = top * aspect;
    float left = -right;

    glFrustum(left, right, bottom, top, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
}

int main() {
    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Moebius Strip with OpenGL", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    initOpenGL();

    ParametricSurface* surface = new MoebiusStrip(0.0f, 2.0f * static_cast<float>(M_PI), -0.5f, 0.5f, 100, 50);
    SurfaceEngine engine(surface);
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        engine.update(deltaTime);
        engine.draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
