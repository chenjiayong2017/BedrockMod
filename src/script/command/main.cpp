// Deps: out/script_command.so: out/script_base.so
#include "../base/main.h"
#include <api.h>

#include <csignal>

#include <minecraft/command/Command.h>
#include <minecraft/command/CommandMessage.h>
#include <minecraft/command/CommandOutput.h>
#include <minecraft/command/CommandParameterData.h>
#include <minecraft/command/CommandRegistry.h>
#include <minecraft/command/CommandVersion.h>

#include <StaticHook.h>

#include <SimpleJit.h>

#include <stack>
#include <string>
#include <unordered_map>

struct TestCommand;
struct MyCommandVTable;
struct ParameterDef;

MAKE_FOREIGN_TYPE(TestCommand *, "command");
MAKE_FOREIGN_TYPE(CommandOrigin *, "command-orig");
MAKE_FOREIGN_TYPE(CommandOutput *, "command-outp");
MAKE_FOREIGN_TYPE(MyCommandVTable *, "command-vtable");
MAKE_FOREIGN_TYPE(ParameterDef *, "parameter-def");

namespace scm {
template <> struct convertible<TestCommand *> : foreign_object_is_convertible<TestCommand *> {};
template <> struct convertible<CommandOrigin *> : foreign_object_is_convertible<CommandOrigin *> {};
template <> struct convertible<CommandOutput *> : foreign_object_is_convertible<CommandOutput *> {};
template <> struct convertible<MyCommandVTable *> : foreign_object_is_convertible<MyCommandVTable *> {};
template <> struct convertible<ParameterDef *> : foreign_object_is_convertible<ParameterDef *> {};
} // namespace scm

struct ParameterDef {
  size_t size;
  std::string name;
  typeid_t<CommandRegistry> type;
  bool (CommandRegistry::*parser)(void *, CommandRegistry::ParseToken const &, CommandOrigin const &, int, std::string &,
                                  std::vector<std::string> &) const;
  void (*init)(void *);
  void (*deinit)(void *);
  SCM (*fetch)(TestCommand *, CommandOrigin *, int pos);
};

struct MyCommandVTable {
  std::vector<ParameterDef *> defs;
  std::function<void(TestCommand *self, CommandOrigin *, CommandOutput *)> exec;

  template <typename... T>
  MyCommandVTable(std::function<void(TestCommand *self, CommandOrigin *, CommandOutput *)> exec, T... ts)
      : exec(exec)
      , defs(ts...) {}
};

struct TestCommand : Command {
  MyCommandVTable *vt;

  virtual void execute(CommandOrigin const &orig, CommandOutput &outp) { vt->exec(this, const_cast<CommandOrigin *>(&orig), &outp); }

  TestCommand(MyCommandVTable *vt)
      : Command() {
    this->vt      = vt;
    size_t offset = 0;
    for (auto def : vt->defs) {
      def->init((void *)((size_t)this + sizeof(TestCommand) + offset));
      offset += def->size;
    }
  }

  static TestCommand *create(MyCommandVTable *vt) {
    size_t size = 0;
    for (auto def : vt->defs) size += def->size;
    auto ptr = new (malloc(sizeof(TestCommand) + size)) TestCommand(vt);
    return ptr;
  }
};

SCM_DEFINE_PUBLIC(command_fetch, "command-args", 2, 0, 0, (scm::val<TestCommand *> cmd, scm::val<CommandOrigin *> orig), "Get command arguments") {
  std::stack<SCM> st;
  size_t pos = 0;
  for (auto &def : cmd->vt->defs) {
    st.push(def->fetch(cmd, orig, pos));
    pos += def->size;
  }
  SCM list = SCM_EOL;
  while (!st.empty()) {
    list = scm_cons(st.top(), list);
    st.pop();
  }
  return list;
}

static ParameterDef *messageParameter(temp_string const &name) {
  return new ParameterDef{ .size   = sizeof(CommandMessage),
                           .name   = name,
                           .type   = CommandMessage::type_id(),
                           .parser = &CommandRegistry::parse<CommandMessage>,
                           .init   = (void (*)(void *))dlsym(MinecraftHandle(), "_ZN14CommandMessageC2Ev"),
                           .deinit = (void (*)(void *))dlsym(MinecraftHandle(), "_ZN14CommandMessageD2Ev"),
                           .fetch  = [](TestCommand *cmd, CommandOrigin *orig, int pos) {
                             return scm::to_scm(((CommandMessage *)((size_t)cmd + sizeof(TestCommand) + pos))->getMessage(*orig));
                           } };
}

SCM_DEFINE_PUBLIC(parameter_message, "parameter-message", 1, 0, 0, (scm::val<char *> name), "Message parameter") {
  return scm::to_scm(messageParameter(name));
}

struct MinecraftCommands {
  CommandRegistry &getRegistry();
};

struct CommandRegistryApply {
  std::string name, description;
  int level;
  std::vector<MyCommandVTable *> vts;
};

