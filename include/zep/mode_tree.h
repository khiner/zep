#pragma once

#include "mode.h"
#include "zep/mode_vim.h"

namespace Zep {

namespace ZepTreeNodeFlags {
enum {
    None = (0),
    IsFolder = (1 << 0)
};
}

class ZepTreeNode {
public:
    using TNode = std::shared_ptr<ZepTreeNode>;
    using TChildren = std::vector<TNode>;

    explicit ZepTreeNode(std::string strName, uint32_t flags = ZepTreeNodeFlags::None);

    virtual const std::string &GetName() const { return m_strName; }
    virtual void SetName(const std::string &name) { m_strName = name; }
    virtual ZepTreeNode *GetParent() const { return m_pParent; }
    virtual TChildren GetChildren() const { return m_children; }
    virtual bool HasChildren() const { return !m_children.empty(); }

    virtual ZepTreeNode *AddChild(const TNode &pNode) {
        m_children.push_back(pNode);
        pNode->m_pParent = this;
        return pNode.get();
    }

    virtual void ClearChildren() { m_children.clear(); }

    virtual bool IsExpanded() const { return m_expanded; }
    virtual void Expand(bool expand) { m_expanded = expand; }

    virtual void ExpandAll(bool expand) {
        using fnVisit = std::function<void(ZepTreeNode *pNode, bool ex)>;
        fnVisit visitor = [&](ZepTreeNode *pNode, bool ex) {
            pNode->Expand(ex);
            if (pNode->HasChildren()) {
                for (auto &pChild: pNode->GetChildren()) {
                    visitor(pChild.get(), ex);
                }
            }
        };
        visitor(this, expand);
    }

    virtual void SetParent(ZepTreeNode *pParent) { m_pParent = pParent; }
protected:
    bool m_expanded = false;
    ZepTreeNode *m_pParent = nullptr;
    TChildren m_children;
    std::string m_strName;
    uint32_t m_flags = ZepTreeNodeFlags::None;
};

struct ZepTree {
    ZepTreeNode::TNode root;
};

class ZepFileNode : public ZepTreeNode {
public:
    explicit ZepFileNode(const std::string &name, uint32_t flags = ZepTreeNodeFlags::None) : ZepTreeNode(name, flags) {}
};

class ZepFileTree : public ZepTree {
public:
    ZepFileTree();

};

class ZepMode_Tree : public ZepMode_Vim {
public:
    ZepMode_Tree(ZepEditor &editor, std::shared_ptr<ZepTree> spTree, ZepWindow &launchWindow, ZepWindow &replWindow);
    ~ZepMode_Tree() override;

    static const char *StaticName() { return "TREE"; }
    void Begin(ZepWindow *pWindow) override;
    void Notify(const std::shared_ptr<ZepMessage> &message) override;
    const char *Name() const override { return StaticName(); }

private:
    void BuildTree();

private:
    std::shared_ptr<ZepTree> m_spTree;
    ZepWindow &m_window;
};

} // namespace Zep
