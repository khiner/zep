#include "zep/mode_tree.h"

namespace Zep {

ZepTreeNode::ZepTreeNode(std::string strName, uint32_t flags) : m_strName(std::move(strName)), m_flags(flags) {}

ZepFileTree::ZepFileTree() {
    // Root node is 'invisible' and always expanded
    root = std::make_shared<ZepFileNode>("Root");
    root->Expand(true);
}

ZepMode_Tree::ZepMode_Tree(ZepEditor &editor, std::shared_ptr<ZepTree> tree, ZepWindow &launchWindow, ZepWindow &window)
    : ZepMode_Vim(editor), m_tree(std::move(tree)), m_window(window) {}

ZepMode_Tree::~ZepMode_Tree() = default;

void ZepMode_Tree::Notify(const std::shared_ptr<ZepMessage> &message) {}

void ZepMode_Tree::BuildTree() {
    auto *buffer = m_window.buffer;

    std::ostringstream strBuffer;
    std::function<void(ZepTreeNode *, uint32_t indent)> fnVisit;

    fnVisit = [&](ZepTreeNode *pNode, uint32_t indent) {
        for (uint32_t i = 0; i < indent; i++) strBuffer << " ";

        strBuffer << (pNode->HasChildren() ? pNode->IsExpanded() ? "~ " : "+ " : "  ");
        strBuffer << pNode->GetName() << std::endl;

        if (pNode->IsExpanded()) {
            for (auto &pChild: pNode->GetChildren()) {
                fnVisit(pChild.get(), indent + 2);
            }
        }
    };

    if (m_tree->root->IsExpanded()) {
        for (const auto &pChild: m_tree->root->GetChildren()) {
            fnVisit(pChild.get(), 0);
        }
    }

    ChangeRecord tempRecord;
    buffer->Clear();
    buffer->Insert(buffer->Begin(), strBuffer.str(), tempRecord);
}

void ZepMode_Tree::Begin(ZepWindow *pWindow) {
    ZepMode_Vim::Begin(pWindow);
    BuildTree();
}

} // namespace Zep
