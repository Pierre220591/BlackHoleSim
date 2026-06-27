#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Camera.h"

// --- CONSTANTES PHYSIQUES ---
const float Rs = 1.0f;          // Rayon de Schwarzschild
const float c = 1.0f;           // Vitesse de la lumière
const float dt = 0.005f;        // Pas de temps (réduit pour la stabilité)
const float LIMIT_DIST = 100.0f; // Distance de reset

// --- VARIABLES CAMÉRA ORBITALE INTERACTIVE ---
float camRadius = 50.0f;
float camYaw = 0.0f;
float camPitch = 0.3f; // Légèrement en hauteur pour apprécier le volume
bool isDragging = false;
double lastMouseX = 0.0, lastMouseY = 0.0;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camRadius -= (float)yoffset * 3.0f;
    if (camRadius < 5.0f) camRadius = 5.0f;
    if (camRadius > 250.0f) camRadius = 250.0f;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) isDragging = true;
        else if (action == GLFW_RELEASE) isDragging = false;
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (isDragging) {
        float xoffset = static_cast<float>(xpos - lastMouseX);
        float yoffset = static_cast<float>(ypos - lastMouseY);
        camYaw -= xoffset * 0.01f;
        camPitch += yoffset * 0.01f;
        if (camPitch > 1.5f) camPitch = 1.5f; // Limites verticales
        if (camPitch < -1.5f) camPitch = -1.5f;
    }
    lastMouseX = xpos;
    lastMouseY = ypos;
}

struct Ray {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 color;
    std::vector<glm::vec3> trail;
    glm::vec3 startPos;
    glm::vec3 startVel;
    bool inDisk;
    bool isChosen;
    float accumulatedAngle;

    void reset() {
        pos = startPos;
        vel = startVel;
        trail.clear();
        inDisk = false;
        accumulatedAngle = 0.0f;
    }
};

// --- CALCUL DE L'ACCÉLÉRATION (GÉODÉSIQUE) ---
glm::vec3 getAcceleration(glm::vec3 pos, glm::vec3 vel) {
    float r = glm::length(pos);
    
    // Sécurité : si on est trop près de la singularité, on stoppe la force
    if (r < Rs * 1.01f) return glm::vec3(0.0f);

    // Moment angulaire spécifique L = r x v
    glm::vec3 L_vec = glm::cross(pos, vel);
    float L2 = glm::dot(L_vec, L_vec);

    // Accélération relativiste corrigée : a = -1.5 * Rs * L^2 / r^4
    // (Le code d'origine divisait par r^5, ce qui sapait gravement la gravité !)
    float accelMag = -1.5f * Rs * L2 / std::pow(r, 4.0f);
    
    return glm::normalize(pos) * accelMag;
}

