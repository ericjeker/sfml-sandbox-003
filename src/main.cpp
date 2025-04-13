#include <algorithm>
#include <cmath>

#include "imgui-SFML.h"
#include "imgui.h"

#include <SFML/Graphics.hpp>
#include <iostream>

#define LOG(msg) std::cout << "[LOG] " << msg << std::endl;
#define DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl;

constexpr int WINDOW_WIDTH = 1920u;
constexpr int WINDOW_HEIGHT = 1080u;

struct World {
    static constexpr float Gravity = 0.f;
    static constexpr float Friction = 0.98f;
};

struct Mouse {
    sf::Vector2i position{-1, -1};
};

struct Color {
    static constexpr auto Background = sf::Color({40, 42, 54});
};

struct Player {
    float maxSpeed = 2000.f;
    // Rotation speed by frame
    float rotationSpeed = 5.f;
    float health = 100.f;
    // Rate of fire (per second)
    float rateOfFire = 100.f;
    float shootCooldown = 0.f;

    sf::Vector2f position{0, 0};
    sf::Vector2f velocity{0, 0};
    sf::Vector2f acceleration{0, 0};
    // Orientation in Radians
    float orientation;
    sf::Vector2f forwardVector{0, 0};

    void seek(const sf::Vector2f targetPosition, const float delta) {
        const sf::Vector2f direction = targetPosition - position;

        // Give the angle in radians toward the target
        const float targetAngle = atan2(direction.y, direction.x);
        // Find the angle difference between the current orientation and the target angle
        float angleDifference = targetAngle - orientation;

        // Normalize angle difference to [-M_PI, M_PI] range
        while (angleDifference > M_PI)
            angleDifference -= 2 * M_PI;
        while (angleDifference < -M_PI)
            angleDifference += 2 * M_PI;

        // Update current angle
        orientation += angleDifference * rotationSpeed * delta;
        // Recalculate the forward vector based on the new orientation
        forwardVector = sf::Vector2f(std::cos(orientation), std::sin(orientation));

        // Calculate the distance toward the target
        if (direction.length() < 3) {
            return;
        }

        velocity += acceleration * delta;
        position += velocity * delta;
        acceleration = {0, 0};
    }
};

struct Bullet {
    float maxSpeed = 2000.f;
    float damage = 100.f;

    sf::Vector2f position{0.f, 0.f};
    sf::Vector2f velocity{0.f, 0.f};
    sf::Vector2f orientation{0.f, 0.f};
    sf::Vector2f rotation{0.f, 0.f};

    // Mark a bullet for destruction
    bool toRemove = false;
};

struct GameState {
    std::vector<Bullet> bullets;
    std::vector<sf::Sprite> enemies;
};

/**
 * @struct BulletSystem
 *
 * Responsible for managing bullet behavior and updating their states. This system tracks and modifies bullets
 * based on game rules, interactions with other game entities, and elapsed time.
 */
struct BulletSystem {
    void Update(const float delta, std::vector<Bullet> &bullets);
    void Draw(const GameState &gameState, sf::RenderWindow &window);
};

void CalculateScreenWarp(sf::Vector2f &position) {
    if (position.x < 0) {
        position.x = WINDOW_WIDTH;
    }

    if (position.x > WINDOW_WIDTH) {
        position.x = 0;
    }

    if (position.y < 0) {
        position.y = WINDOW_HEIGHT;
    }

    if (position.y > WINDOW_HEIGHT) {
        position.y = 0;
    }
}

