#include "server/sim/systems/DiplomacySystem.hpp"

#include <algorithm>
#include <iostream>

namespace gs::server {

    static constexpr uint32_t PACT_VOTE_DEADLINE_TICKS = 1500;
    static constexpr uint32_t TRAITOR_DEBUFF_TICKS = 3000;
    static constexpr uint32_t PACT_CREATION_COST = 5000;

    void DiplomacySystem::tick(GameState& state) {
        // Przetwórz inputy dyplomatyczne
        for (auto& input : state.pendingInputs) {
            switch (input.type) {
            case InputType::DeclareWar:
                processWarDeclaration(state, input); break;
            case InputType::ProposePact:
                processPactProposal(state, input);   break;
            case InputType::PactVote:
                processPactVote(state, input);       break;
            case InputType::LeavePact:
                processPactLeave(state, input);      break;
            case InputType::SellProvince:
                processProvinceSale(state, input);   break;
            default: break;
            }
        }

        // Tick wygasających głosowań
        tickPactVotings(state);

        // Tick debuffów zdrajcy
        tickTraitorDebuffs(state);

        // Bazowy przychód złota (ZMIANA: Zoptymalizowane pod kątem sieci!)
        for (auto& entity : state.entities) {
            if (entity.isEliminated) continue;
            entity.gold += 2; // +2/tick bazowo

            // Z premedytacją NIE wywołujemy state.markEntityDirty(entity.id);
            // Pasywny przychód jest w 100% deterministyczny. Klient powinien
            // na podstawie aktualnego ticku gry dodawać to złoto w swoim UI.
            // Oznaczamy entity jako dirty WYŁĄCZNIE, gdy gracz dokona transakcji.
        }
    }

    // ── Deklaracja wojny ──────────────────────────────────────────────────────────

    void DiplomacySystem::processWarDeclaration(GameState& state,
        const PlayerInput& input) {
        uint32_t a = input.entityId;
        uint32_t b = input.targetId;
        if (a == b) return;

        auto& rel = state.getOrCreateRelation(a, b);
        if (rel.hasFlag(RelationFlag::AtWar)) return; // już w wojnie

        // Sprawdź debuff zdrajcy — czy A jest w pakcie militarnym z B
        bool isBetrayal = state.inSameMilitaryPact(a, b);

        rel.setFlag(RelationFlag::AtWar);
        rel.clearFlag(RelationFlag::BilateralAlly);
        rel.clearFlag(RelationFlag::BilateralTrade);
        rel.setFlag(RelationFlag::Embargo);

        // Debuff zdrajcy
        if (isBetrayal) {
            applyTraitorDebuff(state, a, TRAITOR_DEBUFF_TICKS);

            // Wypchnij A z paktu militarnego
            auto* entityA = state.getEntity(a);
            if (entityA && entityA->militaryPactId != 0) {
                leavePact(state, a, entityA->militaryPactId);
            }
        }

        GameEvent evt;
        evt.type = GameEventType::WarDeclared;
        evt.entityId = a;
        evt.targetId = b;
        evt.tick = state.currentTick;
        state.pendingEvents.push_back(evt);

        std::cout << "[Diplomacy] War declared: entity " << a
            << " → entity " << b << "\n";
    }

    // ── Propozycja paktu ──────────────────────────────────────────────────────────

    void DiplomacySystem::processPactProposal(GameState& state,
        const PlayerInput& input) {
        uint32_t founderId = input.entityId;
        PactType pactType = static_cast<PactType>(
            static_cast<uint8_t>(input.floatParam));

        auto* founder = state.getEntity(founderId);
        if (!founder) return;

        // Koszt założenia paktu
        if (founder->gold < PACT_CREATION_COST) {
            std::cout << "[Diplomacy] Not enough gold to create pact\n";
            return;
        }

        // Sprawdź czy nie jest już w pakcie tego typu
        if (pactType == PactType::Military && founder->militaryPactId != 0) return;
        if (pactType == PactType::Economic && founder->economicPactId != 0) return;

        founder->gold -= PACT_CREATION_COST;

        Pact pact;
        pact.id = static_cast<uint32_t>(state.pacts.size()) + 1;
        pact.type = pactType;
        pact.name = (pactType == PactType::Military)
            ? "Military Pact" : "Economic Pact";
        pact.founderId = founderId;
        pact.createdTick = state.currentTick;
        pact.memberIds.push_back(founderId);

        if (pactType == PactType::Military) founder->militaryPactId = pact.id;
        else                                founder->economicPactId = pact.id;

        state.pacts.push_back(std::move(pact));
        state.markEntityDirty(founderId);

        GameEvent evt;
        evt.type = GameEventType::PactFormed;
        evt.entityId = founderId;
        evt.tick = state.currentTick;
        state.pendingEvents.push_back(evt);

        std::cout << "[Diplomacy] Pact created by entity " << founderId << "\n";
    }

