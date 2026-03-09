#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <optional>
#include <algorithm>
#include <cmath>
#include <array>
#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

static constexpr float PI = 3.14159265358979f;
static constexpr float RAD2DEG = 180.f / PI;

// ── Weapon definitions ────────────────────────────────────────────────────────
struct WeaponDef
{
    const char* name;
    const char* texFile;
    int         cost;        // coins to unlock
    float       damage;      // hp removed per bullet
    float       fireDelay;   // minimum seconds between shots
};

static const WeaponDef WEAPONS[] = {
    { "Pistol",  "weaponR2.png",   0,  40.f, 0.40f },
    { "AK-47",   "ak47",         150,  35.f, 0.10f },
    { "Shotgun", "weaponR3.png", 300,  25.f, 0.70f },
};
static constexpr int NUM_WEAPONS = 3;

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
        sprite.setScale({2.0f, 2.0f});
        sprite.setPosition(pos);
        float angle = std::atan2(vel.y, vel.x) * RAD2DEG;
        sprite.setRotation(sf::degrees(angle));
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

    std::array<sf::Texture, NUM_WEAPONS> weaponTextures;
    for (int i = 0; i < NUM_WEAPONS; ++i)
    {
        std::string p;
        if (i == 1) // AK-47: idle display uses first shoot frame
            p = "assets/Extra/Background/PNG/Guns/AK-47/Shoot/tile000.png";
        else
        {
            p = "assets/Extra/Background/Characters/Weapons/";
            p += WEAPONS[i].texFile;
        }
        if (!weaponTextures[i].loadFromFile(p))
        { std::cout << "Failed to load weapon " << i << "\n"; return 1; }
    }

    // ── AK-47 animation textures ──────────────────────────────────────────────
    const std::string ak47BasePath = "assets/Extra/Background/PNG/Guns/AK-47/";
    std::array<sf::Texture, 12> ak47ShootTex;
    for (int i = 0; i < 12; ++i)
    {
        std::ostringstream ss;
        ss << ak47BasePath << "Shoot/tile" << std::setfill('0') << std::setw(3) << i << ".png";
        if (!ak47ShootTex[i].loadFromFile(ss.str()))
        { std::cout << "Failed to load AK-47 shoot frame " << i << "\n"; return 1; }
    }
    std::array<sf::Texture, 28> ak47ReloadTex;
    for (int i = 0; i < 28; ++i)
    {
        std::ostringstream ss;
        ss << ak47BasePath << "Reload/tile" << std::setfill('0') << std::setw(3) << i << ".png";
        if (!ak47ReloadTex[i].loadFromFile(ss.str()))
        { std::cout << "Failed to load AK-47 reload frame " << i << "\n"; return 1; }
    }

    sf::Texture bulletTexture;
    if (!bulletTexture.loadFromFile("assets/Extra/Background/PNG/Guns/bullet.png"))
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


    // Sounds ───────────────────────────────────────────────────────────────

    sf::SoundBuffer akShotBuffer;
    if (!akShotBuffer.loadFromFile("assets/Extra/Background/sounds/gunshot.mp3"))
    {
        std::cout << "Failed to load ak47.wav\n";
        return 1;
    }

    sf::Sound akShot(akShotBuffer);
    akShot.setVolume(15.f);

    sf::SoundBuffer reloadBuffer;
    if (!reloadBuffer.loadFromFile("assets/Extra/Background/sounds/reload.mp3"))
    {
        std::cout << "Failed to load ak47.wav\n";
        return 1;
    }

    sf::Sound reloadSound(reloadBuffer);
    reloadSound.setVolume(40.f);

    // ── Sprites ───────────────────────────────────────────────────────────────

    sf::Sprite grassSprite(grassTexture);

    sf::Sprite playerSprite(walkTextures[0]);
    {
        auto sz = walkTextures[0].getSize();
        playerSprite.setOrigin({sz.x / 2.f, sz.y / 2.f});
        playerSprite.setScale({0.1f, 0.1f});
    }

    const float weaponScale = 0.05f;
    sf::Sprite weapSprite(weaponTextures[0]);
    {
        auto sz = weaponTextures[0].getSize();
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
    const float enemyAttackDamage      = 5.f;   // hp lost per hit
    const float enemyAttackInterval   = 0.3f;  // seconds between hits
    const float enemyContactRadius    = 60.f;  // world-px melee range
    const float enemySeparationRadius = 50.f;  // enemies can't get closer than this

    // Reusable health-bar shapes
    sf::RectangleShape hpBarBg, hpBarFill;
    hpBarBg  .setFillColor(sf::Color(120, 0, 0));
    hpBarFill.setFillColor(sf::Color(50, 220, 50));

    // ── Game state ────────────────────────────────────────────────────────────
    enum class GameState { Menu, Playing };
    GameState gameState = GameState::Menu;

    // ── Weapon shop & economy ─────────────────────────────────────────────────
    int  equippedWeapon                       = 0;
    std::array<bool, NUM_WEAPONS> weaponOwned = { true, false, false };
    int  coins     = 0;
    int  highScore = 0;   // best kill count across sessions
    int  killCount = 0;   // kills this run

    // ── Menu panel (slides from the left edge) ────────────────────────────────
    const float PANEL_W   = 260.f;
    bool  panelOpen   = false;
    float panelOffset = -PANEL_W;   // animated x position

    // ── Fire cooldown ─────────────────────────────────────────────────────────
    float fireCooldown = 0.f;

    // ── AK-47 animation & ammo ────────────────────────────────────────────────
    enum class AK47State { Idle, Shooting, Reloading };
    AK47State   ak47State        = AK47State::Idle;
    int         ak47Frame        = 0;
    float       ak47Timer        = 0.f;
    const float AK47_SHOOT_FT   = WEAPONS[1].fireDelay / 12.f;   // seconds per shoot frame  (12 × 0.05 = 0.6 s)
    const float AK47_RELOAD_FT  = 0.05f;   // seconds per reload frame (28 × 0.05 = 1.4 s)
    int         ak47Ammo         = 30;
    const int   AK47_MAX_AMMO    = 30;
    const float ak47Scale        = 0.90f;  // display scale for AK-47 sprite

    // ── Persistent save / load ────────────────────────────────────────────────
    auto saveData = [&]()
    {
        std::ofstream f("save.dat");
        if (!f) return;
        f << coins << "\n" << highScore << "\n" << equippedWeapon << "\n";
        for (int i = 0; i < NUM_WEAPONS; ++i) f << (weaponOwned[i] ? 1 : 0) << " ";
        f << "\n";
    };

    auto loadData = [&]()
    {
        std::ifstream f("save.dat");
        if (!f) return;
        f >> coins >> highScore >> equippedWeapon;
        equippedWeapon = std::clamp(equippedWeapon, 0, NUM_WEAPONS - 1);
        for (int i = 0; i < NUM_WEAPONS; ++i)
        {
            int v = 0;
            if (f >> v) weaponOwned[i] = (v != 0);
        }
        if (!weaponOwned[equippedWeapon]) equippedWeapon = 0;
    };

    loadData();

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // ── Events ────────────────────────────────────────────────────────────

        // Helper: fully reset the in-game state (used by Play and Return-to-Menu)
        auto resetGame = [&]()
        {
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
            spawnTimer   = 0.f;
            showMuzzle   = false;
            animFrame    = 0;
            animTimer    = 0.f;
            fireCooldown = 0.f;
            // Reset AK-47 state
            ak47State = AK47State::Idle;
            ak47Frame = 0;
            ak47Timer = 0.f;
            ak47Ammo  = AK47_MAX_AMMO;
            // Arm weapon sprite for currently equipped weapon
            weapSprite.setTexture(weaponTextures[equippedWeapon]);
            auto sz = weaponTextures[equippedWeapon].getSize();
            weapSprite.setOrigin({sz.x * 0.4f, sz.y * 0.6f});
        };

        bool shouldShoot = false;
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                saveData();
                window.close();
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                {
                    float mx = static_cast<float>(mb->position.x);
                    float my = static_cast<float>(mb->position.y);

                    if (gameState == GameState::Menu)
                    {
                        // ── Panel tab toggle ──────────────────────────────────
                        float tabX = panelOffset + PANEL_W;
                        float tabY = WINDOW_HEIGHT / 2.f - 40.f;
                        if (mx >= tabX && mx <= tabX + 28.f &&
                            my >= tabY && my <= tabY + 80.f)
                        {
                            panelOpen = !panelOpen;
                        }

                        // ── Buy / Equip buttons (inside open panel) ───────────
                        if (mx >= panelOffset && mx <= panelOffset + PANEL_W)
                        {
                            for (int i = 0; i < NUM_WEAPONS; ++i)
                            {
                                float btnY = 100.f + i * 110.f + 60.f;
                                if (my >= btnY && my <= btnY + 30.f)
                                {
                                    if (weaponOwned[i])
                                    {
                                        equippedWeapon = i;
                                    }
                                    else if (coins >= WEAPONS[i].cost)
                                    {
                                        coins -= WEAPONS[i].cost;
                                        weaponOwned[i]  = true;
                                        equippedWeapon  = i;
                                        saveData();
                                    }
                                }
                            }
                        }

                        // ── Play button ───────────────────────────────────────
                        float pbx = WINDOW_WIDTH  / 2.f;
                        float pby = WINDOW_HEIGHT / 2.f + 60.f;
                        if (mx >= pbx - 100.f && mx <= pbx + 100.f &&
                            my >= pby -  30.f  && my <= pby +  30.f)
                        {
                            gameState = GameState::Playing;
                            killCount = 0;
                            resetGame();
                        }
                    }
                    else // Playing
                    {
                        if (playerDead && deathAnimDone)
                        {
                            // ── Return to Menu button ─────────────────────────
                            float bx = WINDOW_WIDTH  / 2.f;
                            float by = WINDOW_HEIGHT / 2.f + 30.f;
                            if (mx >= bx - 110.f && mx <= bx + 110.f &&
                                my >= by -  28.f  && my <= by +  28.f)
                            {
                                saveData();
                                gameState = GameState::Menu;
                                resetGame();
                            }
                        }
                        else if (!playerDead)
                        {
                            shouldShoot = true;
                        }
                    }
                }
            }

            // ── R key: manual AK-47 reload ────────────────────────────────────
            if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                if (kp->code == sf::Keyboard::Key::R &&
                    gameState == GameState::Playing &&
                    equippedWeapon == 1 &&
                    ak47State == AK47State::Idle &&
                    ak47Ammo < AK47_MAX_AMMO &&
                    !playerDead)
                {
                    ak47State = AK47State::Reloading;
                    ak47Frame = 0;
                    ak47Timer = 0.f;
                    reloadSound.play();
                }
            }
        }

        // ── Menu state ────────────────────────────────────────────────────────
        if (gameState == GameState::Menu)
        {
            // Animate panel slide
            float panelTarget = panelOpen ? 0.f : -PANEL_W;
            panelOffset += (panelTarget - panelOffset) * std::min(1.f, 15.f * dt);
            if (std::abs(panelOffset - panelTarget) < 0.5f) panelOffset = panelTarget;

            window.setMouseCursorVisible(true);
            window.clear(sf::Color(15, 18, 25));

            // Tiled grass backdrop
            sf::View menuView(sf::FloatRect({0.f, 0.f},
                {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)}));
            window.setView(menuView);
            for (int gy = 0; gy < static_cast<int>(WINDOW_HEIGHT); gy += 64)
                for (int gx = 0; gx < static_cast<int>(WINDOW_WIDTH); gx += 64)
                {
                    grassSprite.setPosition({static_cast<float>(gx), static_cast<float>(gy)});
                    window.draw(grassSprite);
                }

            // Dark overlay
            sf::RectangleShape mOverlay(
                {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            mOverlay.setFillColor(sf::Color(0, 0, 0, 155));
            window.draw(mOverlay);

            // ── Weapon panel ──────────────────────────────────────────────────
            sf::RectangleShape panel({PANEL_W, static_cast<float>(WINDOW_HEIGHT)});
            panel.setPosition({panelOffset, 0.f});
            panel.setFillColor(sf::Color(18, 20, 45, 235));
            window.draw(panel);

            // Panel tab (always visible at right edge of panel)
            float tabX = panelOffset + PANEL_W;
            float tabY = WINDOW_HEIGHT / 2.f - 40.f;
            sf::RectangleShape tab({28.f, 80.f});
            tab.setPosition({tabX, tabY});
            tab.setFillColor(sf::Color(55, 55, 190));
            window.draw(tab);

            if (fontLoaded)
            {
                // Panel header
                sf::Text panelTitle(font, "WEAPONS", 17);
                panelTitle.setFillColor(sf::Color(200, 200, 255));
                panelTitle.setPosition({panelOffset + 10.f, 14.f});
                window.draw(panelTitle);

                // Weapon list
                for (int i = 0; i < NUM_WEAPONS; ++i)
                {
                    float iy   = 100.f + i * 110.f;
                    bool  own  = weaponOwned[i];
                    bool  equp = (equippedWeapon == i);

                    sf::Text wName(font, WEAPONS[i].name, 17);
                    wName.setFillColor(equp ? sf::Color::Yellow : sf::Color::White);
                    wName.setPosition({panelOffset + 10.f, iy});
                    window.draw(wName);

                    // Stats line
                    std::string stats =
                        "DMG " + std::to_string((int)WEAPONS[i].damage) +
                        "  RoF " + std::to_string((int)(1.f / WEAPONS[i].fireDelay)) + "/s";
                    sf::Text wStats(font, stats, 12);
                    wStats.setFillColor(sf::Color(160, 160, 160));
                    wStats.setPosition({panelOffset + 10.f, iy + 24.f});
                    window.draw(wStats);

                    // Buy / Equip button
                    sf::RectangleShape btn({220.f, 28.f});
                    btn.setPosition({panelOffset + 10.f, iy + 60.f});
                    btn.setFillColor(equp  ? sf::Color( 30, 120,  30) :
                                     own   ? sf::Color( 30,  80, 160) :
                                             sf::Color(100,  60,  20));
                    window.draw(btn);

                    std::string lbl = equp ? "EQUIPPED" :
                                      own  ? "EQUIP" :
                                             "BUY  " + std::to_string(WEAPONS[i].cost) + " coins";
                    sf::Text btnTxt(font, lbl, 13);
                    btnTxt.setFillColor(sf::Color::White);
                    btnTxt.setPosition({panelOffset + 16.f, iy + 65.f});
                    window.draw(btnTxt);
                }

                // Tab arrow
                sf::Text arrow(font, panelOpen ? "<" : ">", 18);
                arrow.setFillColor(sf::Color::White);
                auto ab = arrow.getLocalBounds();
                arrow.setOrigin({ab.position.x + ab.size.x / 2.f,
                                 ab.position.y + ab.size.y / 2.f});
                arrow.setPosition({tabX + 14.f, WINDOW_HEIGHT / 2.f});
                window.draw(arrow);

                // ── Centre content ─────────────────────────────────────────
                auto centreText = [&](sf::Text& t, float cy)
                {
                    auto b = t.getLocalBounds();
                    t.setOrigin({b.position.x + b.size.x / 2.f,
                                 b.position.y + b.size.y / 2.f});
                    t.setPosition({WINDOW_WIDTH / 2.f, cy});
                };

                sf::Text title(font, "ZOMBIE INVASION", 58);
                title.setFillColor(sf::Color(220, 45, 45));
                centreText(title, WINDOW_HEIGHT / 2.f - 80.f);
                window.draw(title);

                sf::Text sub(font, "Survive the horde!", 22);
                sub.setFillColor(sf::Color(180, 180, 180));
                centreText(sub, WINDOW_HEIGHT / 2.f - 22.f);
                window.draw(sub);

                sf::Text hs(font, "Best: " + std::to_string(highScore) + " kills", 20);
                hs.setFillColor(sf::Color(220, 220, 70));
                centreText(hs, WINDOW_HEIGHT / 2.f + 18.f);
                window.draw(hs);

                // Play button
                sf::RectangleShape playBtn({200.f, 60.f});
                playBtn.setOrigin({100.f, 30.f});
                playBtn.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f + 60.f});
                playBtn.setFillColor(sf::Color(35, 150, 35));
                window.draw(playBtn);

                sf::Text playTxt(font, "PLAY", 30);
                playTxt.setFillColor(sf::Color::White);
                centreText(playTxt, WINDOW_HEIGHT / 2.f + 60.f);
                window.draw(playTxt);

                // Coin counter (top right)
                sf::Text coinTxt(font, "Coins: " + std::to_string(coins), 22);
                coinTxt.setFillColor(sf::Color(255, 215, 0));
                auto cb = coinTxt.getLocalBounds();
                coinTxt.setOrigin({cb.position.x + cb.size.x, 0.f});
                coinTxt.setPosition({WINDOW_WIDTH - 16.f, 12.f});
                window.draw(coinTxt);
            }

            window.display();
            continue;   // skip all game-logic sections below
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

        // ── AK-47 animation update ────────────────────────────────────────────
        if (equippedWeapon == 1)
        {
            if (ak47State == AK47State::Shooting)
            {
                ak47Timer += dt;
                while (ak47Timer >= AK47_SHOOT_FT)
                {
                    ak47Timer -= AK47_SHOOT_FT;
                    if (++ak47Frame >= 12) { ak47Frame = 0; ak47State = AK47State::Idle; }
                }
            }
            else if (ak47State == AK47State::Reloading)
            {
                ak47Timer += dt;
                while (ak47Timer >= AK47_RELOAD_FT)
                {
                    ak47Timer -= AK47_RELOAD_FT;
                    if (++ak47Frame >= 28)
                    {
                        ak47Frame = 0;
                        ak47State = AK47State::Idle;
                        ak47Ammo  = AK47_MAX_AMMO;
                    }
                }
            }
            // Auto-reload when magazine empties
            if (ak47Ammo <= 0 && ak47State == AK47State::Idle) { 
                ak47State = AK47State::Reloading;
                ak47Frame = 0;
                ak47Timer = 0.f;
                reloadSound.play();
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
        bool aimingLeft = (aimDir.x < 0.f);
        float scaleX = (aimingLeft) ? -0.1f : 0.1f;
        playerSprite.setScale({scaleX, 0.1f});
        

        float offsetX = aimingLeft ? 20.f : -20.f;

        // ── Weapon positioning & rotation ─────────────────────────────────────
        // Gun sits below the player centre, so recompute aim from weapPos so
        // the rotation angle matches where bullets actually go.

        sf::Vector2f weapPos = playerSprite.getPosition() + sf::Vector2f(offsetX, 75.f);

        sf::Vector2f weapAimDir = mouseWorld - weapPos;
        float weapAimLen = std::sqrt(weapAimDir.x * weapAimDir.x + weapAimDir.y * weapAimDir.y);
        if (weapAimLen > 0.f) weapAimDir /= weapAimLen;
        float weapAimAngle = std::atan2(weapAimDir.y, weapAimDir.x) * RAD2DEG;

        bool aimLeft = (weapAimDir.x < 0.f);

        // Swap in the equipped weapon texture each frame
        // AK-47 uses its animated frames; other weapons use the static texture.
        const sf::Texture* activeWeapTex   = nullptr;
        float               activeWeapScale = weaponScale;
        {
            if (equippedWeapon == 1) // AK-47
            {
                if      (ak47State == AK47State::Shooting)  activeWeapTex = &ak47ShootTex[ak47Frame];
                else if (ak47State == AK47State::Reloading) activeWeapTex = &ak47ReloadTex[ak47Frame];
                else                                         activeWeapTex = &ak47ShootTex[0];
                activeWeapScale = ak47Scale;
            }
            else
            {
                activeWeapTex   = &weaponTextures[equippedWeapon];
                activeWeapScale = weaponScale;
            }
            weapSprite.setTexture(*activeWeapTex);
            auto sz = activeWeapTex->getSize();
            weapSprite.setOrigin(equippedWeapon == 1
                // ? sf::Vector2f{sz.x * 0.6f, sz.y * 0.5f}   // AK-47: pivot near grip
                ? sf::Vector2f{sz.x * 0.15f, sz.y * 0.5f}   // AK-47: pivot near grip
                : sf::Vector2f{sz.x * 0.4f, sz.y * 0.6f}); // other weapons
        }

        weapSprite.setPosition(weapPos);
        // AK-47 sprite faces LEFT by default, so flip X to make it face right
        // like the other weapons, then apply the normal Y-flip for left aiming.
        float xFlip = (equippedWeapon == 1) ? activeWeapScale : activeWeapScale;
        weapSprite.setScale({xFlip, aimLeft ? -activeWeapScale : activeWeapScale});
        weapSprite.setRotation(sf::degrees(weapAimAngle));

        // AK-47: after X-flip, barrel (originally at left edge) is now at the right.
        // Origin is 60 % from left, so barrel is 60 % * width past the origin.
        float barrelLength = (equippedWeapon == 1)
            ? activeWeapTex->getSize().x * 0.35f * activeWeapScale
            : activeWeapTex->getSize().x * 0.45f * activeWeapScale;

        // Perpendicular offset keeps the muzzle aligned with the gun barrel.
        // Flip sign when aiming left because the gun sprite is Y-flipped.
        sf::Vector2f perp = {-weapAimDir.y, weapAimDir.x};
        float sideOffset = aimLeft ? 14.f : -14.f;

        sf::Vector2f barrelTip =
            weapPos
            + weapAimDir * barrelLength
            + perp * sideOffset;

        // ── Shooting ──────────────────────────────────────────────────────────

        fireCooldown = std::max(0.f, fireCooldown - dt);

        // AK-47 can't fire while reloading or with an empty mag
        bool ak47CanFire = (equippedWeapon != 1) ||
                           (ak47State != AK47State::Reloading && ak47Ammo > 0);

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !playerDead &&
            fireCooldown <= 0.f && ak47CanFire)
        {
            sf::Vector2f bulletDir = mouseWorld - barrelTip;
            float bdLen = std::sqrt(bulletDir.x * bulletDir.x + bulletDir.y * bulletDir.y);
            if (bdLen > 0.f) bulletDir /= bdLen;

            if (equippedWeapon == 2) // Shotgun: 7 pellets in a spread cone
            {
                float baseAngle = std::atan2(bulletDir.y, bulletDir.x);
                const int   pellets   = 7;
                const float spreadRad = 25.f * PI / 180.f;
                for (int i = 0; i < pellets; ++i)
                {
                    float offset = -spreadRad + (2.f * spreadRad / (pellets - 1)) * i;
                    float a = baseAngle + offset;
                    sf::Vector2f dir = {std::cos(a), std::sin(a)};
                    bullets.emplace_back(bulletTexture, barrelTip, dir * bulletSpeed);
                }
            }
            else
            {
                bullets.emplace_back(bulletTexture, barrelTip, bulletDir * bulletSpeed);
                if(equippedWeapon == 1){
                    akShot.play();
                }
            }

            // AK-47: consume ammo and start shoot animation
            if (equippedWeapon == 1)
            {
                --ak47Ammo;
                if (ak47State == AK47State::Idle)
                {
                    ak47State = AK47State::Shooting;
                    ak47Frame = 0;
                    ak47Timer = 0.f;
                }
            }
            if(equippedWeapon != 1){
                showMuzzle   = true;
                muzzleTimer  = muzzleDuration;
            }
            
            fireCooldown = WEAPONS[equippedWeapon].fireDelay;
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
                    e.hp -= WEAPONS[equippedWeapon].damage;
                    b.lifetime = 0.f;
                    if (e.hp <= 0.f)
                    {
                        e.isDying    = true;
                        e.deathFrame = 0;
                        e.deathTimer = 0.f;
                        e.isHit      = false;
                        coins += 5;
                        ++killCount;
                        if (killCount > highScore) highScore = killCount;
                    }
                    else
                    {
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

            // Push enemy out if it crossed inside the minimum separation radius
            {
                sf::Vector2f toEnemy = e.sprite.getPosition() - playerSprite.getPosition();
                float d = std::sqrt(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y);
                if (d > 0.f && d < enemySeparationRadius)
                    e.sprite.setPosition(
                        playerSprite.getPosition() + (toEnemy / d) * enemySeparationRadius);
            }

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
                sf::Vector2f playerBody = playerSprite.getPosition() + sf::Vector2f(0.f, 30.f);
                sf::Vector2f d = eBody - playerBody;
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

        // ── In-game HUD (top corners, screen space) ───────────────────────────
        if (!playerDead && fontLoaded)
        {
            sf::Text killHud(font, "Kills: " + std::to_string(killCount), 20);
            killHud.setFillColor(sf::Color::White);
            killHud.setPosition({12.f, 10.f});
            window.draw(killHud);

            sf::Text coinHud(font, "Coins: " + std::to_string(coins), 20);
            coinHud.setFillColor(sf::Color(255, 215, 0));
            auto cb = coinHud.getLocalBounds();
            coinHud.setOrigin({cb.position.x + cb.size.x, 0.f});
            coinHud.setPosition({WINDOW_WIDTH - 12.f, 10.f});
            window.draw(coinHud);

            // AK-47 ammo counter (bottom-centre)
            if (equippedWeapon == 1)
            {
                bool  reloading = (ak47State == AK47State::Reloading);
                std::string ammoStr = reloading
                    ? "RELOADING..."
                    : std::to_string(ak47Ammo) + " / " + std::to_string(AK47_MAX_AMMO);
                sf::Text ammoHud(font, ammoStr, 22);
                ammoHud.setFillColor(reloading ? sf::Color::Yellow : sf::Color::White);
                auto ab = ammoHud.getLocalBounds();
                ammoHud.setOrigin({ab.position.x + ab.size.x / 2.f,
                                   ab.position.y + ab.size.y / 2.f});
                ammoHud.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 36.f});
                window.draw(ammoHud);
            }
        }

        // ── Game-over overlay ─────────────────────────────────────────────────
        if (playerDead && deathAnimDone)
        {
            sf::RectangleShape overlay(
                {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
            overlay.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(overlay);

            // Return-to-menu button
            sf::RectangleShape btn({220.f, 56.f});
            btn.setOrigin({110.f, 28.f});
            btn.setPosition({WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f + 30.f});
            btn.setFillColor(sf::Color(40, 40, 200));
            window.draw(btn);

            if (fontLoaded)
            {
                auto centre = [&](sf::Text& t, float cy)
                {
                    auto b = t.getLocalBounds();
                    t.setOrigin({b.position.x + b.size.x / 2.f,
                                 b.position.y + b.size.y / 2.f});
                    t.setPosition({WINDOW_WIDTH / 2.f, cy});
                };

                sf::Text goText(font, "GAME OVER", 52);
                goText.setFillColor(sf::Color::Red);
                centre(goText, WINDOW_HEIGHT / 2.f - 65.f);
                window.draw(goText);

                sf::Text killsText(font,
                    "Kills: " + std::to_string(killCount) +
                    "   Best: " + std::to_string(highScore), 24);
                killsText.setFillColor(sf::Color(220, 220, 70));
                centre(killsText, WINDOW_HEIGHT / 2.f - 12.f);
                window.draw(killsText);

                sf::Text btnText(font, "RETURN TO MENU", 22);
                btnText.setFillColor(sf::Color::White);
                centre(btnText, WINDOW_HEIGHT / 2.f + 30.f);
                window.draw(btnText);
            }
        }

        window.display();
    }

    return 0;
}