int main() {
    // Tells if the window should be closed
    bool shouldClose = false;
    bool imGuiAvailable = false;

    // Window initialization
    auto window = sf::RenderWindow(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "AI for Games : Movement");
    window.setFramerateLimit(144);

    // ImGui initialization
    imGuiAvailable = ImGui::SFML::Init(window);

    // Initialize the Game State
    GameState gameState;
    Mouse mouse;
    mouse.position = {-1, -1};
    Player player;
    player.position = sf::Vector2{WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f};

    // Load the resources
    sf::Texture bulletTexture("assets/Projectile_3_Green.png");
    bulletTexture.setSmooth(true);
    sf::Sprite bulletSprite(bulletTexture);
    bulletSprite.setScale({0.5f, 0.5f});

    sf::Texture playerTexture("assets/Green_Player_Ship_9.png");
    playerTexture.setSmooth(true);
    sf::Sprite playerSprite(playerTexture);
    playerSprite.setOrigin({playerTexture.getSize().x / 2.f, playerTexture.getSize().y / 2.f});
    playerSprite.scale({0.5f, 0.5f});

    /**
     * Start the Game Loop
     */
    sf::Clock clock;
    while (window.isOpen()) {
        const auto deltaTime = clock.restart();
        const float delta = deltaTime.asSeconds();

        if (delta > 1.f / 60.f) {
            LOG("WARNING! Budget exceeded! Delta: " << delta << " seconds.");
        }

        /**
         * Handle events
         */
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>()) {
                LOG("Closing intent");
                shouldClose = true;
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
            LOG("Closing intent");
            shouldClose = true;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
            player.acceleration += player.forwardVector * player.maxSpeed;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
            player.acceleration -= player.forwardVector * player.maxSpeed;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
            player.acceleration += player.forwardVector.rotatedBy(sf::degrees(-90)).normalized() * player.maxSpeed;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
            player.acceleration += player.forwardVector.rotatedBy(sf::degrees(90)).normalized() * player.maxSpeed;
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            if (player.shootCooldown < 0.f) {
                // Spawn a bullet where the spaceship currently is
                Bullet bullet;

                // Orient the bullet forward
                bullet.position = player.position;
                bullet.velocity = player.forwardVector * bullet.maxSpeed;
                bullet.orientation = player.forwardVector;

                // Give the bullet a forward velocity
                gameState.bullets.push_back(bullet);
                player.shootCooldown = 1.f / player.rateOfFire;
            }
        }
        player.shootCooldown -= delta;

        mouse.position = sf::Mouse::getPosition(window);

        // If the events triggered window close, we stop here
        if (shouldClose) {
            LOG("Closing the Window");
            ImGui::SFML::Shutdown();
            window.close();
            break;
        }

        // Player System
        player.seek(static_cast<sf::Vector2f>(mouse.position), delta);

        player.velocity *= World::Friction;

        CalculateScreenWarp(player.position);

        // Bullet System
        for (auto &bullet: gameState.bullets) {
            bullet.position += bullet.velocity * delta;

            // If the bullet is out of screen, we remove it from the array
            if (bullet.position.x < 0 || bullet.position.x > WINDOW_WIDTH || bullet.position.y < 0 ||
                bullet.position.y > WINDOW_HEIGHT) {
                bullet.toRemove = true;
            }
        }

        // Erase the bullets we don't need to render anymore
        gameState.bullets.erase(std::remove_if(gameState.bullets.begin(), gameState.bullets.end(),
                                               [](const auto &bullet) { return bullet.toRemove; }),
                                gameState.bullets.end());


        // ImGUI Tweak Boxes, for debugging
        ImGui::SFML::Update(window, deltaTime);
        ImGui::Begin("Tweak Box & Monitoring");
        ImGui::Text("FPS: %.1f", 1.0f / delta);
        ImGui::Text("Delta: %.1f", delta * 1000);
        ImGui::Text("Mouse Position: (%d, %d)", mouse.position.x, mouse.position.y);
        if (ImGui::CollapsingHeader("Player Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Max Speed", &player.maxSpeed, 0.0f, 500.0f);
            ImGui::SliderFloat("Rotation Speed", &player.rotationSpeed, 0.0f, 20.0f);
            ImGui::SliderFloat("Player Rate of Fire", &player.rateOfFire, 0.0f, 500.0f);
        }
        if (ImGui::CollapsingHeader("Bullet Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Cooldown: %.1f", player.shootCooldown);
            ImGui::Text("Bullet count: %d", static_cast<int>(gameState.bullets.size()));
        }
        ImGui::End();

        /**
         * Start the Rendering process
         */
        window.clear(Color::Background);

        // Draw the bullets
        for (auto &bullet: gameState.bullets) {
            // Draw the bullet
            sf::Sprite sprite(bulletSprite);
            sprite.setScale({0.5f, 0.5f});
            sprite.setPosition(bullet.position);
            sprite.setRotation(bullet.orientation.angle() + sf::degrees(90.f));
            window.draw(sprite);
        }

        // Draw the enemies
        for (auto &enemy: gameState.enemies) {
        }

        // Draw the player
        playerSprite.setRotation(sf::degrees(player.orientation * 180.f / M_PI) + sf::degrees(90.f));
        playerSprite.setPosition(sf::Vector2f(player.position));
        window.draw(playerSprite);

        // Draw the ImGui interface
        ImGui::SFML::Render(window);

        window.display();
    }

    return 0;
}