    // ── Głosowanie ────────────────────────────────────────────────────────────────

    void DiplomacySystem::processPactVote(GameState& state,
        const PlayerInput& input) {
        uint32_t voterId = input.entityId;
        uint32_t pactId = input.targetId;
        uint32_t candidateId = input.provinceId; // overload pola — kandydat
        bool     voteYes = input.floatParam > 0.5f;

        Pact* pact = nullptr;
        for (auto& p : state.pacts) {
            if (p.id == pactId) { pact = &p; break; }
        }
        if (!pact) return;

        // Sprawdź czy voter jest członkiem
        auto memberIt = std::find(pact->memberIds.begin(),
            pact->memberIds.end(), voterId);
        if (memberIt == pact->memberIds.end()) return;

        // Znajdź aplikację
        for (auto& app : pact->pendingApplications) {
            if (app.candidateEntityId != candidateId) continue;

            app.votes[voterId] = voteYes ? VoteResult::Yes : VoteResult::No;

            // Sprawdź czy wszyscy zagłosowali
            bool allVoted = true;
            bool allYes = true;
            for (uint32_t mid : pact->memberIds) {
                if (mid == candidateId) continue;
                auto it = app.votes.find(mid);
                if (it == app.votes.end()) { allVoted = false; break; }
                if (it->second != VoteResult::Yes) allYes = false;
            }

            if (allVoted) {
                if (allYes) {
                    addEntityToPact(state, candidateId, pactId);
                }
                // Usuń aplikację
                app.deadlineTick = 0; // zostanie usunięta przez tickPactVotings
            }
            break;
        }
    }

    // ── Opuszczenie paktu ─────────────────────────────────────────────────────────

    void DiplomacySystem::processPactLeave(GameState& state,
        const PlayerInput& input) {
        uint32_t entityId = input.entityId;
        uint32_t pactId = input.targetId;

        auto* entity = state.getEntity(entityId);
        if (!entity) return;

        bool isInPact = (entity->militaryPactId == pactId
            || entity->economicPactId == pactId);
        if (!isInPact) return;

        leavePact(state, entityId, pactId);
    }

    void DiplomacySystem::leavePact(GameState& state,
        uint32_t entityId, uint32_t pactId) {
        for (auto& pact : state.pacts) {
            if (pact.id != pactId) continue;

            auto it = std::find(pact.memberIds.begin(),
                pact.memberIds.end(), entityId);
            if (it == pact.memberIds.end()) return;
            pact.memberIds.erase(it);

            auto* entity = state.getEntity(entityId);
            if (entity) {
                if (entity->militaryPactId == pactId) entity->militaryPactId = 0;
                if (entity->economicPactId == pactId) entity->economicPactId = 0;
                state.markEntityDirty(entityId);
            }

            std::cout << "[Diplomacy] Entity " << entityId
                << " left pact " << pactId << "\n";
            break;
        }
    }

    // ── Sprzedaż prowincji ────────────────────────────────────────────────────────

