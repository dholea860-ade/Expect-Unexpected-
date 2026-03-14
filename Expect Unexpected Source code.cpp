#include <SFML/Graphics.hpp>
#include <optional>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>

#include <SFML/Graphics.hpp>
#define DEV_START_POS

const float DESIGN_W = 1920.f;  // the "original" width your game layout is based on
const float DESIGN_H = 1080.f;  // the "original" height

struct ParallaxLayer {
    std::vector<sf::Sprite> sprites;
    std::vector<sf::Vector2f> basePositions; // 🔥 ADD THIS
    float parallaxFactor;
};

enum class TrapType
{
    None,
    VanishOnTouch,
    FallOnTouch,
    AppearWhenNear,
    Pressure,
    MoveOnTouch
};
enum class SpikeType
{
    Static,
    FallingOnTrigger,
    FallingTimed,
    AppearWhenNear   // 🔥 new type
};

struct Platform
{
    sf::RectangleShape shape;

    bool active = true;
    bool triggered = false;
    sf::Vector2f originalPos;

    TrapType trapType = TrapType::None;

    float triggerDistance = 150.f;
    float fallSpeed = 600.f;
    float timer = 0.f;

    float fallDelay = 0.1f;   // how long to wait before falling
    float fallTimer = 0.f;   // internal timer
    float respawnDelay = 1.f;   // wait time before becoming active again
    float respawnTimer = 0.f;   // internal timer

    // ===== MOVING SYSTEM =====
    bool isMoving = false;

    sf::Vector2f pointA;
    sf::Vector2f pointB;
    sf::Vector2f moveDir;

    float moveSpeed = 150.f;
    float moveDistance = 0.f;

    float traveled = 0.f;
    bool forward = true;

    float originalY;
    float moveSpeedpresspt = 300.f;
    float maxDrop = 1000.f;
    bool isPressed = false;


    Platform(float width, float height, sf::Vector2f position, TrapType type = TrapType::None)
        : trapType(type) , originalPos(position)
    {
        shape.setSize({ width, height });
        shape.setFillColor(sf::Color::Red);
        shape.setPosition(position);
        originalY = position.y;
    }
};
struct Level
{
    float startX;
    float endX;
};

std::vector<Level> levels;
int currentLevel = 0;
int lastCompletedLevel = 0;
float spikeSpawnTimer = 0.f;
float spikeSpawnInterval = 2.0f; // spawn every 2 seconds
// ================= PLAYER =================
struct Player {

    sf::Sprite sprite;
    sf::Image mask; // Added
    sf::Vector2f velocity;
    sf::Vector2f spawnPos;  // respawn position

    float jumpForce = -700.f;
    float gravity = 1500.f;

    int jumpCount = 0;
    int maxJumps = 2;
    bool isGrounded = false;
    bool jumpHeld = false;

    float baseScale = 0.15f;
    int facing = -1;
    float idleTimer = 0.f;

    float jumpTime = 0.f;
    float maxJumpTime = 0.18f;
    float jumpSustainForce = -2200.f;

    float coyoteTimer = 0.f;
    float coyoteTime = 0.12f;

    float jumpBufferTimer = 0.f;
    float jumpBufferTime = 0.12f;

    float runAcceleration = 4000.f;
    float groundFriction = 700.f;
    float airDrag = 500.f;
    float maxRunSpeed = 350.f;

    sf::Vector2f colliderSize;
    sf::Vector2f colliderOffset;
    sf::FloatRect hitbox;

    Player(sf::Texture& tex, sf::Vector2f startPos)
        : sprite(tex)
    {
        sprite.setOrigin({
            tex.getSize().x / 2.f,
            tex.getSize().y / 2.f
            });
        sprite.setPosition(startPos);

        mask = tex.copyToImage(); // Added

        float spriteWidth = tex.getSize().x * baseScale;
        float spriteHeight = tex.getSize().y * baseScale;

        // Smaller than sprite for better feel
        colliderSize = { spriteWidth * 0.6f, spriteHeight * 0.8f };

        // Centered box
        colliderOffset = { -colliderSize.x / 2.f, -colliderSize.y / 2.f };

        hitbox = sf::FloatRect(
            sprite.getPosition() + colliderOffset,
            colliderSize
        );
    }

    void tryJump()
    {
        jumpBufferTimer = jumpBufferTime;
    }

    void startJump()
    {
        velocity.y = jumpForce;
        jumpTime = 0.f;
        jumpCount++;
        isGrounded = false;
    }

    void applyGravity(float dt)
    {
        velocity.y += gravity * dt;
    }

