#pragma once

#include "zep/mode.h"
#include <future>
#include <memory>
#include <regex>

namespace Zep {

enum class ReplParseType {
    SubExpression,
    OuterExpression,
    Line,
    All
};

// A provider that can handle repl commands.
// This is just a default repl that does nothing; if you want to provide a repl,
// you need to register this interface and handle the messages to run the repl.
struct IZepReplProvider {
    virtual std::string ReplParse(ZepBuffer &text, const GlyphIterator &cursorOffset, ReplParseType type) = 0;
    virtual std::string ReplParse(const std::string &text) = 0;
    virtual bool ReplIsFormComplete(const std::string &input, int &depth) = 0;
};

struct ZepReplExCommand : public ZepExCommand {
    ZepReplExCommand(ZepEditor &editor, IZepReplProvider *pProvider);

    static void Register(ZepEditor &editor, IZepReplProvider *pProvider);

    void Run(const std::vector<std::string> &args) override;
    const char *ExCommandName() const override { return "ZRepl"; }
    const KeyMap *GetKeyMappings(ZepMode &) const override { return &m_keymap; }
    bool AddKeyPress(uint32_t key, uint32_t modifiers);

private:
    void Prompt();
    void MoveToEnd();

private:
    IZepReplProvider *m_pProvider = nullptr;
    ZepBuffer *m_pReplBuffer = nullptr;
    ZepWindow *m_pReplWindow = nullptr;
    KeyMap m_keymap;
    GlyphIterator m_startLocation;
};

struct ZepReplEvaluateOuterCommand : public ZepExCommand {
    ZepReplEvaluateOuterCommand(ZepEditor &editor, IZepReplProvider *provide);

    static void Register(ZepEditor &editor, IZepReplProvider *provider);

    void Notify(const std::shared_ptr<ZepMessage> &message) override { ZEP_UNUSED(message); }
    void Run(const std::vector<std::string> &args) override;
    const char *ExCommandName() const override { return "ZReplEvalOuter"; }
    const KeyMap *GetKeyMappings(ZepMode &) const override { return &m_keymap; }
private:
    IZepReplProvider *m_pProvider = nullptr;
    KeyMap m_keymap;
};

struct ZepReplEvaluateCommand : public ZepExCommand {
    ZepReplEvaluateCommand(ZepEditor &editor, IZepReplProvider *pProvider);

    static void Register(ZepEditor &editor, IZepReplProvider *pProvider);

    void Notify(const std::shared_ptr<ZepMessage> &message) override { ZEP_UNUSED(message); }
    void Run(const std::vector<std::string> &args) override;
    const char *ExCommandName() const override { return "ZReplEval"; }
    const KeyMap *GetKeyMappings(ZepMode &) const override { return &m_keymap; }
private:
    IZepReplProvider *m_pProvider = nullptr;
    KeyMap m_keymap;
};

struct ZepReplEvaluateInnerCommand : public ZepExCommand {
    ZepReplEvaluateInnerCommand(ZepEditor &editor, IZepReplProvider *pProvider);

    static void Register(ZepEditor &editor, IZepReplProvider *pProvider);

    void Notify(const std::shared_ptr<ZepMessage> &message) override { ZEP_UNUSED(message); }
    void Run(const std::vector<std::string> &args) override;
    const char *ExCommandName() const override { return "ZReplEvalInner"; }
    const KeyMap *GetKeyMappings(ZepMode &) const override { return &m_keymap; }
private:
    IZepReplProvider *m_pProvider = nullptr;
    KeyMap m_keymap;
};


} // namespace Zep
