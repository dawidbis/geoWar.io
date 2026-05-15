#include "server/sim/GameLoop.hpp"
#include <shared/net/Message.hpp>
#include <shared/sim/Constants.hpp>

#include <iostream>

namespace gs::server {

    GameLoop::GameLoop(boost::asio::io_context& ioc,
        GameState& state,
        SessionBroadcaster& broadcaster)
        : ioc_(ioc)
        , timer_(ioc)
        , state_(state)
        , broadcaster_(broadcaster)
        , strand_(boost::asio::make_strand(ioc))
    {
    }

    GameLoop::~GameLoop() {
        stop();
    }

    // ── Start / Stop ──────────────────────────────────────────────────────────────

    void GameLoop::start() {
        if (running_.exchange(true)) return; // już działa
        debugPaused_ = false;
        std::cout << "[GameLoop] Starting at " << tickrateHz_.load() << " Hz\n";
        scheduleTick();
    }

    void GameLoop::stop() {
        if (!running_.exchange(false)) return;

        std::cout << "[GameLoop] Stopped at tick " << state_.currentTick << "\n";
    }

    // ── Harmonogram ticków ────────────────────────────────────────────────────────

    void GameLoop::scheduleTick() {
        if (!running_.load()) return;
        if (debugPaused_ && debugStepsRemaining_ == 0) return;

        auto intervalMs = std::chrono::milliseconds(1000 / tickrateHz_.load());
        timer_.expires_after(intervalMs);

        timer_.async_wait(
            boost::asio::bind_executor(strand_,
                [this](boost::system::error_code ec) {
                    if (ec || !running_.load()) return;

                    tick();

                    if (debugPaused_) {
                        if (debugStepsRemaining_ > 0) {
                            --debugStepsRemaining_;
                            scheduleTick();
                        }
                        // else: czekaj na kolejne stepTicks()
                    }
                    else {
                        scheduleTick();
                    }
                }));
    }

    // ── Główny tick ───────────────────────────────────────────────────────────────

    void GameLoop::tick() {
        state_.currentTick++;

        // 1. Przetwórz inputy graczy
        processInputs();

        // 2. Systemy symulacji — kolejność jest ważna (patrz dokumentacja)
        // TODO: Każdy system będzie tu wywołany gdy zostanie zaimplementowany
        // combatSys_.tick(state_);
        // populationSys_.tick(state_);
        // supplySys_.tick(state_);
        // electricSys_.tick(state_);
        // tradeSys_.tick(state_);
        // navalSys_.tick(state_);
        // airSys_.tick(state_);
        // nukeSys_.tick(state_);
        // diplomacySys_.tick(state_);

        // 3. Warunki zwycięstwa
        checkVictory();

        // 4. Generuj delta i roześlij
        if (!state_.dirtyTiles.empty()
            || !state_.dirtyBuildings.empty()
            || !state_.dirtyEntities.empty()
            || !state_.dirtyShares.empty()
            || !state_.pendingEvents.empty()) {

            auto delta = buildDelta();
            broadcaster_.broadcast(delta);
        }

        // 5. Wyczyść dirty flags i zdarzenia
        state_.clearDirtyFlags();

        // 6. Log co 100 ticków (co 10 sekund)
        if (state_.currentTick % 100 == 0) {
            std::cout << "[GameLoop] Tick " << state_.currentTick << "\n";
        }
    }

    // ── Przetwarzanie inputów ─────────────────────────────────────────────────────

    void GameLoop::processInputs() {
        // Inputy dodawane przez enqueueInput() — już na stranie więc bezpieczne
        for (auto& input : state_.pendingInputs) {
            // TODO: routing do właściwego systemu zależnie od InputType
            // Np. InputType::Attack → CombatSystem::handleInput(state_, input)
            (void)input; // suppress unused warning
        }
        state_.pendingInputs.clear();
    }

    // ── Warunki zwycięstwa ────────────────────────────────────────────────────────

    void GameLoop::checkVictory() {
        if (state_.phase != GamePhase::Playing) return;

        // Policz kafelki lądowe z właścicielem (fallout = bez właściciela)
        uint32_t totalOwned = 0;
        for (auto& tile : state_.tiles) {
            if (tile.landProvinceId != 0 && tile.ownerId != 0)
                ++totalOwned;
        }
        if (totalOwned == 0) return;

        // Sprawdź per encja
        for (auto& entity : state_.entities) {
            if (entity.isEliminated) continue;

            uint32_t myTiles = 0;
            for (auto& tile : state_.tiles) {
                if (tile.ownerId == entity.id && tile.landProvinceId != 0)
                    ++myTiles;
            }

            if (static_cast<float>(myTiles) / static_cast<float>(totalOwned)
                >= constants::VICTORY_THRESHOLD) {

                std::cout << "[GameLoop] Entity " << entity.id
                    << " (" << entity.name << ") wins!\n";

                state_.phase = GamePhase::Finished;

                GameEvent evt;
                evt.type = GameEventType::VictoryAchieved;
                evt.entityId = entity.id;
                evt.tick = state_.currentTick;
                state_.pendingEvents.push_back(evt);

                stop();
                return;
            }
        }

        // TODO: Sprawdź pakty militarne (zsumowane kafelki)
    }

    // ── Budowanie delty ───────────────────────────────────────────────────────────

    Message GameLoop::buildDelta() const {
        // TODO: Serializuj zmienione pola do GameStateDelta
        // Na razie wysyłamy pusty delta jako placeholder
        // Pełna implementacja po zdefiniowaniu formatu GameStateDelta w Serializer

        Serializer s;
        s.writeU32(state_.currentTick);
        s.writeU16(0); // numTileChanges
        s.writeU16(0); // numShareChanges
        s.writeU16(0); // numUnitChanges
        s.writeU16(static_cast<uint16_t>(state_.pendingEvents.size()));

        for (auto& evt : state_.pendingEvents) {
            s.writeU8(static_cast<uint8_t>(evt.type));
            s.writeU32(evt.entityId);
            s.writeU32(evt.targetId);
            s.writeF32(evt.x);
            s.writeF32(evt.y);
            s.writeF32(evt.radius);
        }

        return Message::make(MessageType::GameStateDelta, s);
    }

    // ── Tryb debug ────────────────────────────────────────────────────────────────

    void GameLoop::stepTicks(uint32_t n) {
        boost::asio::post(strand_, [this, n]() {
            if (n == 0) return;
            debugPaused_ = true;
            debugStepsRemaining_ = n;
            if (!running_.exchange(true)) {
                scheduleTick();
            }
            else {
                scheduleTick(); // wznów
            }
            });
    }

    void GameLoop::setTickrate(uint32_t hz) {
        uint32_t clamped = std::clamp(hz, 1u, 60u);
        tickrateHz_.store(clamped);
        std::cout << "[GameLoop] Tickrate set to " << clamped << " Hz\n";
    }

    // ── Kolejka inputów ───────────────────────────────────────────────────────────

    void GameLoop::enqueueInput(PlayerInput input) {
        boost::asio::post(strand_, [this, inp = std::move(input)]() mutable {
            inp.tick = state_.currentTick;
            state_.pendingInputs.push_back(std::move(inp));
            });
    }

} // namespace gs::server