    void update(float dt)
    {
        if (isGrounded)
        {
            if (velocity.x > 0.f)
            {
                velocity.x -= groundFriction * dt;
                if (velocity.x < 0.f) velocity.x = 0.f;
            }
            else if (velocity.x < 0.f)
            {
                velocity.x += groundFriction * dt;
                if (velocity.x > 0.f) velocity.x = 0.f;
            }
        }

        float breathe = baseScale;

        if (velocity.x == 0.f && isGrounded)
        {
            idleTimer += dt;
            breathe = baseScale + std::sin(idleTimer * 2.5f) * 0.015f;
        }

        sprite.setScale({
            facing * breathe,
            breathe
            });

        // TIMERS
        if (!isGrounded)
            coyoteTimer -= dt;

        jumpBufferTimer -= dt;


        // BUFFERED JUMP
        if (jumpBufferTimer > 0.f &&
            (isGrounded || coyoteTimer > 0.f) &&
            jumpCount < maxJumps)
        {
            startJump();
            jumpBufferTimer = 0.f;
        }


        // SUSTAIN JUMP WHILE HOLDING
        if (jumpHeld &&
            velocity.y < 0.f &&
            jumpTime < maxJumpTime)
        {
            velocity.y += jumpSustainForce * dt;
            jumpTime += dt;
        }
    }

    void resetJump()
    {
        jumpCount = 0;
        isGrounded = true;
        coyoteTimer = coyoteTime;
    }
    // ---------------- PLAYER MOVEMENT & COLLISION (SFML 3.0.2) ----------------
    void movePlayer(Player& player, float dt, std::vector<Platform>& platforms)
    {
        // 🔥 LEVEL PROGRESSION CHECK

        // ----- HORIZONTAL MOVEMENT -----
        sf::Vector2f input(0.f, 0.f);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  input.x -= 1.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) input.x += 1.f;

        // Apply acceleration
        if (input.x != 0.f)
        {
            player.velocity.x += input.x * player.runAcceleration * dt;
            if (player.velocity.x > player.maxRunSpeed) player.velocity.x = player.maxRunSpeed;
            if (player.velocity.x < -player.maxRunSpeed) player.velocity.x = -player.maxRunSpeed;
            player.facing = input.x > 0 ? -1 : 1;
        }
        else
        {
            // Apply friction/air drag
            float drag = player.isGrounded ? player.groundFriction : player.airDrag;
            if (player.velocity.x > 0.f)
            {
                player.velocity.x -= drag * dt;
                if (player.velocity.x < 0.f) player.velocity.x = 0.f;
            }
            else if (player.velocity.x < 0.f)
            {
                player.velocity.x += drag * dt;
                if (player.velocity.x > 0.f) player.velocity.x = 0.f;
            }
        }

        // ----- VERTICAL MOVEMENT -----
        // Apply gravity
        player.velocity.y += player.gravity * dt;

        // Buffered jump
        if (player.jumpBufferTimer > 0.f &&
            (player.isGrounded || player.coyoteTimer > 0.f) &&
            player.jumpCount < player.maxJumps)
        {
            player.velocity.y = player.jumpForce;
            player.jumpTime = 0.f;
            player.jumpCount++;
            player.isGrounded = false;
            player.jumpBufferTimer = 0.f;
        }

        // Sustain jump
        if (player.jumpHeld && player.velocity.y < 0.f && player.jumpTime < player.maxJumpTime)
        {
            player.velocity.y += player.jumpSustainForce * dt;
            player.jumpTime += dt;
        }

        // ----- APPLY HORIZONTAL MOVEMENT -----
        player.sprite.move({ player.velocity.x * dt, 0.f });
        player.hitbox.position = player.sprite.getPosition() + player.colliderOffset;

        sf::FloatRect playerBounds = player.hitbox;

        // Horizontal collision

        for (auto& p : platforms)
        {
            if (!p.active) continue;
            sf::FloatRect platBounds = p.shape.getGlobalBounds();

            // Only check platforms near player
            if (platBounds.position.x + platBounds.size.x < playerBounds.position.x - 200000.f ||
                platBounds.position.x > playerBounds.position.x + playerBounds.size.x + 200000.f)
                continue;

            bool verticalOverlap = (playerBounds.position.y + playerBounds.size.y > platBounds.position.y + 1.f) &&
                (playerBounds.position.y < platBounds.position.y + platBounds.size.y - 1.f);
            if (!verticalOverlap) continue;

            // Collision on right
            if (player.velocity.x > 0.f &&
                playerBounds.position.x + playerBounds.size.x > platBounds.position.x &&
                playerBounds.position.x < platBounds.position.x + platBounds.size.x)
            {
                float overlap = (playerBounds.position.x + playerBounds.size.x) - platBounds.position.x;
                player.sprite.move({ -overlap, 0.f });
                player.hitbox.position = player.sprite.getPosition() + player.colliderOffset;
                playerBounds = player.hitbox;
                player.velocity.x = 0.f;
                player.hitbox.position = player.sprite.getPosition() + player.colliderOffset;
                playerBounds = player.hitbox;
            }
            // Collision on left
            else if (player.velocity.x < 0.f &&
                playerBounds.position.x < platBounds.position.x + platBounds.size.x &&
                playerBounds.position.x + playerBounds.size.x > platBounds.position.x)
            {
                float overlap = (platBounds.position.x + platBounds.size.x) - playerBounds.position.x;
                player.sprite.move({ overlap, 0.f });
                player.velocity.x = 0.f;
                player.hitbox.position = player.sprite.getPosition() + player.colliderOffset;
                playerBounds = player.hitbox;
            }
        }

