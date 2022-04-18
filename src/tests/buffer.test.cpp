#include "config_app.h"
#include "zep/logger.h"

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/editor.h"
#include <gtest/gtest.h>

using namespace Zep;
struct BufferTest : public testing::Test {
    BufferTest() {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        editor = std::make_shared<ZepEditor>(new ZepDisplayNull(), ZEP_ROOT, ZepEditorFlags::DisableThreads);
        pBuffer = editor->InitWithText("", "");
    }

    ~BufferTest() {}

    std::shared_ptr<ZepEditor> editor;
    ZepBuffer *pBuffer;
};

TEST_F(BufferTest, CreatedProperly
)
{
ASSERT_TRUE(pBuffer
->
workingBuffer
.
size()
== 1);
}

TEST_F(BufferTest, DefaultConstructedWith0
)
{
auto pNew = std::make_shared<ZepBuffer>(*editor, std::string("empty"));
ASSERT_TRUE(pNew
->
workingBuffer
.
size()
== 1);
}

TEST_F(BufferTest, FindFirstOf
)
{
auto pNew = std::make_shared<ZepBuffer>(*editor, std::string("empty"));
pNew->SetText("Hello");

int32_t char_index;
auto loc = pNew->FindFirstCharOf(pNew->Begin(), "zo", char_index, Direction::Forward);
ASSERT_TRUE(char_index
== 1 && loc.
Index()
== 4);

loc = pNew->FindFirstCharOf(pNew->Begin(), "H", char_index, Direction::Forward);
ASSERT_TRUE(char_index
== 0 && loc.
Index()
== 0);

loc = pNew->Begin() + 4;
loc = pNew->FindFirstCharOf(loc, "o", char_index, Direction::Backward);
ASSERT_TRUE(char_index
== 0 && loc.
Index()
== 4);

loc = pNew->Begin() + 4;
loc = pNew->FindFirstCharOf(loc, "H", char_index, Direction::Backward);
ASSERT_TRUE(char_index
== 0 && loc.
Index()
== 0);
}

// TODO