    void DiplomacySystem::processProvinceSale(GameState& state,
        const PlayerInput& input) {
        uint32_t seller = input.entityId;
        uint32_t buyer = input.targetId;
        uint32_t provId = input.provinceId;
        int64_t  price = static_cast<int64_t>(input.floatParam);

        // ZMIANA: Zabezpieczenie przed Undefined Behavior przy oszukiwaniu przez klienta
        if (seller == buyer) return;

        auto* sellerEntity = state.getEntity(seller);
        auto* buyerEntity = state.getEntity(buyer);
        if (!sellerEntity || !buyerEntity) return;
        if (buyerEntity->gold < price) return;

        auto* prov = state.getLandProvince(provId);
        auto* share = prov ? prov->getShare(seller) : nullptr;
        if (!share) return;

        // Transfer złota
        buyerEntity->gold -= price;
        sellerEntity->gold += price;

        // Przenieś kafelki
        for (uint32_t tid : share->ownedTileIds) {
            auto* tile = state.getTile(tid);
            if (tile) { tile->ownerId = buyer; state.markTileDirty(tid); }
        }

        // Przenieś budynki
        for (uint32_t bid : share->buildingIds) {
            auto* b = state.getBuilding(bid);
            if (b) { b->ownerId = buyer; state.markBuildingDirty(bid); }
        }

        // Utwórz lub powiększ share kupującego
        auto& buyerShare = prov->playerShares[buyer];
        buyerShare.entityId = buyer;
        buyerShare.provinceId = provId;

        // Zabezpieczenie zadziałało wyżej (seller != buyer), więc ten kod jest 100% bezpieczny
        buyerShare.ownedTileIds.insert(buyerShare.ownedTileIds.end(),
            share->ownedTileIds.begin(),
            share->ownedTileIds.end());
        buyerShare.buildingIds.insert(buyerShare.buildingIds.end(),
            share->buildingIds.begin(),
            share->buildingIds.end());
        buyerShare.population.workers += share->population.workers;
        buyerShare.population.military += share->population.military;

        // Usuń stary share
        prov->playerShares.erase(seller);

        state.markEntityDirty(seller);
        state.markEntityDirty(buyer);
        state.markShareDirty(provId, buyer);

        std::cout << "[Diplomacy] Province " << provId
            << " sold from " << seller << " to " << buyer << "\n";
    }

    // ── Tick głosowań ─────────────────────────────────────────────────────────────

    void DiplomacySystem::tickPactVotings(GameState& state) {
        for (auto& pact : state.pacts) {
            std::erase_if(pact.pendingApplications, [&](const PactApplication& app) {
                if (app.deadlineTick == 0) return true; // zaakceptowana lub odrzucona
                if (state.currentTick >= app.deadlineTick) {
                    // Deadline minął — odrzucone
                    std::cout << "[Diplomacy] Pact application expired for entity "
                        << app.candidateEntityId << "\n";
                    return true;
                }
                return false;
                });
        }

        // Usuń puste pakty
        // ZMIANA: std::erase_if wspiera std::deque od C++20, jest w 100% bezpieczne
        std::erase_if(state.pacts, [](const Pact& p) {
            return p.memberIds.empty();
            });
    }

    // ── Debuff zdrajcy ────────────────────────────────────────────────────────────

    void DiplomacySystem::tickTraitorDebuffs(GameState& state) {
        for (auto& entity : state.entities) {
            if (entity.traitorDebuffUntilTick == 0) continue;
            if (state.currentTick >= entity.traitorDebuffUntilTick) {
                entity.traitorDebuffUntilTick = 0;
                state.markEntityDirty(entity.id);
            }
        }
    }

    void DiplomacySystem::applyTraitorDebuff(GameState& state,
        uint32_t entityId,
        uint32_t durationTicks) {
        auto* entity = state.getEntity(entityId);
        if (!entity) return;
        entity->traitorDebuffUntilTick = state.currentTick + durationTicks;
        state.markEntityDirty(entityId);
        std::cout << "[Diplomacy] Traitor debuff applied to entity " << entityId << "\n";
    }

    // ── Helpers ───────────────────────────────────────────────────────────────────

    void DiplomacySystem::addEntityToPact(GameState& state,
        uint32_t entityId, uint32_t pactId) {
        Pact* pact = nullptr;
        for (auto& p : state.pacts) {
            if (p.id == pactId) { pact = &p; break; }
        }
        if (!pact) return;

        auto* entity = state.getEntity(entityId);
        if (!entity) return;

        // Sprawdź limit (1 pakt per typ)
        if (pact->type == PactType::Military && entity->militaryPactId != 0) return;
        if (pact->type == PactType::Economic && entity->economicPactId != 0) return;

        pact->memberIds.push_back(entityId);
        if (pact->type == PactType::Military) entity->militaryPactId = pactId;
        else                                   entity->economicPactId = pactId;

        state.markEntityDirty(entityId);
        std::cout << "[Diplomacy] Entity " << entityId
            << " joined pact " << pactId << "\n";
    }

} // namespace gs::server