        // ----- APPLY VERTICAL MOVEMENT -----
        player.isGrounded = false;
        player.sprite.move({ 0.f, player.velocity.y * dt });
        player.hitbox.position = player.sprite.getPosition() + player.colliderOffset;
        playerBounds = player.hitbox;


        for (auto& p : platforms)
        {
            if (!p.active) continue;
            sf::FloatRect platBounds = p.shape.getGlobalBounds();

            // Only check platforms near player
            if (platBounds.position.y + platBounds.size.y < playerBounds.position.y - 400.f ||
                platBounds.position.y > playerBounds.position.y + playerBounds.size.y + 400.f)
                continue;

            bool horizontalOverlap = (playerBounds.position.x + playerBounds.size.x > platBounds.position.x + 1.f) &&
                (playerBounds.position.x < platBounds.position.x + platBounds.size.x - 1.f);
            if (!horizontalOverlap) continue;

            std::optional<sf::FloatRect> intersection = playerBounds.findIntersection(platBounds);
            if (!intersection.has_value()) continue;
            
            float interHeight = intersection->size.y;

            // Landing on top
            if (player.velocity.y > 0.f && playerBounds.position.y + playerBounds.size.y - interHeight <= platBounds.position.y)
            {
                player.sprite.move({ 0.f, -interHeight });
                player.velocity.y = 0.f;
                if (!player.isGrounded)
                {
                    player.resetJump();
                    player.isGrounded = true;
                    // 🔥 UPDATE SPAWN TO THIS PLATFORM
                    player.spawnPos = player.sprite.getPosition();
                }
                if (p.trapType == TrapType::Pressure)
                {
                    p.isPressed = true;
                }
                if (p.trapType == TrapType::MoveOnTouch)
                {
                    p.isMoving = true;
                }

                if (p.isMoving)
                {
                    float step = p.moveSpeed * dt;
                    if (!p.forward) step = -step;

                    player.sprite.move(p.moveDir * step);
                }

                playerBounds = player.sprite.getGlobalBounds();
            }
            // Hitting head
            else if (player.velocity.y < 0.f && playerBounds.position.y + interHeight >= platBounds.position.y + platBounds.size.y)
            {
                player.sprite.move({ 0.f, interHeight });
                player.velocity.y = 0.f;
                playerBounds = player.sprite.getGlobalBounds();
            }
        }

        // ----- UPDATE TIMERS -----
        if (!player.isGrounded)
            player.coyoteTimer -= dt;

        player.jumpBufferTimer -= dt;

        hitbox.position = sprite.getPosition() + colliderOffset;
    }
};
// ================= SPIKE =================
struct Spike
{
    sf::ConvexShape shape;

    SpikeType type = SpikeType::Static;

    float fallSpeed = 900.f;
    bool triggered = false;

    float timer = 0.f;
    float interval = 1.f;   // used only for timed spikes

    // 🔥 NEW
    float triggerDistance = 150.f;
    bool active = true;

    Spike(float w, float h,
        sf::Vector2f pos,
        bool ceiling = false,
        SpikeType spikeType = SpikeType::Static)
        : type(spikeType)
    {
        shape.setPointCount(3);

        if (!ceiling)
        {
            shape.setPoint(0, { 0.f, h });
            shape.setPoint(1, { w / 2.f, 0.f });
            shape.setPoint(2, { w, h });
        }
        else
        {
            shape.setPoint(0, { 0.f, 0.f });
            shape.setPoint(1, { w / 2.f, h });
            shape.setPoint(2, { w, 0.f });
        }

        shape.setPosition(pos);
        if (type == SpikeType::AppearWhenNear)
        {
            active = false;
            shape.setFillColor(sf::Color::Transparent);
        }
        else
        {
            shape.setFillColor(sf::Color::White);
        }
    }
};
// Normalized platform info for scaling
struct PlatformData {
    float xNorm;      // X position as 0..1 relative to design width
    float yNorm;      // Y position as 0..1 relative to design height
    float widthNorm;  // width as fraction of design width
    float heightNorm; // height as fraction of design height
};
std::vector<PlatformData> platformDefs;

std::vector<ParallaxLayer> layers;
void createFallRow(
    std::vector<Platform>& platforms,
    std::vector<PlatformData>& platformDefs,
    sf::Vector2u winSize,
    float startXNorm,
    float yNorm,
    float widthNorm,
    float heightNorm,
    int count,
    float gapNorm,
    float fallSpeed)
{
    for (int i = 0; i < count; i++)
    {
        float x = startXNorm + i * gapNorm;

        float px = x * winSize.x;
        float py = yNorm * winSize.y;
        float pw = widthNorm * winSize.x;
        float ph = heightNorm * winSize.y;

        Platform p(pw, ph, sf::Vector2f(px, py), TrapType::FallOnTouch);

        p.fallSpeed = fallSpeed;   // ✅ control speed here

        platforms.push_back(p);
        platformDefs.push_back({ x, yNorm, widthNorm, heightNorm });
    }
}

