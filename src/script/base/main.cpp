#include <StaticHook.h>
#include <api.h>

MAKE_FOREIGN_TYPE(mce::UUID, "uuid", { "t0", "t1" });
MAKE_FOREIGN_TYPE(Actor *, "actor");
MAKE_FOREIGN_TYPE(ItemActor *, "item-actor");
MAKE_FOREIGN_TYPE(ServerPlayer *, "player");
MAKE_FOREIGN_TYPE(ItemInstance *, "item-instance");

extern "C" Minecraft *support_get_minecraft();

#include "main.h"

#ifndef DIAG
static_assert(sizeof(void *) == 8, "Only works in 64bit");
#endif

SCM_DEFINE_PUBLIC(c_make_uuid, "uuid", 1, 0, 0, (scm::val<std::string> name), "Create UUID") {
  std::string temp = name;
  return scm::to_scm(mce::UUID::fromStringFix(temp));
}

SCM_DEFINE_PUBLIC(c_uuid_to_string, "uuid->string", 1, 0, 0, (scm::val<mce::UUID> uuid), "UUID to string") {
  return scm::to_scm(uuid.get().asString());
}

SCM_DEFINE_PUBLIC(c_uuid_eq, "uuid=?", 2, 0, 0, (scm::val<mce::UUID> uuid1, scm::val<mce::UUID> uuid2), "UUID equal test") {
  return scm::to_scm(uuid1.get() == uuid2.get());
}

struct TeleportCommand {
  void teleport(Actor &actor, Vec3 pos, Vec3 *center, DimensionId dim) const;
};

void teleport(Actor &actor, Vec3 target, int dim, Vec3 *center) {
  auto real = center != nullptr ? *center : target;
  ((TeleportCommand *)nullptr)->teleport(actor, target, &real, { dim });
}

