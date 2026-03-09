#include <SFML/Graphics.hpp>
#include <iostream>
#include <optional>
#include <algorithm>
#include <cmath>
#include <array>
#include <string>
#include <vector>
#include <random>

static constexpr float PI = 3.14159265358979f;
static constexpr float RAD2DEG = 180.f / PI;

struct Enemy
{
    sf::Sprite sprite;
    int   animFrame   = 0;
    float animTimer   = 0.f;
    float hp          = 100.f;
    float attackTimer = 0.f;   // counts down; attack fires when <= 0

    // Hit-flash
    bool  isHit    = false;
    int   hitFrame = 0;
    float hitTimer = 0.f;

    // Death animation
    bool  isDying    = false;
    int   deathFrame = 0;
    float deathTimer = 0.f;

    explicit Enemy(const sf::Texture& tex) : sprite(tex)
    {
        auto size = tex.getSize();
        sprite.setOrigin({size.x / 2.f, size.y / 2.f});
        sprite.setScale({0.1f, 0.1f});
    }
};

struct Bullet
{
    sf::Sprite sprite;
    sf::Vector2f velocity;
    float lifetime = 3.f;

    Bullet(const sf::Texture& tex, sf::Vector2f pos, sf::Vector2f vel)
        : sprite(tex), velocity(vel)
    {
        auto sz = tex.getSize();
        sprite.setOrigin({sz.x / 2.f, sz.y / 2.f});
        sprite.setScale({0.008f, 0.008f});
        sprite.setPosition(pos);
    }
};