void setupMovingPlatform(
    Platform& p,
    sf::Vector2f start,
    sf::Vector2f end,
    float speed)
{
    p.isMoving = true;
    p.pointA = start;
    p.pointB = end;
    p.moveSpeed = speed;

    sf::Vector2f diff = end - start;
    p.moveDistance = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    if (p.moveDistance != 0.f)
        p.moveDir = diff / p.moveDistance;

    p.traveled = 0.f;
    p.forward = true;
}

void createPlatforms(
    std::vector<Platform>& platforms,
    std::vector<PlatformData>& platformDefs,
    sf::Vector2u winSize)
{
    auto createPlatform = [&](float xNorm, float yNorm,
        float wNorm, float hNorm,
        TrapType type = TrapType::None)
        {
            float px = xNorm * winSize.x;
            float py = yNorm * winSize.y;
            float pw = wNorm * winSize.x;
            float ph = hNorm * winSize.y;

            Platform p(pw, ph, sf::Vector2f(px, py), type);
            platforms.push_back(p);

            platformDefs.push_back({ xNorm, yNorm, wNorm, hNorm });
        };

    createPlatform(0.15f, 0.35f, 0.20f, 0.03f, TrapType::None);
    createPlatform(0.50f, 0.65f, 0.10f, 0.03f, TrapType::AppearWhenNear);
    createPlatform(0.70f, 0.45f, 0.10f, 0.03f, TrapType::VanishOnTouch);
    createPlatform(0.80f, 0.45f, 0.10f, 0.03f, TrapType::AppearWhenNear);
    createPlatform(0.35f, 0.20f, 0.15f, 0.03f, TrapType::Pressure);

    createFallRow(
        platforms,
        platformDefs,
        winSize,
        0.90f,   // start X
        0.45f,   // Y
        0.005f,  // width
        0.03f,   // height
        100,      // 🔥 how many platforms
        0.005f,
        1000// gap between them
    );


    createPlatform(2.0f, 0.20f, 0.10f, 0.03f);
    Platform& last = platforms.back();
    sf::Vector2f start = last.shape.getPosition();
    sf::Vector2f end = start + sf::Vector2f(300.f, 0.f);
    setupMovingPlatform(last, start, end, 200.f);

    createPlatform(1.40f, 0.65f, 0.10f, 0.03f);
    Platform& last2 = platforms.back();
    sf::Vector2f start2 = last2.shape.getPosition();
    sf::Vector2f end2 = start2 + sf::Vector2f(0.f, -700.f);
    setupMovingPlatform(last2, start2, end2, 150.f);

    createPlatform(1.60f, 0.60f, 0.10f, 0.03f);
    Platform& last3 = platforms.back();
    sf::Vector2f start3 = last3.shape.getPosition();
    sf::Vector2f end3 = start3 + sf::Vector2f(600.f, -150.f);
    setupMovingPlatform(last3, start3, end3, 180.f);

    createPlatform(2.50f, 0.20f, 0.05f, 0.03f, TrapType::None);
    createPlatform(2.80f, 0.35f, 0.05f, 0.03f, TrapType::None);
    createPlatform(2.90f, 0.50f, 0.05f, 0.03f, TrapType::AppearWhenNear);
    createPlatform(3.15f, 0.50f, 0.05f, 0.03f, TrapType::VanishOnTouch);
    createPlatform(3.25f, 0.50f, 0.05f, 0.03f, TrapType::AppearWhenNear);

    createFallRow(
        platforms,
        platformDefs,
        winSize,
        3.50f,   // start X
        0.75f,   // Y
        0.005f,  // width
        0.03f,   // height
        50,      // 🔥 how many platforms
        0.005f,
        1000// gap between them
    );

    createPlatform(3.75f, 0.75f, 0.05f, 0.03f, TrapType::None);

    createFallRow(
        platforms,
        platformDefs,
        winSize,
        3.80f,   // start X
        0.75f,   // Y
        0.005f,  // width
        0.03f,   // height
        25,      // 🔥 how many platforms
        0.005f,
        1000// gap between them
    );

    createPlatform(4.10f, 0.45f, 0.30f, 0.03f, TrapType::Pressure);
    createPlatform(4.45f, 0.20f, 0.05f, 0.03f, TrapType::None);

    createPlatform(4.70f, -0.45f, 2.20f, 0.75f, TrapType::None);
    createPlatform(4.70f, 0.60f, 2.20f, 0.5f, TrapType::None);

    createPlatform(4.80f, 0.55f, 0.10f, 0.3f, TrapType::Pressure);

    createPlatform(5.80f, 0.45f, 0.10f, 0.02f, TrapType::None);
    createPlatform(6.00f, 0.45f, 0.10f, 0.02f, TrapType::None);

    createPlatform(6.90f, 0.65f, 0.12f, 0.03f, TrapType::MoveOnTouch);
    Platform& last4 = platforms.back();
    sf::Vector2f start4  = last4.shape.getPosition();
    sf::Vector2f end4 = start4 + sf::Vector2f(5000.f, 0.f);  // move forward
    setupMovingPlatform(last4, start4, end4, 200.f);
    last4.isMoving = false; // start stopped

    createPlatform(7.20f, 0.55f, 1.10f, 0.02f, TrapType::None);
    createPlatform(7.325f, 0.40f, 0.025f, 0.10f, TrapType::None);
    createPlatform(7.575f, 0.40f, 0.025f, 0.10f, TrapType::None);
    createPlatform(7.825f, 0.30f, 0.025f, 0.10f, TrapType::None);
    createPlatform(8.075f, 0.25f, 0.025f, 0.10f, TrapType::None);

}