SCM_DEFINE_PUBLIC(c_teleport, "teleport", 3, 0, 0, (scm::val<Actor *> actor, scm::val<Vec3> target, scm::val<int> dim), "Teleport actor") {
  teleport(*actor.get(), target, dim, nullptr);
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(actor_position, "actor-pos", 1, 0, 0, (scm::val<Actor *> actor), "Get position of actor") { return scm::to_scm(actor->getPos()); }

SCM_DEFINE_PUBLIC(actor_dim, "actor-dim", 1, 0, 0, (scm::val<Actor *> actor), "Get actor dim") { return scm::to_scm(actor->getDimensionId()); }

SCM_DEFINE_PUBLIC(player_spawnpoint, "player-spawnpoint", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get spawnpoint of player") {
  return scm::to_scm(player->getSpawnPosition());
}

SCM_DEFINE_PUBLIC(player_set_spawnpoint, "set-player-spawnpoint", 2, 0, 0, (scm::val<ServerPlayer *> player, scm::val<BlockPos> pos),
                  "Set spawnpoint of player") {
  return scm::to_scm(player->setBedRespawnPosition(pos));
}

SCM_DEFINE_PUBLIC(level_spawnpoint, "world-spawnpoint", 0, 0, 0, (scm::val<ServerPlayer *> player), "Get spawnpoint of level") {
  return scm::to_scm(support_get_minecraft()->getLevel().getDefaultSpawn());
}

SCM_DEFINE_PUBLIC(c_actor_eq, "actor=?", 2, 0, 0, (scm::val<Actor *> act1, scm::val<Actor *> act2), "Test equal of two actors") {
  return scm::to_scm(act1.get() == act2.get());
}

SCM_DEFINE_PUBLIC(c_actor_dim, "actor-dim", 1, 0, 0, (scm::val<Actor *> act), "Get actor's dimension") { return scm::to_scm(act->getDimensionId()); }

SCM_DEFINE_PUBLIC(c_actor_dim_set, "actor-dim-set!", 2, 1, 0, (scm::val<Actor *> act, scm::val<int> dim, scm::val<bool> showCredit),
                  "Set actor's dimension") {
  if (auto player = dynamic_cast<ServerPlayer *>(act.get()); player)
    player->changeDimension(DimensionId(dim), showCredit[false]);
  else
    act->changeDimension(DimensionId(dim), showCredit[false]);
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_actor_debug, "actor-debug-info", 1, 0, 0, (scm::val<Actor *> act), "Get Actor's debug info") {
  std::vector<std::string> vect;
  act->getDebugText(vect);
  SCM list = SCM_EOL;
  for (auto it = vect.rbegin(); it != vect.rend(); it++) { list = scm_cons(scm::to_scm(*it), list); }
  return list;
}

SCM_DEFINE_PUBLIC(c_actor_name, "actor-name", 1, 0, 0, (scm::val<Actor *> act), "Return Actor's name") { return scm::to_scm(act->getNameTag()); }

SCM_DEFINE_PUBLIC(c_for_each_player, "for-each-player", 1, 0, 0, (scm::callback<bool, ServerPlayer *> callback), "Invoke function for each player") {
  support_get_minecraft()->getLevel().forEachPlayer([=](Player &p) { return callback((ServerPlayer *)&p); });
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_kick, "player-kick", 1, 0, 0, (scm::val<ServerPlayer *> player), "Kick player from server") {
  kickPlayer(player);
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_permission_level, "player-permission-level", 1, 0, 0, (scm::val<ServerPlayer *> player),
                  "Get player's permission level.") {
  return scm::to_scm(player->getCommandPermissionLevel());
}

SCM_DEFINE_PUBLIC(c_player_uuid, "player-uuid", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get Player's UUID") {
  return scm::to_scm(player->getUUID());
}

SCM_DEFINE_PUBLIC(c_player_xuid, "player-xuid", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get Player's XUID") {
  return scm::to_scm(player->getXUID());
}

SCM_DEFINE_PUBLIC(c_player_stats, "player-stats", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get Player's stats info") {
  auto status = ServerCommand::mGame->getNetworkHandler().getPeerForUser(player->getClientId()).getNetworkStatus();
  return scm::list(status.ping, status.avgping, status.packetloss, status.avgpacketloss);
}

SCM_DEFINE_PUBLIC(c_player_open_inventory, "player-open-inventory", 1, 0, 0, (scm::val<ServerPlayer *> player), "Open player's inventory") {
  player->openInventory();
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_suspend, "player-suspend", 1, 0, 0, (scm::val<ServerPlayer *> player), "Suspend Player") {
  player->getLevel().suspendPlayer(*player.get());
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_resume, "player-resume", 1, 0, 0, (scm::val<ServerPlayer *> player), "Suspend Player") {
  player->getLevel().resumePlayer(*player.get());
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_is_survival, "player-survival?", 1, 0, 0, (scm::val<ServerPlayer *> player), "Check if player is in survival mode") {
  return scm::to_scm(player->isSurvival());
}

SCM_DEFINE_PUBLIC(c_player_is_creative, "player-creative?", 1, 0, 0, (scm::val<ServerPlayer *> player), "Check if player is in creative mode") {
  return scm::to_scm(player->isCreative());
}

SCM_DEFINE_PUBLIC(c_player_is_adventure, "player-adventure?", 1, 0, 0, (scm::val<ServerPlayer *> player), "Check if player is in adventure mode") {
  return scm::to_scm(player->isAdventure());
}

SCM_DEFINE_PUBLIC(c_player_is_worldbuilder, "player-worldbuilder?", 1, 0, 0, (scm::val<ServerPlayer *> player), "Check if player is worldbuilder") {
  return scm::to_scm(player->isWorldBuilder());
}

SCM_DEFINE_PUBLIC(vec3_to_blockpos, "vec3->blockpos", 1, 0, 0, (scm::val<Vec3> vec3), "Convert Vec3 to BlockPos") {
  return scm::to_scm(BlockPos(vec3.get()));
}

SCM_DEFINE_PUBLIC(blockpos_to_vec3, "blockpos->vec3", 1, 0, 0, (scm::val<BlockPos> pos), "Convert Vec3 to BlockPos") {
  return scm::to_scm(Vec3(pos));
}

SCM_DEFINE_PUBLIC(blockpos_to_vec3_center, "blockpos->vec3/center", 1, 0, 0, (scm::val<BlockPos> pos), "Convert Vec3 to BlockPos") {
  return scm::to_scm(Vec3(pos.get().x + 0.5, pos.get().y + 0.5, pos.get().z + 0.5));
}

SCM_DEFINE_PUBLIC(item_instance_null_p, "item-instance-null?", 1, 0, 0, (scm::val<ItemInstance *> instance), "Check ItemInstance is null") {
  return scm::to_scm(instance->isNull());
}

SCM_DEFINE_PUBLIC(item_instance_to_string, "item-instance-debug-info", 1, 0, 0, (scm::val<ItemInstance *> instance),
                  "Get Debug Info of the ItemInstance") {
  return scm::to_scm(instance->toString());
}

SCM_DEFINE_PUBLIC(item_instance_name, "item-instance-name", 1, 0, 0, (scm::val<ItemInstance *> instance), "Get the name of the ItemInstance") {
  return scm::to_scm(instance->getName());
}

SCM_DEFINE_PUBLIC(item_instance_id, "item-instance-id", 1, 0, 0, (scm::val<ItemInstance *> instance), "Get ItemInstancee's id") {
  return scm::to_scm(instance->getId());
}

SCM_DEFINE_PUBLIC(item_id_from_string, "lookup-item-id", 1, 0, 0, (scm::val<std::string> name), "Get Item Id from name") {
  auto item = ItemRegistry::lookupByName(name, true);
  if (!item) return SCM_BOOL_F;
  return scm::to_scm(item->getId());
}

SCM_DEFINE_PUBLIC(item_actor_name, "item-actor-name", 1, 0, 0, (scm::val<ItemActor *> act), "Get ItemActor's name") {
  return scm::to_scm(getEntityName(*(Actor *)act));
}

SCM_DEFINE_PUBLIC(item_actor_to_instance, "item-actor->item-instance", 1, 0, 0, (scm::val<ItemActor *> act), "Get ItemInstance from ItemActor") {
  return scm::to_scm(&act->getItemInstance());
}

MAKE_HOOK(player_login, "player-login", mce::UUID);
MAKE_FLUID(bool, login_result, "login-result");

struct Whitelist {};

TInstanceHook(bool, _ZNK9Whitelist9isAllowedERKN3mce4UUIDERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE, Whitelist, mce::UUID &uuid,
              std::string const &msg) {
  return login_result()[true] <<= [=] { player_login(uuid); };
}

extern "C" const char *mcpelauncher_get_profile();

std::unordered_map<ServerPlayer *, scm::hook<>> playerLeftHooks;

SCM_DEFINE_PUBLIC(c_when_player_leave, "player-left-for", 1, 0, 0, (scm::val<ServerPlayer *> player), "player leave hook") {
  if (playerLeftHooks.count(player) > 0) { scm_gc_protect_object(playerLeftHooks[player]); }
  return playerLeftHooks[player];
}

LOADFILE(preload, "src/script/base/preload.scm");

PRELOAD_MODULE("minecraft base") {
  scm::definer("*profile*") = mcpelauncher_get_profile();
  scm_c_export("*profile*");

  onPlayerJoined <<= scm::define_hook<ServerPlayer &>("player-joined");
  onPlayerLeft <<= scm::define_hook<ServerPlayer &>("player-left");

  onPlayerLeft <<= [](ServerPlayer &player) {
    if (playerLeftHooks.count(&player) > 0) {
      playerLeftHooks[&player]();
      scm_gc_unprotect_object(playerLeftHooks[&player]);
      playerLeftHooks.erase(&player);
    }
  };

#ifndef DIAG
#include "main.x"
#include "preload.scm.z"
#endif

  scm_c_eval_string(&file_preload_start);
}

extern "C" void mod_set_server(void *v) { support_get_minecraft()->activateWhitelist(); }