int main()
{
    std::cout << "Game started\n";

    const unsigned int WINDOW_WIDTH  = 1280;
    const unsigned int WINDOW_HEIGHT = 720;

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Zombie Shooter");
    window.setFramerateLimit(60);
    window.setMouseCursorVisible(false);

    // ── Textures ──────────────────────────────────────────────────────────────

    sf::Texture grassTexture;
    if (!grassTexture.loadFromFile("assets/Extra/Background/PNG/Default/tileGrass_slope.png"))
    { std::cout << "Failed to load tileGrass_slope.png\n"; return 1; }

    const std::string charPath  = "assets/Extra/Background/Characters/Body/Char_1/hands/";
    const std::string enemyPath = "assets/Extra/Background/Characters/Body/Enemies/Enemy_1/";
    const std::string extrasPath = "assets/Extra/Background/Characters/Extras/";

    std::array<sf::Texture, 8> walkTextures;
    for (int i = 0; i < 8; ++i)
        if (!walkTextures[i].loadFromFile(charPath + "walk_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load walk_" << i << "\n"; return 1; }

    std::array<sf::Texture, 6> idleTextures;
    for (int i = 0; i < 6; ++i)
        if (!idleTextures[i].loadFromFile(charPath + "idle_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load idle_" << i << "\n"; return 1; }

    std::array<sf::Texture, 3> playerHitTextures;
    for (int i = 0; i < 3; ++i)
        if (!playerHitTextures[i].loadFromFile(charPath + "hit_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load player hit_" << i << "\n"; return 1; }

    std::array<sf::Texture, 10> playerDeathTextures;
    for (int i = 0; i < 10; ++i)
        if (!playerDeathTextures[i].loadFromFile(charPath + "death_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load player death_" << i << "\n"; return 1; }

    std::array<sf::Texture, 8> enemyWalkTextures;
    for (int i = 0; i < 8; ++i)
        if (!enemyWalkTextures[i].loadFromFile(enemyPath + "walk_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load enemy walk_" << i << "\n"; return 1; }

    std::array<sf::Texture, 3> enemyHitTextures;
    for (int i = 0; i < 3; ++i)
        if (!enemyHitTextures[i].loadFromFile(enemyPath + "hit_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load enemy hit_" << i << "\n"; return 1; }

    std::array<sf::Texture, 10> enemyDeathTextures;
    for (int i = 0; i < 10; ++i)
        if (!enemyDeathTextures[i].loadFromFile(enemyPath + "death_" + std::to_string(i) + ".png"))
        { std::cout << "Failed to load enemy death_" << i << "\n"; return 1; }

    sf::Texture weaponTexture;
    if (!weaponTexture.loadFromFile("assets/Extra/Background/Characters/Weapons/weaponR2.png"))
    { std::cout << "Failed to load weaponR2.png\n"; return 1; }

    sf::Texture bulletTexture;
    if (!bulletTexture.loadFromFile(extrasPath + "bullet.png"))
    { std::cout << "Failed to load bullet.png\n"; return 1; }

    sf::Texture muzzleTexture;
    if (!muzzleTexture.loadFromFile(extrasPath + "muzzle.png"))
    { std::cout << "Failed to load muzzle.png\n"; return 1; }

    sf::Texture crosshairTexture;
    if (!crosshairTexture.loadFromFile(extrasPath + "crosshair.png"))
    { std::cout << "Failed to load crosshair.png\n"; return 1; }

    sf::Font font;
    bool fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");
    if (!fontLoaded) std::cout << "Warning: could not load font, game-over text disabled\n";

    // ── Sprites ───────────────────────────────────────────────────────────────

    sf::Sprite grassSprite(grassTexture);

    sf::Sprite playerSprite(walkTextures[0]);
    {
        auto sz = walkTextures[0].getSize();
        playerSprite.setOrigin({sz.x / 2.f, sz.y / 2.f});
        playerSprite.setScale({0.1f, 0.1f});
    }

    const float weaponScale = 0.05f;
    sf::Sprite weapSprite(weaponTexture);
    {
        auto sz = weaponTexture.getSize();
        // Gun graphic sits at ~82% down in the 2048px canvas, grip at ~27% from left
        weapSprite.setOrigin({sz.x * 0.4f, sz.y * 0.6f});
        weapSprite.setScale({weaponScale, weaponScale});
    }

    const float muzzleScale = 0.015f;
    sf::Sprite muzzleSprite(muzzleTexture);
    {
        auto sz = muzzleTexture.getSize();
        muzzleSprite.setOrigin({sz.x / 2.f, sz.y / 2.f});
        muzzleSprite.setScale({muzzleScale, muzzleScale});
    }

    sf::Sprite crosshairSprite(crosshairTexture);
    {
        auto sz = crosshairTexture.getSize();
        crosshairSprite.setOrigin({sz.x / 2.f, sz.y / 2.f});
        crosshairSprite.setScale({0.5f, 0.5f});
    }

    // ── World ─────────────────────────────────────────────────────────────────

    const int   tileSize    = 64;
    const float playerSpeed = 300.f;
    const float worldWidth  = 3000.f;
    const float worldHeight = 3000.f;

    playerSprite.setPosition({worldWidth / 2.f, worldHeight / 2.f});

    sf::View camera(sf::FloatRect({0.f, 0.f},
        {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)}));
    camera.setCenter(playerSprite.getPosition());

    sf::RectangleShape borderTop({worldWidth, 20.f});
    borderTop.setPosition({0.f, 0.f});
    borderTop.setFillColor(sf::Color::Red);

    sf::RectangleShape borderBottom({worldWidth, 20.f});
    borderBottom.setPosition({0.f, worldHeight - 20.f});
    borderBottom.setFillColor(sf::Color::Red);

    sf::RectangleShape borderLeft({20.f, worldHeight});
    borderLeft.setPosition({0.f, 0.f});
    borderLeft.setFillColor(sf::Color::Red);

    sf::RectangleShape borderRight({20.f, worldHeight});
    borderRight.setPosition({worldWidth - 20.f, 0.f});
    borderRight.setFillColor(sf::Color::Red);

    // ── Player animation state ────────────────────────────────────────────────

    int   animFrame      = 0;
    float animTimer      = 0.f;
    bool  wasWalking     = false;
    const float walkFrameTime  = 0.08f;
    const float idleFrameTime  = 0.12f;
    const float hitFrameTime   = 0.06f;   // 3-frame hit flash  (~0.18 s)
    const float deathFrameTime = 0.10f;   // 10-frame death anim (~1.0 s)

    // ── Player hit / death state ──────────────────────────────────────────────

    bool  playerIsHit      = false;
    int   playerHitFrame   = 0;
    float playerHitTimer   = 0.f;

    bool  playerDead       = false;
    int   playerDeathFrame = 0;
    float playerDeathTimer = 0.f;
    bool  deathAnimDone    = false;

    // ── Enemy state ───────────────────────────────────────────────────────────

    std::vector<Enemy> enemies;
    float spawnTimer = 0.f;
    const float spawnInterval  = 2.f;
    const float enemySpeed     = 120.f;
    const float enemyFrameTime = 0.1f;

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int>   edgeDist(0, 3);
    std::uniform_real_distribution<float> xDist(20.f, worldWidth  - 20.f);
    std::uniform_real_distribution<float> yDist(20.f, worldHeight - 20.f);

    // ── Bullet & muzzle flash state ───────────────────────────────────────────

    std::vector<Bullet> bullets;
    const float bulletSpeed     = 900.f;
    bool  showMuzzle            = false;
    float muzzleTimer           = 0.f;
    const float muzzleDuration  = 0.08f;

    // ── Player health state ───────────────────────────────────────────────────

    const float playerMaxHp      = 100.f;
    float       playerHp         = playerMaxHp;
    float       playerRegenDelay = 0.f;        // countdown before regen kicks in
    const float regenDelaySec    = 3.f;        // seconds after last hit before regen
    const float regenRate        = 12.f;       // hp per second
    const float enemyAttackDamage   = 5.f;     // hp lost per hit
    const float enemyAttackInterval = 0.3f;   // seconds between hits
    const float enemyContactRadius  = 60.f;   // world-px melee range

    // Reusable health-bar shapes
    sf::RectangleShape hpBarBg, hpBarFill;
    hpBarBg  .setFillColor(sf::Color(120, 0, 0));
    hpBarFill.setFillColor(sf::Color(50, 220, 50));

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // ── Events ────────────────────────────────────────────────────────────

        bool shouldShoot = false;
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                {
                    if (playerDead && deathAnimDone)
                    {
                        // Hit-test the restart button (screen space, centred on window)
                        float mx = static_cast<float>(mb->position.x);
                        float my = static_cast<float>(mb->position.y);
                        float bx = WINDOW_WIDTH  / 2.f;
                        float by = WINDOW_HEIGHT / 2.f + 50.f;
                        if (mx >= bx - 100.f && mx <= bx + 100.f &&
                            my >= by -  30.f && my <= by +  30.f)
                        {
                            // ── Restart ──────────────────────────────────────
                            playerHp         = playerMaxHp;
                            playerDead       = false;
                            playerDeathFrame = 0;
                            playerDeathTimer = 0.f;
                            deathAnimDone    = false;
                            playerIsHit      = false;
                            playerHitFrame   = 0;
                            playerHitTimer   = 0.f;
                            playerRegenDelay = 0.f;
                            enemies.clear();
                            bullets.clear();
                            playerSprite.setPosition({worldWidth / 2.f, worldHeight / 2.f});
                            spawnTimer = 0.f;
                            showMuzzle = false;
                            animFrame  = 0;
                            animTimer  = 0.f;
                        }
                    }
                    else if (!playerDead)
                    {
                        shouldShoot = true;
                    }
                }
            }
        }

        // ── Player death animation advance ────────────────────────────────────

        if (playerDead && !deathAnimDone)
        {
            playerDeathTimer += dt;
            if (playerDeathTimer >= deathFrameTime)
            {
                playerDeathTimer -= deathFrameTime;
                if (++playerDeathFrame >= 10) { playerDeathFrame = 9; deathAnimDone = true; }
            }
        }

        // ── Player input ──────────────────────────────────────────────────────

        sf::Vector2f movement = {0.f, 0.f};
        if (!playerDead)
        {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) movement.y -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) movement.y += 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) movement.x -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) movement.x += 1.f;

            if (movement.x != 0.f || movement.y != 0.f)
            {
                float len = std::sqrt(movement.x * movement.x + movement.y * movement.y);
                movement.x /= len;
                movement.y /= len;
            }
        }

        // ── Player animation ──────────────────────────────────────────────────

        bool isWalking = (movement.x != 0.f || movement.y != 0.f);
        const sf::Texture* playerTexPtr = nullptr;

        if (playerDead)
        {
            // Death frame is advanced in the dedicated block above; just read it.
            playerTexPtr = &playerDeathTextures[playerDeathFrame];
        }
        else if (playerIsHit)
        {
            playerHitTimer += dt;
            if (playerHitTimer >= hitFrameTime)
            {
                playerHitTimer -= hitFrameTime;
                if (++playerHitFrame >= 3) { playerIsHit = false; playerHitFrame = 0; }
            }
            // After the last hit frame, fall back to walk/idle for this frame
            playerTexPtr = playerIsHit
                ? &playerHitTextures[playerHitFrame]
                : (isWalking ? &walkTextures[animFrame] : &idleTextures[animFrame]);
        }
        else
        {
            if (isWalking != wasWalking) { animFrame = 0; animTimer = 0.f; wasWalking = isWalking; }
            animTimer += dt;
            float frameTime  = isWalking ? walkFrameTime : idleFrameTime;
            int   frameCount = isWalking ? 8 : 6;
            if (animTimer >= frameTime) { animTimer -= frameTime; animFrame = (animFrame + 1) % frameCount; }
            playerTexPtr = isWalking ? &walkTextures[animFrame] : &idleTextures[animFrame];
        }

        playerSprite.setTexture(*playerTexPtr);
        auto texSize = playerTexPtr->getSize();
        playerSprite.setOrigin({texSize.x / 2.f, texSize.y / 2.f});

        // ── Player movement & clamping ────────────────────────────────────────

        playerSprite.move(movement * playerSpeed * dt);

        sf::FloatRect pb = playerSprite.getGlobalBounds();
        sf::Vector2f playerPos = playerSprite.getPosition();
        playerPos.x = std::clamp(playerPos.x, pb.size.x / 2.f, worldWidth  - pb.size.x / 2.f);
        playerPos.y = std::clamp(playerPos.y, pb.size.y / 2.f, worldHeight - pb.size.y / 2.f);
        playerSprite.setPosition(playerPos);

        // ── Camera ────────────────────────────────────────────────────────────

        sf::Vector2f camCenter = playerSprite.getPosition();
        camCenter.x = std::clamp(camCenter.x, WINDOW_WIDTH  / 2.f, worldWidth  - WINDOW_WIDTH  / 2.f);
        camCenter.y = std::clamp(camCenter.y, WINDOW_HEIGHT / 2.f, worldHeight - WINDOW_HEIGHT / 2.f);
        camera.setCenter(camCenter);
        window.setView(camera);

        // ── Aim direction (world-space mouse) ─────────────────────────────────

        sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window), camera);
        sf::Vector2f aimDir     = mouseWorld - playerSprite.getPosition();
        float aimLen = std::sqrt(aimDir.x * aimDir.x + aimDir.y * aimDir.y);
        if (aimLen > 0.f) aimDir /= aimLen;
        float aimAngle = std::atan2(aimDir.y, aimDir.x) * RAD2DEG;

        // ── Player flip ───────────────────────────────────────────────────────

        float scaleX = (aimDir.x < 0.f) ? -0.1f : 0.1f;
        playerSprite.setScale({scaleX, 0.1f});

        // ── Weapon positioning & rotation ─────────────────────────────────────
        // Gun sits below the player centre, so recompute aim from weapPos so
        // the rotation angle matches where bullets actually go.

        sf::Vector2f weapPos = playerSprite.getPosition() + sf::Vector2f(0.f, 45.f);

        sf::Vector2f weapAimDir = mouseWorld - weapPos;
        float weapAimLen = std::sqrt(weapAimDir.x * weapAimDir.x + weapAimDir.y * weapAimDir.y);
        if (weapAimLen > 0.f) weapAimDir /= weapAimLen;
        float weapAimAngle = std::atan2(weapAimDir.y, weapAimDir.x) * RAD2DEG;

        bool  aimLeft = (weapAimDir.x < 0.f);
        float wsy     = aimLeft ? -weaponScale : weaponScale;

        weapSprite.setPosition(weapPos);
        weapSprite.setScale({weaponScale, wsy});
        weapSprite.setRotation(sf::degrees(weapAimAngle));

        // Barrel tip: grip at 27%, barrel end at ~61% of the 2048px canvas → 34% of width
        float barrelLength = weaponTexture.getSize().x * 0.45f * weaponScale;

        // Perpendicular offset keeps the muzzle aligned with the gun barrel.
        // Flip sign when aiming left because the gun sprite is Y-flipped.
        sf::Vector2f perp = {-weapAimDir.y, weapAimDir.x};
        float sideOffset = aimLeft ? -10.f : 10.f;

        sf::Vector2f barrelTip =
            weapPos
            + weapAimDir * barrelLength
            + perp * sideOffset;

        // ── Shooting ──────────────────────────────────────────────────────────

        if (shouldShoot)
        {
            // Aim bullet from barrelTip directly toward the crosshair
            sf::Vector2f bulletDir = mouseWorld - barrelTip;
            float bdLen = std::sqrt(bulletDir.x * bulletDir.x + bulletDir.y * bulletDir.y);
            if (bdLen > 0.f) bulletDir /= bdLen;
            bullets.emplace_back(bulletTexture, barrelTip, bulletDir * bulletSpeed);
            showMuzzle  = true;
            muzzleTimer = muzzleDuration;
        }

        // ── Muzzle flash timer ────────────────────────────────────────────────

        if (showMuzzle)
        {
            muzzleTimer -= dt;
            if (muzzleTimer <= 0.f) showMuzzle = false;

            muzzleSprite.setPosition(barrelTip);
            muzzleSprite.setRotation(sf::degrees(weapAimAngle));
            muzzleSprite.setScale({muzzleScale, muzzleScale});
        }

        // ── Bullet update + enemy hit detection ───────────────────────────────

        for (auto& b : bullets)
        {
            b.sprite.move(b.velocity * dt);
            b.lifetime -= dt;

            if (b.lifetime <= 0.f) continue;

            for (auto& e : enemies)
            {
                if (e.isDying) continue;   // can't shoot already-dying enemies

                // The sprite origin is at the canvas centre (top of head area),
                // so shift the hit-centre down into the torso.
                sf::Vector2f eBody = e.sprite.getPosition() + sf::Vector2f(0.f, 40.f);
                sf::Vector2f d = b.sprite.getPosition() - eBody;
                float dist = std::sqrt(d.x * d.x + d.y * d.y);
                if (dist < 60.f)
                {
                    e.hp -= 40.f;
                    b.lifetime = 0.f;
                    if (e.hp <= 0.f)
                    {
                        // Start death animation
                        e.isDying    = true;
                        e.deathFrame = 0;
                        e.deathTimer = 0.f;
                        e.isHit      = false;
                    }
                    else
                    {
                        // Start hit-flash
                        e.isHit    = true;
                        e.hitFrame = 0;
                        e.hitTimer = 0.f;
                    }
                    break;
                }
            }
        }

        // Remove dead bullets and dead enemies
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [&](const Bullet& b) {
                auto p = b.sprite.getPosition();
                return b.lifetime <= 0.f ||
                       p.x < 0.f || p.x > worldWidth ||
                       p.y < 0.f || p.y > worldHeight;
            }), bullets.end());

        enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
            [](const Enemy& e) { return e.isDying && e.deathFrame >= 10; }), enemies.end());

        // ── Enemy spawn ───────────────────────────────────────────────────────

        spawnTimer += dt;
        if (spawnTimer >= spawnInterval)
        {
            spawnTimer = 0.f;
            enemies.emplace_back(enemyWalkTextures[0]);
            Enemy& e = enemies.back();
            int edge = edgeDist(rng);
            if      (edge == 0) e.sprite.setPosition({xDist(rng), 20.f});
            else if (edge == 1) e.sprite.setPosition({xDist(rng), worldHeight - 20.f});
            else if (edge == 2) e.sprite.setPosition({20.f, yDist(rng)});
            else                e.sprite.setPosition({worldWidth - 20.f, yDist(rng)});
        }

        // ── Enemy update ──────────────────────────────────────────────────────

        for (auto& e : enemies)
        {
            if (e.isDying)
            {
                // Advance death animation; movement frozen
                e.deathTimer += dt;
                if (e.deathTimer >= deathFrameTime)
                {
                    e.deathTimer -= deathFrameTime;
                    ++e.deathFrame;   // removal happens when deathFrame >= 10
                }
                int df = std::min(e.deathFrame, 9);
                const sf::Texture& dTex = enemyDeathTextures[df];
                e.sprite.setTexture(dTex);
                auto dsz = dTex.getSize();
                e.sprite.setOrigin({dsz.x / 2.f, dsz.y / 2.f});
                continue;
            }

            // Chase player
            sf::Vector2f dir = playerSprite.getPosition() - e.sprite.getPosition();
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.f) dir /= len;

            if (!playerDead)
                e.sprite.move(dir * enemySpeed * dt);

            float esx = (dir.x < 0.f) ? -0.1f : 0.1f;
            e.sprite.setScale({esx, 0.1f});

            // Choose texture: hit-flash takes priority over walk
            const sf::Texture* eTexPtr = nullptr;
            if (e.isHit)
            {
                e.hitTimer += dt;
                if (e.hitTimer >= hitFrameTime)
                {
                    e.hitTimer -= hitFrameTime;
                    if (++e.hitFrame >= 3) { e.isHit = false; e.hitFrame = 0; }
                }
                eTexPtr = e.isHit ? &enemyHitTextures[e.hitFrame]
                                  : &enemyWalkTextures[e.animFrame];
            }
            else
            {
                e.animTimer += dt;
                if (e.animTimer >= enemyFrameTime)
                { e.animTimer -= enemyFrameTime; e.animFrame = (e.animFrame + 1) % 8; }
                eTexPtr = &enemyWalkTextures[e.animFrame];
            }

            e.sprite.setTexture(*eTexPtr);
            auto esz = eTexPtr->getSize();
            e.sprite.setOrigin({esz.x / 2.f, esz.y / 2.f});
        }

        // ── Player contact damage & health regen ─────────────────────────────

        bool touchingEnemy = false;
        if (!playerDead)
        {
            for (auto& e : enemies)
            {
                if (e.isDying) continue;   // dying enemies don't attack

                sf::Vector2f eBody = e.sprite.getPosition() + sf::Vector2f(0.f, 40.f);
                sf::Vector2f d = eBody - playerSprite.getPosition();
                float dist = std::sqrt(d.x * d.x + d.y * d.y);
                if (dist < enemyContactRadius)
                {
                    touchingEnemy = true;
                    e.attackTimer -= dt;
                    if (e.attackTimer <= 0.f)
                    {
                        playerHp -= enemyAttackDamage;
                        playerRegenDelay = regenDelaySec;
                        e.attackTimer = enemyAttackInterval;
                        // Trigger player hit-flash
                        playerIsHit    = true;
                        playerHitFrame = 0;
                        playerHitTimer = 0.f;
                    }
                }
            }
            playerHp = std::clamp(playerHp, 0.f, playerMaxHp);

            // Death check
            if (playerHp <= 0.f)
            {
                playerDead       = true;
                playerDeathFrame = 0;
                playerDeathTimer = 0.f;
                deathAnimDone    = false;
                playerIsHit      = false;
                showMuzzle       = false;
            }

            if (!touchingEnemy)
            {
                if (playerRegenDelay > 0.f)
                    playerRegenDelay -= dt;
                else if (playerHp < playerMaxHp)
                    playerHp = std::min(playerMaxHp, playerHp + regenRate * dt);
            }
        }

        // ── Draw ──────────────────────────────────────────────────────────────

        window.clear();

        // World
        for (int y = 0; y < static_cast<int>(worldHeight); y += tileSize)
            for (int x = 0; x < static_cast<int>(worldWidth); x += tileSize)
            {
                grassSprite.setPosition({static_cast<float>(x), static_cast<float>(y)});
                window.draw(grassSprite);
            }

        window.draw(borderTop);
        window.draw(borderBottom);
        window.draw(borderLeft);
        window.draw(borderRight);

        // Enemies
        for (auto& e : enemies)
            window.draw(e.sprite);

        // Bullets
        for (auto& b : bullets)
            window.draw(b.sprite);

        // Player + weapon + muzzle flash
        window.draw(playerSprite);
        if (!playerDead)
        {
            window.draw(weapSprite);
            if (showMuzzle)
                window.draw(muzzleSprite);
        }

        // ── Health bars (world space) ─────────────────────────────────────────

        const float eBarW = 30.f, eBarH = 4.f;
        for (auto& e : enemies)
        {
            if (e.hp >= 100.f) continue;
            float frac = std::max(0.f, e.hp / 100.f);
            sf::Vector2f pos = { e.sprite.getPosition().x - eBarW / 2.f,
                                 e.sprite.getPosition().y - 22.f };
            hpBarBg.setSize({eBarW, eBarH});
            hpBarBg.setPosition(pos);
            window.draw(hpBarBg);

            hpBarFill.setSize({eBarW * frac, eBarH});
            hpBarFill.setPosition(pos);
            window.draw(hpBarFill);
        }

        // Player health bar – only when not at full health
        if (playerHp < playerMaxHp)
        {
            const float pBarW = 40.f, pBarH = 5.f;
            float frac = playerHp / playerMaxHp;
            // Colour shifts green → yellow → red as hp drops
            sf::Color fillCol = frac > 0.5f
                ? sf::Color(static_cast<uint8_t>((1.f - frac) * 2.f * 255), 220, 0)
                : sf::Color(220, static_cast<uint8_t>(frac * 2.f * 220), 0);
            hpBarFill.setFillColor(fillCol);

            sf::Vector2f pos = { playerSprite.getPosition().x - pBarW / 2.f,
                                 playerSprite.getPosition().y - 32.f };
            hpBarBg.setSize({pBarW, pBarH});
            hpBarBg.setPosition(pos);
            window.draw(hpBarBg);

            hpBarFill.setSize({pBarW * frac, pBarH});
            hpBarFill.setPosition(pos);
            window.draw(hpBarFill);

            // Reset enemy bar colour for next frame
            hpBarFill.setFillColor(sf::Color(50, 220, 50));
        }

        // HUD – crosshair in screen space (hidden when dead)
        window.setView(window.getDefaultView());
        // Show the real cursor only on the game-over screen so the player can click Restart
        window.setMouseCursorVisible(playerDead && deathAnimDone);
        if (!playerDead)
        {
            crosshairSprite.setPosition(sf::Vector2f(sf::Mouse::getPosition(window)));
            window.draw(crosshairSprite);
        }

        // ── Game-over overlay ─────────────────────────────────────────────────
        if (playerDead && deathAnimDone)
        {
            // Dark tint
            sf::RectangleShape overlay(
                {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            overlay.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(overlay);

            // Restart button
            sf::RectangleShape btn({200.f, 60.f});
            btn.setOrigin({100.f, 30.f});
            btn.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f + 50.f});
            btn.setFillColor(sf::Color(40, 40, 200));
            window.draw(btn);

            if (fontLoaded)
            {
                sf::Text gameOverText(font, "GAME OVER", 52);
                gameOverText.setFillColor(sf::Color::Red);
                auto gb = gameOverText.getLocalBounds();
                gameOverText.setOrigin(
                    {gb.position.x + gb.size.x / 2.f, gb.position.y + gb.size.y / 2.f});
                gameOverText.setPosition(
                    {WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f - 40.f});
                window.draw(gameOverText);

                sf::Text btnText(font, "RESTART", 28);
                btnText.setFillColor(sf::Color::White);
                auto bb = btnText.getLocalBounds();
                btnText.setOrigin(
                    {bb.position.x + bb.size.x / 2.f, bb.position.y + bb.size.y / 2.f});
                btnText.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f + 50.f});
                window.draw(btnText);
            }
        }

        window.display();
    }

    return 0;
}