void createSpike(
    std::vector<Spike>& spikes,
    sf::Vector2u winSize,
    float xNorm,
    float yNorm,
    float widthNorm,
    float heightNorm,
    bool ceiling = false,
    SpikeType type = SpikeType::Static)
{
    float px = xNorm * winSize.x;
    float py = yNorm * winSize.y;
    float pw = widthNorm * winSize.x;
    float ph = heightNorm * winSize.y;

    spikes.emplace_back(
        pw, ph,
        sf::Vector2f(px, py),
        ceiling,
        type
    );
}
void createSpikeRow(
    std::vector<Spike>& spikes,
    sf::Vector2u winSize,
    float startXNorm,
    float yNorm,
    float widthNorm,
    float heightNorm,
    int count,
    float spacingNorm,
    bool ceiling = false,
    SpikeType type = SpikeType::Static)
{
    for (int i = 0; i < count; i++)
    {
        createSpike(
            spikes,
            winSize,
            startXNorm + i * spacingNorm,
            yNorm,
            widthNorm,
            heightNorm,
            ceiling,
            type
        );
    }
}
void createSpikes(std::vector<Spike>& spikes, sf::Vector2u winSize)
{
    // Ground spikes
    createSpikeRow(
        spikes,
        winSize,
        5.05f,
        0.57f,
        0.01f,
        0.03f,
        7,
        0.01f,
        false,
        SpikeType::Static);

    // 🔥 Ceiling spikes
    createSpikeRow(
        spikes,
        winSize,
        4.90f,
        0.30f,
        0.01f,
        0.03f,
        10,
        0.01f,
        true,
        SpikeType::Static    // ceiling
    );
    // 🔥 Ceiling spikes
    createSpikeRow(
        spikes,
        winSize,
        5.12f,
        0.30f,
        0.01f,
        0.03f,
        10,
        0.01f,
        true,
        SpikeType::Static   // ceiling
    );

    // Ground spikes
    createSpikeRow(
        spikes,
        winSize,
        5.25f,
        0.57f,
        0.01f,
        0.03f,
        7,
        0.01f,
        false,
        SpikeType::Static // ground
    );
    // 🔥 Ceiling spikes
    createSpikeRow(
        spikes,
        winSize,
        5.35f,
        0.30f,
        0.01f,
        0.03f,
        10,
        0.01f,
        true,
        SpikeType::Static  // ceiling
    );

    // Ground spikes
    createSpikeRow(
        spikes,
        winSize,
        5.50f,
        0.57f,
        0.01f,
        0.03f,
        7,
        0.01f,
        false,
        SpikeType::Static  // ground
    );
    // 🔥 Ceiling spikes
    createSpikeRow(
        spikes,
        winSize,
        5.60f,
        0.30f,
        0.01f,
        0.03f,
        10,
        0.01f,
        true,
        SpikeType::Static   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        6.20f,
        0.30f,
        0.01f,
        0.03f,
        20,
        0.02f,
        true,
        SpikeType::FallingOnTrigger   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        6.60f,
        0.30f,
        0.01f,
        0.03f,
        4,
        0.02f,
        true,
        SpikeType::Static   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        5.90f,
        0.57f,
        0.01f,
        0.03f,
        7,
        0.01f,
        false,
        SpikeType::AppearWhenNear   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        6.00f,
        0.57f,
        0.01f,
        0.03f,
        7,
        0.01f,
        false,
        SpikeType::AppearWhenNear   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        6.70f,
        0.57f,
        0.01f,
        0.03f,
        5,
        0.01f,
        false,
        SpikeType::AppearWhenNear   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        7.20f,
        0.52f,
        0.01f,
        0.03f,
        55,
        0.02f,
        false,
        SpikeType::Static   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        8.20f,
        0.57f,
        0.01f,
        0.03f,
        1,
        0.02f,
        true,
        SpikeType::Static   // ceiling
    );
    createSpikeRow(
        spikes,
        winSize,
        0.0f,
        0.98f,
        0.01f,
        0.03f,
        500,
        0.02f,
        false,
        SpikeType::Static   // ceiling
    );
}