static CommandRegistry *registry;

THook(void, _ZN9XPCommand5setupER15CommandRegistry, CommandRegistry &reg) {
  original(reg);
  registry = &reg;
}

static void handleCommandApply(CommandRegistryApply &apply) {
  registry->registerCommand(apply.name, apply.description.c_str(), (CommandPermissionLevel)apply.level, (CommandFlag)0, (CommandFlag)0);
  for (auto vt : apply.vts) {
    registry->registerCustomOverload(apply.name.c_str(), CommandVersion(0, INT32_MAX),
                                     gen_function([=]() -> std::unique_ptr<Command> { return std::unique_ptr<Command>(TestCommand::create(vt)); }),
                                     [&](CommandRegistry::Overload &overload) {
                                       size_t offset = sizeof(TestCommand);
                                       for (auto p : vt->defs) {
                                         overload.params.emplace_back(CommandParameterData(p->type, p->parser, p->name.c_str(),
                                                                                           (CommandParameterDataType)0, nullptr, offset, false, -1));
                                         offset += p->size;
                                       }
                                     });
  }
}

SCM_DEFINE_PUBLIC(register_simple_command, "reg-simple-command", 4, 0, 0,
                  (scm::val<char *> name, scm::val<char *> description, scm::val<int> level,
                   scm::callback<void, TestCommand *, CommandOrigin *, CommandOutput *> cb),
                  "Register simple command") {
  CommandRegistryApply apply{ .name = name.get(), .description = description.get(), .level = level.get() };
  apply.vts.emplace_back(new MyCommandVTable(cb));
  scm_gc_protect_object(cb.scm);
  handleCommandApply(apply);
  return SCM_BOOL_T;
}

SCM_DEFINE_PUBLIC(make_vtable, "command-vtable", 2, 0, 0,
                  (scm::slist<scm::val<ParameterDef *>> params, scm::callback<void, TestCommand *, CommandOrigin *, CommandOutput *> cb),
                  "Make vtable for custom command") {
  scm_gc_protect_object(cb.scm);
  auto ret = new MyCommandVTable(cb);
  for (auto def : params) { ret->defs.emplace_back(def); }
  return scm::to_scm(ret);
}

SCM_DEFINE_PUBLIC(register_command, "reg-command", 4, 0, 0,
                  (scm::val<char *> name, scm::val<char *> description, scm::val<int> level, scm::slist<MyCommandVTable *> vts),
                  "Register simple command") {
  CommandRegistryApply apply{ .name = name.get(), .description = description.get(), .level = level.get() };
  for (auto vt : vts) { apply.vts.emplace_back(vt); }
  handleCommandApply(apply);
  return SCM_BOOL_T;
}

SCM_DEFINE_PUBLIC(outp_add, "outp-add", 2, 0, 0, (scm::val<CommandOutput *> outp, scm::val<char *> msg), "Add message to command output") {
  outp->addMessage(msg.get());
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(outp_success, "outp-success", 1, 1, 0, (scm::val<CommandOutput *> outp, scm::val<char *> msg), "Set command output to success") {
  if (scm_is_string(msg.scm)) outp->addMessage(msg.get());
  outp->success();
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(outp_error, "outp-error", 2, 0, 0, (scm::val<CommandOutput *> outp, scm::val<char *> msg), "Set command output to success") {
  outp->error(msg.get());
  return SCM_UNSPECIFIED;
}

struct CommandOrigin {
  virtual ~CommandOrigin();
  virtual std::string getRequestId();
  virtual std::string getName();
  virtual BlockPos getBlockPosition();
  virtual Vec3 getWorldPosition();
  virtual Level &getLevel();
  virtual void *getDimension();
  virtual Actor &getEntity();
  virtual int getPermissionsLevel();
  virtual CommandOrigin *clone();
  virtual bool canCallHiddenCommands();
  virtual bool hasChatPerms();
  virtual bool hasTellPerms();
  virtual bool canUseAbility(std::string);
  virtual void *getSourceId();
  virtual void *getSourceSubId();
  virtual void *getOutputReceiver();
  virtual int getOriginType();
  virtual void *toCommandOriginData();
  virtual mce::UUID const &getUUID();
  virtual bool mayOverrideName();
};

SCM_DEFINE_PUBLIC(orig_type, "orig-type", 1, 0, 0, (scm::val<CommandOrigin *> orig), "Get CommandOrigin type") {
  return scm::to_scm(orig->getOriginType());
}

SCM_DEFINE_PUBLIC(orig_player, "orig-player", 1, 0, 0, (scm::val<CommandOrigin *> orig), "Get CommandOrigin type") {
  if (orig->getOriginType() == 0) { return scm::to_scm((ServerPlayer *)&orig->getEntity()); }
  return SCM_BOOL_F;
}

PRELOAD_MODULE("minecraft command") {
#ifndef DIAG
#include "main.x"
#endif
}