// --- INTÉGRATION RUNGE-KUTTA 4 (RK4) ---
void updateRay(Ray& ray) {
    float initial_r = glm::length(ray.pos);
    int sub_steps = 1;
    float current_dt = dt;

    // Précision Adaptive (RK4 sub-stepping près de la sphère de photons)
    if (initial_r > 1.2f * Rs && initial_r < 2.0f * Rs) {
        if (ray.isChosen) {
            sub_steps = 50;
            current_dt = dt / 50.0f; // dt très fin équivalent à 0.0001
        } else {
            sub_steps = 10;
            current_dt = dt / 10.0f;
        }
    }

    for (int step = 0; step < sub_steps; ++step) {
        glm::vec3 p = ray.pos;
        glm::vec3 v = ray.vel;

        // k1
        glm::vec3 kv1 = getAcceleration(p, v) * current_dt;
        glm::vec3 kp1 = v * current_dt;

        // k2
        glm::vec3 kv2 = getAcceleration(p + kp1 * 0.5f, v + kv1 * 0.5f) * current_dt;
        glm::vec3 kp2 = (v + kv1 * 0.5f) * current_dt;

        // k3
        glm::vec3 kv3 = getAcceleration(p + kp2 * 0.5f, v + kv2 * 0.5f) * current_dt;
        glm::vec3 kp3 = (v + kv2 * 0.5f) * current_dt;

        // k4
        glm::vec3 kv4 = getAcceleration(p + kp3, v + kv3) * current_dt;
        glm::vec3 kp4 = (v + kv3) * current_dt;

        ray.vel += (kv1 + 2.0f * kv2 + 2.0f * kv3 + kv4) / 6.0f;
        ray.pos += (kp1 + 2.0f * kp2 + 2.0f * kp3 + kp4) / 6.0f;

        // Triche pour le rayon élu : orbite parfaite
        if (ray.isChosen) {
            float r_now = glm::length(ray.pos);
            // Dès qu'il frôle l'orbite photonique, on applique un verrouillage progressif très fort
            if (r_now < 1.55f * Rs) {
                float target_r = 1.5f * Rs;
                glm::vec3 radial_dir = glm::normalize(ray.pos);
                
                // 1. On compresse sa position fermement sur r = 1.5 Rs
                ray.pos += radial_dir * ((target_r - r_now) * 0.1f);
                
                // 2. On dissipe sa vitesse radiale (pour le forcer à tangenter)
                float rad_v = glm::dot(ray.vel, radial_dir);
                ray.vel -= radial_dir * rad_v * 0.5f;
                
                // 3. Vitesse Constante c absolue
                if (glm::length(ray.vel) > 0.0f) {
                    ray.vel = glm::normalize(ray.vel) * c;
                }
            }
        }

        // Calcul de l'angle parcouru
        float currentAngle = std::atan2(ray.pos.y, ray.pos.x);
        float prevAngle = std::atan2(p.y, p.x);
        float dTheta = currentAngle - prevAngle;
        
        float pi_val = 3.14159265359f;
        if (dTheta > pi_val) dTheta -= 2.0f * pi_val;
        if (dTheta < -pi_val) dTheta += 2.0f * pi_val;
        
        ray.accumulatedAngle += std::abs(dTheta);
        
        float current_dist = glm::length(ray.pos);
        // Détection de la traversée de l'épaisseur du disque d'accrétion (Volume 3D)
        if (!ray.inDisk && std::abs(ray.pos.y) <= 0.4f && !ray.isChosen) {
            // Distance cylindrique projetée sur le plan X-Z horizontal
            float r_xz = std::sqrt(ray.pos.x * ray.pos.x + ray.pos.z * ray.pos.z);
            if (r_xz >= 3.0f && r_xz <= 10.0f) {
                ray.inDisk = true;
            }
        }
    }

    float dist = glm::length(ray.pos);

    if (ray.inDisk) {
        if (dist <= Rs * 1.5f) {
            ray.vel = glm::vec3(0.0f); // S'arrêter près de l'horizon
        } else {
            ray.vel *= 0.99f; // Friction => Spirale
        }
        // Couleurs vibrantes : d'orange sombre à un jaune électrique (simulateur de chaleur intense)
        float t = glm::clamp((dist - 1.5f) / 8.5f, 0.0f, 1.0f);
        ray.color = glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.8f, 0.2f, 0.0f), t);
    }

    // On ajoute la position actuelle au trail pour le dessin (si en mouvement)
    if (glm::length(ray.vel) > 0.0001f || ray.trail.empty()) {
        ray.trail.push_back(ray.pos);
    }

    // Conditions de reset
    bool shouldReset = false;
    if (dist < Rs || dist > LIMIT_DIST || ray.pos.x > LIMIT_DIST) {
        shouldReset = true;
    }

    if (ray.isChosen) {
        // Le rayon élu tourne éternellement, on désactive complètement son reset !
        shouldReset = false;
    }

    if (shouldReset) {
        ray.reset();
    }
}

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Relativistic Ray Tracing", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glewInit();

    // Callbacks de Contrôle Caméra Interactive
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Initialisation de 25 rayons avec distribution en couronne 3D (spirale)
    std::vector<Ray> rays;
    int numRays = 25;
    float minY = 0.1f;
    float maxY = 10.0f;
    for (int i = 0; i < numRays; ++i) {
        float t = (float)i / (numRays - 1);
        
        // Distribution en spirale pour diversifier l'axe Y et Z
        float b = minY + t * (maxY - minY); // b = paramètre d'impact géométrique
        float angle = i * 2.39996f; // Nombre d'or rad
        
        float startY = b * std::cos(angle);
        float startZ = b * std::sin(angle);
        
        glm::vec3 color;
        bool isChosen = false;
        
        if (i == 9) { // Le Rayon Élu (le 10ème)
            startY = 2.598076f;
            startZ = 0.0f; // Gardé au pôle pour une orbitographie pure
            color = glm::vec3(0.0f, 1.0f, 1.0f); // Cyan très visible
            isChosen = true;
        } else {
            if (t < 0.25f) { 
                float localT = t / 0.25f;
                color = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.3f, 0.0f), localT);
            } else if (t < 0.5f) { 
                float localT = (t - 0.25f) / 0.25f;
                color = glm::mix(glm::vec3(1.0f, 0.3f, 0.0f), glm::vec3(1.0f, 0.8f, 0.0f), localT);
            } else { 
                float localT = (t - 0.5f) / 0.5f;
                color = glm::mix(glm::vec3(0.0f, 0.4f, 1.0f), glm::vec3(0.8f, 0.9f, 1.0f), localT);
            }
        }
        
        glm::vec3 startPos(-25.0f, startY, startZ);
        
        // La Vitesse pointe vers le centre du Trou Noir, mais avec une déviation délibérée
        // Cette déviation engendre le volume de la géométrie de courbure (impact parameter)
        glm::vec3 target(0.0f, startY, startZ); 
        glm::vec3 dir = glm::normalize(target - startPos);
        glm::vec3 startVel = dir * c;
        
        rays.push_back({startPos, startVel, color, {}, startPos, startVel, false, isChosen, 0.0f});
    }

    // Génération du champ d'étoiles fixes volumétrique (Parallaxe)
    srand((unsigned int)time(NULL));
    std::vector<glm::vec3> stars;
    std::vector<float> starBaseSizes;
    for(int i = 0; i < 1000; ++i) {
        // Profondeur volumétrique pour l'effet de parallaxe
        float z = -50.0f - (rand() % 1500) / 10.0f; // -50.0 à -200.0
        // Pour couvrir le champ de vision qui s'élargit en profondeur :
        float spread = std::abs(z) * 1.5f; 
        float x = (rand() % 2000 - 1000) / 1000.0f * spread; 
        float y = (rand() % 2000 - 1000) / 1000.0f * spread;
        stars.push_back(glm::vec3(x, y, z));
        starBaseSizes.push_back((rand() % 25 + 5) / 10.0f); // 0.5 à 3.0
    }

    // Génération des particules du disque d'accrétion (Poussière et Gaz)
    struct DiskParticle {
        float radius;
        float angle;
        float speed;
        glm::vec3 color;
    };
    std::vector<DiskParticle> diskParticles;
    for(int i = 0; i < 500; ++i) {
        float r = 3.0f + (rand() % 700) / 100.0f; // Rayon 3.0 à 10.0
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        // La vitesse képlérienne dépend de la distance : vr = C / sqrt(r)
        float speed = 2.0f / std::sqrt(r);

        // Couleurs chaudes et variées
        int colorType = rand() % 3;
        glm::vec3 col;
        if(colorType == 0) col = glm::vec3(1.0f, 0.5f, 0.0f); // Orange vif
        else if(colorType == 1) col = glm::vec3(0.8f, 0.2f, 0.0f); // Rouge sombre (matière plus froide)
        else col = glm::vec3(0.9f, 0.7f, 0.0f); // Jaune sombre
        
        diskParticles.push_back({r, angle, speed, col});
    }

    // Esthétique : Lignes plus fines et douces
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_PROGRAM_POINT_SIZE);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Fond noir absolu
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 300.0f); // Far étendu à 300 pour la parallaxe lointaine
        
        // Caméra Orbitale Interactive Mathématique
        glm::vec3 camPos;
        camPos.x = camRadius * std::cos(camPitch) * std::sin(camYaw);
        camPos.y = camRadius * std::sin(camPitch);
        camPos.z = camRadius * std::cos(camPitch) * std::cos(camYaw);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
        glLoadMatrixf(glm::value_ptr(projection * view));

        // 1. Dessiner le champ d'étoiles fixes (Avec Parallaxe dynamique 3D)
        for(size_t i = 0; i < stars.size(); ++i) {
            // Variation de la taille selon la vraie distance 3D avec la caméra
            float dist = glm::length(camPos - stars[i]);
            float scale = 150.0f / dist; // Ajustement dynamique pour la perspective
            glPointSize(starBaseSizes[i] * scale);

            glBegin(GL_POINTS);
            float alphaStar = ((rand() % 100) > 95) ? 0.3f : 0.9f;
            // Etoiles lointaines très légèrement plus sombres ou voilées
            glColor4f(1.0f, 1.0f, 1.0f, alphaStar * std::min(scale, 1.0f));
            glVertex3fv(glm::value_ptr(stars[i]));
            glEnd();
        }

        // 1.5 Update et Rendu du Disque d'Accrétion (Nuage de Particules)
        glPointSize(2.0f); // Les particules de poussière gardent une petite taille fixe
        glBegin(GL_POINTS);
        for(auto& p : diskParticles) {
            // Rotation lente et dépendante du rayon
            p.angle += p.speed * dt;
            if (p.angle > 2.0f * 3.14159f) p.angle -= 2.0f * 3.14159f;
            
            // Position circulaire sur le plan Y = 0
            glm::vec3 pos(p.radius * std::cos(p.angle), 0.0f, p.radius * std::sin(p.angle));
            
            // Texture semi-transparente pour simuler le volume diffus des gaz
            glColor4f(p.color.r, p.color.g, p.color.b, 0.6f);
            glVertex3fv(glm::value_ptr(pos));
        }
        glEnd();

        // 2. Update et Rendu des Rayons
        for (auto& ray : rays) {
            updateRay(ray);

            // Effet de Halo (Glow) / Variation d'épaisseur
            if (ray.isChosen) {
                glLineWidth(3.0f);
            } else if (ray.inDisk) {
                glLineWidth(2.5f);
            } else {
                glLineWidth(1.0f);
            }

            glBegin(GL_LINE_STRIP);
            int trailSize = ray.trail.size();
            for (int i = 0; i < trailSize; ++i) {
                // Gradient Alpha dégressif (Traînées)
                float alpha = (float)i / (float)trailSize;
                alpha = alpha * alpha; // Accentue la transparence sur la queue (effet comète)
                glColor4f(ray.color.r, ray.color.g, ray.color.b, alpha);
                glVertex3fv(glm::value_ptr(ray.trail[i]));
            }
            glEnd();
        }

        // 3. Sphère noire parfaite (Horizon des événements)
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.0f, 0.0f, 0.0f); // Noir absolu
        glVertex3f(0.0f, 0.0f, 0.0f); // Centre
        for(int i = 0; i <= 64; ++i) {
            float angle = i * 2.0f * 3.14159f / 64.0f;
            glVertex3f(cos(angle) * Rs, sin(angle) * Rs, 0.0f);
        }
        glEnd();

        // 4. Liseré de lumière blanche sur les bords
        glLineWidth(2.5f);
        glBegin(GL_LINE_LOOP);
        glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
        for(int i = 0; i < 64; ++i) {
            float angle = i * 2.0f * 3.14159f / 64.0f;
            glVertex3f(cos(angle) * Rs, sin(angle) * Rs, 0.0f);
        }
        glEnd();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}