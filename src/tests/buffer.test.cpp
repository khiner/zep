#include "zep/logger.h"

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/editor.h"
#include <gtest/gtest.h>

#include "TestDisplay.h"

using namespace Zep;

struct BufferTest : public testing::Test {
    BufferTest() {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        editor = std::make_shared<ZepEditor>(&display, "", ZepEditorFlags::DisableThreads, nullptr);
        pBuffer = editor->InitWithText("", "");
    }

    ~BufferTest() override = default;

    TestDisplay display{};
    std::shared_ptr<ZepEditor> editor;
    ZepBuffer *pBuffer;
};

TEST_F(BufferTest, CreatedProperly) {
    ASSERT_TRUE(pBuffer->workingBuffer.size() == 1);
}

//TEST_F(BufferTest, FindFirstOf) {
//    auto newEditor = std::make_shared<ZepBuffer>(editor.get(), std::string("empty"));
//    newEditor->SetText("Hello");
//
//    int32_t char_index;
//    auto loc = newEditor->FindFirstCharOf(newEditor->Begin(), "zo", char_index, Direction::Forward);
//    ASSERT_TRUE(char_index == 1 && loc.Index() == 4);
//
//    loc = newEditor->FindFirstCharOf(newEditor->Begin(), "H", char_index, Direction::Forward);
//    ASSERT_TRUE(char_index == 0 && loc.Index() == 0);
//
//    loc = newEditor->Begin() + 4;
//    loc = newEditor->FindFirstCharOf(loc, "o", char_index, Direction::Backward);
//    ASSERT_TRUE(char_index == 0 && loc.Index() == 4);
//
//    loc = newEditor->Begin() + 4;
//    loc = newEditor->FindFirstCharOf(loc, "H", char_index, Direction::Backward);
//    ASSERT_TRUE(char_index == 0 && loc.Index() == 0);
//}
