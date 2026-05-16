#include "server/sim/core/GameLoop.hpp"
#include <shared/net/Message.hpp>
#include <shared/sim/Constants.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>
#include <algorithm>

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
        // Wrzucamy całą logikę startową na strand, by uniknąć wyścigów przy restarcie
        boost::asio::post(strand_, [this]() {
            if (running_.exchange(true)) return;
            debugPaused_ = false;
            std::cout << "[GameLoop] Starting at " << tickrateHz_.load() << " Hz\n";

            // Wyślij pełny snapshot
            auto snapshot = buildFullSnapshot();
            broadcaster_.broadcast(snapshot);

            // Oznacz początkowy stan jako dirty
            for (auto& tile : state_.tiles)
                state_.markTileDirty(tile.id);
            for (auto& prov : state_.landProvinces)
                for (auto& [eid, share] : prov.playerShares)
                    state_.markShareDirty(prov.id, eid);
            for (auto& e : state_.entities)
                state_.markEntityDirty(e.id);

            // Ustawiamy pierwszy deadline i odpalamy korutynę
            auto intervalMs = std::chrono::milliseconds(1000 / tickrateHz_.load());
            timer_.expires_after(intervalMs);

            boost::asio::co_spawn(strand_, runLoopTask(), boost::asio::detached);
            });
    }

    void GameLoop::stop() {
        // Zabezpieczenie asynchroniczne wyłączenia
        boost::asio::post(strand_, [this]() {
            if (!running_.exchange(false)) return;

            timer_.cancel(); // Przerwie asynchroniczne oczekiwanie w korutynie
            std::cout << "[GameLoop] Stopped at tick " << state_.currentTick << "\n";
            });
    }

    // ── Główna Pętla jako Korutyna (Likwidacja Tick Driftu) ───────────────────────

    boost::asio::awaitable<void> GameLoop::runLoopTask() {
        try {
            while (running_.load()) {
                // Oddajemy kontrolę do momentu wygaśnięcia timera
                co_await timer_.async_wait(boost::asio::use_awaitable);

                if (!running_.load()) break;

                // Logika pauzy w trybie debug
                if (!debugPaused_ || debugStepsRemaining_ > 0) {
                    tick();

                    if (debugPaused_ && debugStepsRemaining_ > 0) {
                        --debugStepsRemaining_;
                    }
                }

                // KLUCZOWE DLA RTS: Unikamy Tick-Driftu!
                // Następny tick dodajemy do OBECNEGO czasu wygaśnięcia, a nie od "teraz".
                auto intervalMs = std::chrono::milliseconds(1000 / tickrateHz_.load());
                timer_.expires_at(timer_.expiry() + intervalMs);
            }
        }
        catch (const boost::system::system_error& e) {
            if (e.code() != boost::asio::error::operation_aborted) {
                std::cerr << "[GameLoop] Timer error: " << e.what() << "\n";
            }
        }
    }

    // ── Główny tick ───────────────────────────────────────────────────────────────

    void GameLoop::tick() {
        state_.currentTick++;

        // 1. Wstępny preprocessing inputów
        processInputs();

        // 2. Systemy symulacji (Kolejność gwarantuje determinizm)
        // Każdy z systemów ma teraz pełny dostęp do state_.pendingInputs!
        combatSys_.tick(state_);
        populationSys_.tick(state_);
        supplySys_.tick(state_);
        electricSys_.tick(state_);
        tradeSys_.tick(state_);
        navalSys_.tick(state_);
        airSys_.tick(state_);
        nukeSys_.tick(state_);
        diplomacySys_.tick(state_);

        // 3. Warunki zwycięstwa
        checkVictory();

        // 4. Budowanie delty i rozsyłanie do klientów
        if (!state_.dirtyTiles.empty()
            || !state_.dirtyBuildings.empty()
            || !state_.dirtyEntities.empty()
            || !state_.dirtyShares.empty()
            || !state_.pendingEvents.empty()) {

            auto delta = buildDelta();
            broadcaster_.broadcast(delta);
        }

        // 5. CZYSZCZENIE STANU NA KONIEC TICKA
        // (W pliku GameState.hpp dodaliśmy do tej metody pendingInputs.clear())
        state_.clearDirtyFlags();

        // 6. Logowanie co 10 sekund
        if (state_.currentTick % (tickrateHz_.load() * 10) == 0) {
            std::cout << "[GameLoop] Tick " << state_.currentTick << "\n";
        }
    }

    // ── Przetwarzanie inputów ─────────────────────────────────────────────────────

    void GameLoop::processInputs() {
        // Ta funkcja służy teraz WYŁĄCZNIE do ogólnego preprocessingu 
        // lub obsługi inputów niezwiązanych z konkretnymi systemami.
        for (auto& input : state_.pendingInputs) {
            // TODO: ew. routing ogólny...
            (void)input;
        }
    }

    // ── Warunki zwycięstwa ────────────────────────────────────────────────────────

    void GameLoop::checkVictory() {
        if (state_.phase != GamePhase::Playing) return;

        uint32_t totalOwned = 0;
        for (auto& tile : state_.tiles) {
            if (tile.landProvinceId != 0 && tile.ownerId != 0)
                ++totalOwned;
        }
        if (totalOwned == 0) return;

        for (auto& entity : state_.entities) {
            if (entity.isEliminated) continue;

            uint32_t myTiles = 0;
            for (auto& tile : state_.tiles) {
                if (tile.ownerId == entity.id && tile.landProvinceId != 0)
                    ++myTiles;
            }

            if (static_cast<float>(myTiles) / static_cast<float>(totalOwned) >= constants::VICTORY_THRESHOLD) {
                std::cout << "[GameLoop] Entity " << entity.id << " (" << entity.name << ") wins!\n";

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
    }

    // ── Budowanie delty ───────────────────────────────────────────────────────────

    // UWAGA DO AUTORA: Jeżeli iterujesz tu po `state_.dirtyTiles`, które są std::unordered_set, 
    // kolejność pakietów w sieci będzie inna przy każdym uruchomieniu! Zmień w GameState.hpp
    // wszystkie flagi dirty (std::unordered_set) na std::set<uint32_t> !

    Message GameLoop::buildDelta() const {
        Serializer s;
        s.writeU32(state_.currentTick);

        s.writeU16(static_cast<uint16_t>(std::min(state_.dirtyTiles.size(), (size_t)0xFFFF)));
        for (uint32_t tid : state_.dirtyTiles) {
            const auto* tile = const_cast<GameState&>(state_).getTile(tid);
            if (!tile) continue;
            s.writeU32(tile->id);
            s.writeU32(tile->ownerId);
            s.writeU8(static_cast<uint8_t>(tile->terrain));
        }

        s.writeU16(static_cast<uint16_t>(std::min(state_.dirtyShares.size(), (size_t)0xFFFF)));
        for (uint64_t key : state_.dirtyShares) {
            uint32_t provId = static_cast<uint32_t>(key >> 32);
            uint32_t eid = static_cast<uint32_t>(key & 0xFFFFFFFF);
            const auto* share = const_cast<GameState&>(state_).getShare(provId, eid);
            if (!share) continue;
            s.writeU32(provId);
            s.writeU32(eid);
            s.writeF32(share->population.workers);
            s.writeF32(share->population.military);
            s.writeF32(share->population.maxCap);
            s.writeF32(share->supplyStored);
            s.writeF32(share->workerSplitRatio);
        }

        s.writeU16(0); // TODO: dirty units

        s.writeU16(static_cast<uint16_t>(state_.pendingEvents.size()));
        for (const auto& evt : state_.pendingEvents) {
            s.writeU8(static_cast<uint8_t>(evt.type));
            s.writeU32(evt.entityId);
            s.writeU32(evt.targetId);
            s.writeF32(evt.x);
            s.writeF32(evt.y);
            s.writeF32(evt.radius);
        }

        return Message::make(MessageType::GameStateDelta, s);
    }

    Message GameLoop::buildFullSnapshot() const {
        Serializer s;
        s.writeU32(state_.currentTick);
        s.writeU16(state_.mapWidth);
        s.writeU16(state_.mapHeight);

        s.writeU32(static_cast<uint32_t>(state_.tiles.size()));
        for (const auto& tile : state_.tiles) {
            s.writeU32(tile.id);
            s.writeU16(tile.x);
            s.writeU16(tile.y);
            s.writeU32(tile.ownerId);
            s.writeU8(static_cast<uint8_t>(tile.terrain));
            s.writeU32(tile.landProvinceId);
            s.writeU32(tile.seaProvinceId);
        }

        s.writeU32(static_cast<uint32_t>(state_.landProvinces.size()));
        for (const auto& prov : state_.landProvinces) {
            s.writeU32(prov.id);
            s.writeStr(prov.name);
            s.writeU16(static_cast<uint16_t>(prov.playerShares.size()));
            for (const auto& [eid, share] : prov.playerShares) {
                s.writeU32(prov.id);
                s.writeU32(eid);
                s.writeF32(share.population.workers);
                s.writeF32(share.population.military);
                s.writeF32(share.population.maxCap);
                s.writeF32(share.supplyStored);
                s.writeF32(share.workerSplitRatio);
            }
        }

        s.writeU32(static_cast<uint32_t>(state_.entities.size()));
        for (const auto& e : state_.entities) {
            s.writeU32(e.id);
            s.writeStr(e.name);
            s.writeU8(static_cast<uint8_t>(e.type));
            s.writeI64(e.gold);
            s.writeF32(e.uranium);
            s.writeBool(e.isEliminated);
            s.writeU32(e.militaryPactId);
            s.writeU32(e.economicPactId);
        }

        s.writeU32(static_cast<uint32_t>(state_.buildings.size()));
        for (const auto& b : state_.buildings) {
            s.writeU32(b.id);
            s.writeU8(static_cast<uint8_t>(b.type));
            s.writeU8(b.level);
            s.writeU32(b.ownerId);
            s.writeU32(b.landProvinceId);
            s.writeU16(b.topLeftX);
            s.writeU16(b.topLeftY);
            s.writeBool(b.isActive);
            s.writeBool(b.isCapital);
        }

        return Message::make(MessageType::GameStateFull, s);
    }

    // ── Tryb debug & Kolejka (Thread-safe przez Strand) ──────────────────────────

    void GameLoop::stepTicks(uint32_t n) {
        boost::asio::post(strand_, [this, n]() {
            if (n == 0) return;
            debugPaused_ = true;
            debugStepsRemaining_ = n;
            });
    }

    void GameLoop::setTickrate(uint32_t hz) {
        boost::asio::post(strand_, [this, hz]() {
            uint32_t clamped = std::clamp(hz, 1u, 60u);
            tickrateHz_.store(clamped);
            std::cout << "[GameLoop] Tickrate set to " << clamped << " Hz\n";
            });
    }

    void GameLoop::enqueueInput(PlayerInput input) {
        boost::asio::post(strand_, [this, inp = std::move(input)]() mutable {
            inp.tick = state_.currentTick;
            state_.pendingInputs.push_back(std::move(inp));
            });
    }

    void GameLoop::enqueueDisconnect(uint32_t entityId) {
        boost::asio::post(strand_, [this, eid = entityId]() {
            if (auto* entity = state_.getEntity(eid)) {
                entity->disconnectedSinceTick = state_.currentTick;
                entity->type = EntityType::Disconnected;
                state_.markEntityDirty(eid);
            }
            });
    }

} // namespace gs::server