# 13 — System dyplomacji (DiplomacySystem)

## Odpowiedzialność

Przetwarza komendy dyplomatyczne graczy (deklaracje wojny, propozycje sojuszy, głosowania w paktach, sprzedaż prowincji), aktualizuje relacje i pakty.

## tick()

```cpp
void DiplomacySystem::tick(GameState& state) {
    // Przetwórz komendy dyplomatyczne z kolejki inputów
    for (auto& input : state.pendingDiplomacyInputs) {
        switch (input.diplomacyType) {
            case DiplomacyInput::DeclareWar:
                processWarDeclaration(state, input);   break;
            case DiplomacyInput::ProposePact:
                processPactProposal(state, input);     break;
            case DiplomacyInput::PactVote:
                processPactVote(state, input);         break;
            case DiplomacyInput::SellProvince:
                processProvinceSale(state, input);     break;
            case DiplomacyInput::LeavePact:
                processPactLeave(state, input);        break;
        }
    }
    state.pendingDiplomacyInputs.clear();

    // Tick głosowań — wygasłe = odrzucone
    tickPactVotings(state);

    // Tick debuffu zdrajcy
    tickTraitorDebuffs(state);
}
```

## Deklaracja wojny

```cpp
void DiplomacySystem::processWarDeclaration(GameState& state,
                                             const DiplomacyInput& input) {
    uint32_t a = input.entityId;
    uint32_t b = input.targetEntityId;

    setRelationFlag(state, a, b, RelationFlag::AtWar);

    // Zerwij sojusze bilateralne
    clearRelationFlag(state, a, b, RelationFlag::BilateralAlly);
    clearRelationFlag(state, a, b, RelationFlag::BilateralTrade);

    // Automatyczne embargo
    setRelationFlag(state, a, b, RelationFlag::Embargo);
    setRelationFlag(state, b, a, RelationFlag::Embargo);

    // Debuff zdrajcy jeśli A był w pakcie militarnym z B
    if (state.inSameMilitaryPact(a, b)) {
        applyTraitorDebuff(state, a, 3000);  // 3000 ticków
    }

    state.pendingEvents.push_back(makeWarEvent(a, b));
}
```

## Sprzedaż prowincji

```cpp
void DiplomacySystem::processProvinceSale(GameState& state,
                                           const DiplomacyInput& input) {
    uint32_t seller = input.entityId;
    uint32_t buyer  = input.targetEntityId;
    uint32_t provId = input.provinceId;
    int64_t  price  = input.goldAmount;

    auto* sellerEntity = &state.entities[seller];
    auto* buyerEntity  = &state.entities[buyer];
    auto* share        = state.getShare(provId, seller);

    if (!share) return;
    if (buyerEntity->gold < price) return;

    // Transfer złota
    buyerEntity->gold  -= price;
    sellerEntity->gold += price;

    // Przenieś kafelki, budynki, populację
    for (uint32_t tileId : share->ownedTileIds) {
        state.tiles[tileId].ownerId = buyer;
        state.dirtyTiles.insert(tileId);
    }
    // Przenieś budynki
    for (uint32_t bid : share->buildingIds) {
        state.buildings[bid].ownerId = buyer;
    }
    // Przenieś populację do nowego share buyera
    auto& buyerShare = getOrCreateShare(state, provId, buyer);
    buyerShare.population.workers  += share->population.workers;
    buyerShare.population.military += share->population.military;
    buyerShare.ownedTileIds.insert(buyerShare.ownedTileIds.end(),
                                   share->ownedTileIds.begin(),
                                   share->ownedTileIds.end());
    buyerShare.buildingIds.insert(buyerShare.buildingIds.end(),
                                  share->buildingIds.begin(),
                                  share->buildingIds.end());

    // Usuń stary share
    state.landProvinces[provId].playerShares.erase(seller);

    // Rebuild grafów elektrycznych
    state.electricSystem->rebuildGrids(state);
}
```

## Zależności

- `GameState` — relations, pacts, entities, landProvinces
- `ElectricSystem` — rebuild po sprzedaży prowincji z budynkami elektrycznymi
