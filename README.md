# Relativistic Ray Tracing - Schwarzschild Black Hole Simulation

Ce projet est un simulateur tridimensionnel en temps réel de la trajectoire de la lumière (photons) aux abords d'un trou noir statique, basé sur la métrique de Schwarzschild et les principes de la Relativité Générale. Développé en C++ avec OpenGL, le programme intègre une résolution numérique par la méthode de Runge-Kutta d'ordre 4 (RK4) pour modéliser le mirage gravitationnel, le disque d'accrétion et la sphère de photons.

## Fonctionnalités principales

* **Intégration RK4 adaptative** : Résolution mathématique précise des géodésiques de la lumière avec réduction dynamique du pas de temps ($dt$) à l'approche de l'horizon des événements afin d'éviter les erreurs d'intégration numérique.
* **Modélisation tridimensionnelle volumétrique** : Distribution circulaire et conique des rayons incidents orientés vers la singularité pour capturer l'effet de lentille gravitationnelle sous tous les angles.
* **Représentation des trois cas relativistes** :
  * **Capture** : Absorption irrémédiable des rayons franchissant l'horizon des événements.
  * **Sphère de photons** : Verrouillage et stabilisation mathématique d'un rayon critique sur une orbite instable à $1.5 R_s$.
  * **Lentille gravitationnelle** : Déviation continue des rayons lointains créant l'effet visuel caractéristique d'un mirage cosmique.
* **Rendu esthétique avancé** : Disque d'accrétion modélisé par un système de 500 particules en rotation, champ d'étoiles tridimensionnel avec gestion de la parallaxe lors des déplacements, et évanouissement progressif (gradient alpha) des trajectoires lumineuses.

## Prérequis et Dépendances

Le projet a été configuré et optimisé pour macOS (Apple Silicon M1/M2/M3) via le profil Core d'OpenGL 4.1.

* Compilateur compatible C++11 (Clang/GCC)
* CMake (>= 3.11)
* GLFW3
* GLEW
* GLM (OpenGL Mathematics)

### Installation des dépendances sur macOS

```bash
brew install cmake glfw glew glm