void spawnFallingSpike(std::vector<Spike>& spikes,
    sf::Vector2u winSize,
    float worldWidth)
{
    float width = 30.f;
    float height = 60.f;

    float x = static_cast<float>(rand()) / RAND_MAX;
    x *= worldWidth;

    float y = -100.f;

    spikes.emplace_back(width, height, sf::Vector2f(x, y), true);
}
// ================= UPDATE PLATFORMS =================
void updatePlatforms(std::vector<Platform>& platforms, Player& player, float dt)
{
    sf::FloatRect playerBounds = player.hitbox;

    float playerRight = playerBounds.position.x + playerBounds.size.x;
    float playerBottom = playerBounds.position.y + playerBounds.size.y;

    for (auto& p : platforms)
    {
        sf::FloatRect platBounds = p.shape.getGlobalBounds();
        float platLeft = platBounds.position.x;
        float platRight = platBounds.position.x + platBounds.size.x;
        float platTop = platBounds.position.y;
        float platBottom = platBounds.position.y + platBounds.size.y;

        // ===== MOVING PLATFORM UPDATE =====
        if (p.isMoving)
        {
            float step = p.moveSpeed * dt;

            if (!p.forward)
                step = -step;

            sf::Vector2f movement = p.moveDir * step;
            p.shape.move(movement);

            p.traveled += std::abs(step);

            if (p.traveled >= p.moveDistance)
            {
                p.forward = !p.forward;
                p.traveled = 0.f;
            }
        }
        // ===== PRESSURE PLATFORM =====
        if (p.trapType == TrapType::Pressure)
        {
            float currentY = p.shape.getPosition().y;
            float moveAmount = 0.f;

            if (p.isPressed)
            {
                if (currentY < p.originalY + p.maxDrop)
                    moveAmount = p.moveSpeedpresspt * dt;
            }
            else
            {
                if (currentY > p.originalY)
                    moveAmount = -p.moveSpeedpresspt * dt;
            }

            if (moveAmount != 0.f)
            {
                p.shape.move({ 0.f, moveAmount });

                // 🔥 MOVE PLAYER WITH PLATFORM (THE FIX)
                if (p.isPressed)
                {
                    player.sprite.move({ 0.f, moveAmount });
                    player.hitbox.position = player.sprite.getPosition() + player.colliderOffset;
                }
            }

            p.isPressed = false;
        }


        switch (p.trapType)
        {
        case TrapType::AppearWhenNear:
        {
            float playerCenterX = playerBounds.position.x + playerBounds.size.x / 2.f;
            float platCenterX = platBounds.position.x + platBounds.size.x / 2.f;

            float distance = std::abs(playerCenterX - platCenterX);

            if (distance <= p.triggerDistance)
                p.active = true;
            else
                p.active = false;
        }
        break;

        case TrapType::VanishOnTouch:
            if (p.active &&
                playerRight > platLeft &&
                playerBounds.position.x < platRight &&
                playerBottom >= platTop - 5.f) // tolerance
            {
                p.active = false;
                p.timer = 2.f; // respawn after 2 seconds
            }
            else if (!p.active)
            {
                p.timer -= dt;
                if (p.timer <= 0.f)
                    p.active = true; // respawn
            }
            break;

        case TrapType::FallOnTouch:
        {
            float playerLeft = playerBounds.position.x;

            // If platform inactive → count respawn timer
            if (!p.active)
            {
                p.respawnTimer -= dt;

                if (p.respawnTimer <= 0.f)
                {
                    p.shape.setPosition(p.originalPos);
                    p.triggered = false;
                    p.active = true;
                }

                break; // skip rest while inactive
            }

            // Trigger when player touches
            if (!p.triggered &&
                playerRight > platLeft &&
                playerLeft < platRight &&
                playerBottom >= platTop - 5.f)
            {
                p.triggered = true;
                p.fallTimer = p.fallDelay;
            }

            if (p.triggered)
            {
                // WAIT before falling
                if (p.fallTimer > 0.f)
                {
                    p.fallTimer -= dt;
                }
                else
                {
                    // Fall down
                    p.shape.move(sf::Vector2f(0.f, p.fallSpeed * dt));

                    // If fallen far enough → deactivate
                    if (p.shape.getPosition().y > p.originalPos.y + 600.f)
                    {
                        p.active = false;
                        p.respawnTimer = p.respawnDelay;
                    }
                }
            }
        }
        break;

        default:
            break;
        }
    }
}

