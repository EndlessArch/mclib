#include "PlayerManager.h"
#include "Packets/PacketDispatcher.h"

namespace Minecraft {

PlayerManager::PlayerManager(Packets::PacketDispatcher* dispatcher, EntityManager* entityManager)
    : Packets::PacketHandler(dispatcher),
      m_EntityManager(entityManager)
{
    dispatcher->RegisterHandler(Protocol::State::Play, Protocol::Play::PlayerListItem, this);

    m_EntityManager->RegisterListener(this);
}

PlayerManager::~PlayerManager() {
    m_EntityManager->UnregisterListener(this);
}

PlayerManager::iterator PlayerManager::begin() {
    return m_Players.begin();
}

PlayerManager::iterator PlayerManager::end() {
    return m_Players.end();
}

void PlayerManager::OnPlayerSpawn(PlayerEntityPtr entity, UUID uuid) {
    m_Players[uuid]->SetEntity(entity);

    NotifyListeners(&PlayerListener::OnPlayerSpawn, m_Players[uuid]);
}

void PlayerManager::OnEntityDestroy(EntityPtr entity) {
    EntityId eid = entity->GetEntityId();

    auto player = GetPlayerByEntityId(eid);

    if (player) {
        player->SetEntity(PlayerEntityPtr());
        NotifyListeners(&PlayerListener::OnPlayerDestroy, player, eid);
    }
}

void PlayerManager::OnEntityMove(EntityPtr entity, Vector3d oldPos, Vector3d newPos) {
    EntityId eid = entity->GetEntityId();
    PlayerPtr player = GetPlayerByEntityId(eid);

    if (!player) return;

    NotifyListeners(&PlayerListener::OnPlayerMove, player, oldPos, newPos);
}

PlayerPtr PlayerManager::GetPlayerByUUID(UUID uuid) const {
    if (m_Players.find(uuid) != m_Players.end())
        return m_Players.at(uuid);

    return nullptr;
}

PlayerPtr PlayerManager::GetPlayerByEntityId(EntityId eid) const {
    auto iter = std::find_if(m_Players.begin(), m_Players.end(), [eid](std::pair<UUID, PlayerPtr> kv) {
        PlayerPtr player = kv.second;
        auto ptr = player->GetEntity();
        if (!ptr) return false;
        return ptr->GetEntityId() == eid;
    });

    if (iter != m_Players.end())
        return iter->second;

    return nullptr;
}

void PlayerManager::HandlePacket(Packets::Inbound::PlayerListItemPacket* packet) {
    using namespace Packets::Inbound;

    auto action = packet->GetAction();
    const auto& actionDataList = packet->GetActionData();

    for (const auto& actionData : actionDataList) {
        UUID uuid = actionData->uuid;

        if (action == PlayerListItemPacket::Action::AddPlayer) {
            PlayerPtr player;

            player = std::make_shared<Player>(uuid, actionData->name);

            m_Players[uuid] = player;

            NotifyListeners(&PlayerListener::OnPlayerJoin, m_Players[uuid]);
        } else if (action == PlayerListItemPacket::Action::RemovePlayer) {
            NotifyListeners(&PlayerListener::OnPlayerLeave, m_Players[uuid]);

            m_Players.erase(uuid);
        }
    }
}

} // ns Minecraft