void updateSpikes(std::vector<Spike>& spikes,
    const sf::FloatRect& playerBounds,
    Player& player,
    float dt,
    sf::Vector2u winSize)
{
    for (size_t i = 0; i < spikes.size(); )
    {
        auto& s = spikes[i];

        // ===== TYPE LOGIC =====
        if (s.type == SpikeType::AppearWhenNear)
        {
            float playerCenterX = playerBounds.position.x + playerBounds.size.x / 2.f;
            float spikeCenterX = s.shape.getPosition().x;

            float distance = std::abs(playerCenterX - spikeCenterX);

            if (distance < s.triggerDistance)
            {
                s.active = true;
                s.shape.setFillColor(sf::Color::White);
            }
        }
        if (s.type == SpikeType::FallingTimed)
        {
            s.timer += dt;

            if (s.timer >= s.interval)
            {
                s.triggered = true;
                s.timer = 0.f;
            }
        }

        if (s.type == SpikeType::FallingOnTrigger)
        {
            float playerX = player.sprite.getPosition().x;
            float spikeX = s.shape.getPosition().x;

            if (std::abs(playerX - spikeX) < 5.f)
                s.triggered = true;
        }

        // ===== MOVE ONLY IF TRIGGERED =====
        if (s.triggered)
        {
            s.shape.move({ 0.f, s.fallSpeed * dt });
        }

        sf::FloatRect spikeBounds = s.shape.getGlobalBounds();

        // 💀 DEATH CHECK
        if (spikeBounds.findIntersection(playerBounds).has_value())
        {
            float respawnX = levels[lastCompletedLevel].startX + 100.f;

            player.sprite.setPosition({ respawnX, 200.f });
            player.velocity = { 0.f, 0.f };
            player.resetJump();
            player.hitbox.position =
                player.sprite.getPosition() + player.colliderOffset;
        }

        // Remove fallen spikes
        if (s.shape.getPosition().y > winSize.y + 200.f)
        {
            spikes.erase(spikes.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}
// ================= MAIN =================
int main() {

    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();

    sf::RenderWindow window(
        desktop,
        "Ninja Cube Platformer",
        sf::State::Fullscreen
    );
    sf::Vector2u winSize = window.getSize();
    //float worldWidth = winSize.x * 3.f;

    float scaleX = (float)winSize.x / DESIGN_W;
    float scaleY = (float)winSize.y / DESIGN_H;

    window.setFramerateLimit(60);

    // ================= PARALLAX BACKGROUND =================
    // ===== FIXED TEXTURE STORAGE (NO REALLOCATION EVER) =====
    std::vector<sf::Texture> skyTextures(3);
    std::vector<sf::Texture> midTextures(3);
    std::vector<sf::Texture> frontTextures(3);

    float currentX = 0.f;
    float worldWidth = 0.f;

    // -------- SKY LAYER --------
    layers.emplace_back();
    layers.back().parallaxFactor = 0.2f;
    ParallaxLayer& skyLayer = layers.back();
    for (int i = 0; i < 3; ++i)
    {
        if (!skyTextures[i].loadFromFile("Assets/Sky_" + std::to_string(i + 1) + ".png"))
        {
            std::cout << "Failed to load Sky texture " << i + 1 << std::endl;
        }
        float scale = (float)window.getSize().y / skyTextures[i].getSize().y;
        float width = skyTextures[i].getSize().x * scale;

        for (int r = 0; r < 2; ++r)
        {
            sf::Sprite sprite(skyTextures[i]);
            sprite.setScale({ scale, scale });
            sprite.setPosition({ currentX, 0.f });

            skyLayer.sprites.push_back(sprite);
            skyLayer.basePositions.push_back(sprite.getPosition());
            currentX += width;
            if (i == 0) { break; }
        }
    }

    std::cout << layers[0].parallaxFactor << std::endl;
    worldWidth = currentX;
    // -------- MOUNTAIN MID LAYER --------
    currentX = 0.f;

    ParallaxLayer midLayer;
    midLayer.parallaxFactor = 0.25f;   // between sky and front

    for (int i = 0; i < 3; ++i)
    {
        if (!midTextures[i].loadFromFile("Assets/Mid_" + std::to_string(i + 1) + ".png"))
        {
            std::cout << "Failed to load Mid texture " << i + 1 << std::endl;
        }

        float scale = (float)window.getSize().y / midTextures[i].getSize().y;
        float width = midTextures[i].getSize().x * scale;

        for (int r = 0; r < 4; ++r)
        {
            sf::Sprite sprite(midTextures[i]);
            sprite.setScale({ scale, scale });
            sprite.setPosition({ currentX, 0.f });

            midLayer.sprites.push_back(sprite);
            midLayer.basePositions.push_back(sprite.getPosition());

            currentX += width;
        }
    }

    layers.push_back(midLayer);
    worldWidth = std::max(worldWidth, currentX);

    // -------- FRONT LAYER --------
    currentX = 0.f;

    ParallaxLayer frontLayer;
    frontLayer.parallaxFactor = 0.5f;   // faster movement

    for (int i = 0; i < 3; ++i)
    {
        frontTextures[i].loadFromFile("Assets/Front_" + std::to_string(i + 1) + ".png");

        float scale = (float)window.getSize().y / frontTextures[i].getSize().y;
        float width = frontTextures[i].getSize().x * scale;

        for (int r = 0; r < 5; ++r)
        {
            sf::Sprite sprite(frontTextures[i]);
            sprite.setScale({ scale, scale });
            sprite.setPosition({ currentX, 0.f });


            frontLayer.sprites.push_back(sprite);
            frontLayer.basePositions.push_back(sprite.getPosition());  // 🔥 ADD THIS
            currentX += width;
        }
    }

    layers.push_back(frontLayer);
    worldWidth = std::max(worldWidth, currentX);

    levels.clear();

    // Manually define based on your design
    levels.push_back({ 200.f, 2630.f });
    levels.push_back({ 2630.f, 6050.f });
    levels.push_back({ 6230.f, 9000.f });
    levels.push_back({ 9200.f, 12000.f });
    levels.push_back({ 13000.f, 15000.f });

    std::vector<Platform> platforms;
    float platformScale = 0.5f;
    createPlatforms(platforms, platformDefs, winSize);
    std::vector<Spike> spikes;
    createSpikes(spikes, winSize);

    // Player
    sf::Texture ninjaTexture;
    ninjaTexture.loadFromFile("Assets/ninja_5.png");

    sf::Vector2f startPos;

    #ifdef DEV_START_POS
        // 🔥 Developer test position
        startPos = sf::Vector2f(200, 0.0f);
    #else
        // Normal spawn on first platform
        float playerHalfHeight = (ninjaTexture.getSize().y * 0.15f) / 2.f;
        sf::Vector2f platPos = platforms[0].shape.getPosition();
        float platHalfHeight = platforms[0].shape.getSize().y / 2.f;

        startPos = sf::Vector2f(
            platPos.x,
            platPos.y - platHalfHeight - playerHalfHeight
        );
    #endif

    Player player(ninjaTexture, startPos);

    player.spawnPos = player.sprite.getPosition(); // store spawn position
    player.resetJump();

    sf::Clock clock;

    sf::View view(sf::FloatRect(
        { 0.f, 0.f },
        { (float)window.getSize().x, (float)window.getSize().y }
    ));

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();


            // ---------------- KEY PRESSED ----------------
            else if (event->is<sf::Event::KeyPressed>())
            {
                if (auto keyEvent = event->getIf<sf::Event::KeyPressed>())
                {
                    if (keyEvent->code == sf::Keyboard::Key::Escape)
                        window.close();

                    if (keyEvent->code == sf::Keyboard::Key::Up)
                    {
                        player.jumpHeld = true;
                        player.tryJump();
                    }

                    if (keyEvent->code == sf::Keyboard::Key::LControl ||
                        keyEvent->code == sf::Keyboard::Key::RControl)
                    {
                        player.sprite.setPosition(player.spawnPos);
                        player.velocity = { 0.f, 0.f };
                        player.resetJump();
                    }
                }
            }

            // ---------------- KEY RELEASED ----------------
            else if (event->is<sf::Event::KeyReleased>())
            {
                if (auto keyEvent = event->getIf<sf::Event::KeyReleased>())
                {
                    if (keyEvent->code == sf::Keyboard::Key::Up)
                        player.jumpHeld = false;
                }
            }

            // ---------------- WINDOW RESIZED ----------------
        }

        float dt = clock.restart().asSeconds();

        bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);

        //player.handleHorizontal(dt, left, right);

        //player.applyGravity(dt);
        player.update(dt);


        player.movePlayer(player, dt, platforms);
        // 🔥 LEVEL PROGRESSION CHECK
        float playerX = player.sprite.getPosition().x;

        if (currentLevel < levels.size())
        {
            if (playerX > levels[currentLevel].endX)
            {
                lastCompletedLevel = currentLevel;
                currentLevel++;

                std::cout << "Level " << currentLevel << " completed!\n";
            }
        }

        updatePlatforms(platforms, player, dt);


        // ================= SPIKE SPAWNER =================

        updateSpikes(spikes, player.hitbox, player, dt, winSize);

        sf::Vector2f center = player.sprite.getPosition();
        float halfWidth = view.getSize().x / 2.f;

        if (center.x < halfWidth) center.x = halfWidth;
        if (center.x > worldWidth - halfWidth)
            center.x = worldWidth - halfWidth;

        center.y = window.getSize().y / 2.f;

        view.setCenter(center);
        window.setView(view);

        window.clear(sf::Color(20, 20, 30));

        float viewLeft = view.getCenter().x - view.getSize().x / 2.f;

        // =========================================
        // 1️⃣ DRAW SKY (fixed to screen)
        // =========================================
        sf::View defaultView = window.getDefaultView();
        window.setView(defaultView);

        for (size_t l = 0; l < layers.size(); ++l)
        {
            for (size_t i = 0; i < layers[l].sprites.size(); ++i)
            {
                layers[l].sprites[i].setPosition({
                    layers[l].basePositions[i].x - viewLeft * layers[l].parallaxFactor,
                    layers[l].basePositions[i].y
                    });

                window.draw(layers[l].sprites[i]);
            }
        }

        // =========================================
        // DRAW GAME WORLD
        // =========================================
        window.setView(view);

        // Draw platforms
        for (auto& p : platforms)
        {
            if (p.active)
                window.draw(p.shape);
        }

        for (auto& s : spikes)
            window.draw(s.shape);

        window.draw(player.sprite);
        // Draw platforms
        for (auto& p : platforms)
        {
            if (p.active)
                window.draw(p.shape);
        }
        for (auto& s : spikes)
            window.draw(s.shape);

        // Draw player
        window.draw(player.sprite);

        window.display();
    }
}