#include "zep/buffer.h"
#include "zep/editor.h"
#include "zep/syntax.h"
#include "TestDisplay.h"

#include <gtest/gtest.h>

using namespace Zep;
struct SyntaxTest : public testing::Test {
    SyntaxTest() {
        editor = std::make_shared<ZepEditor>(&display, "", ZepEditorFlags::DisableThreads, nullptr);
    }

    ~SyntaxTest() override = default;

    TestDisplay display{};
    std::shared_ptr<ZepEditor> editor;
};

// Given a filename and a sample text, check the syntax colouring returns the right thing
#define SYNTAX_TEST(name, filename, source, offset, color)         \
    TEST_F(SyntaxTest, name)                                       \
    {                                                              \
        ZepBuffer *pBuffer = editor->GetEmptyBuffer(filename);     \
        pBuffer->SetText(source);                                  \
        ASSERT_EQ(pBuffer->syntax->GetSyntaxAt(GlyphIterator(pBuffer, offset)).foreground, ThemeColor::color); \
    };

#define CPP_SYNTAX_TEST(name, source, offset, color) SYNTAX_TEST(name, "test.cpp", source, offset, color)

CPP_SYNTAX_TEST(cpp_keyword, "int i;", 0, Keyword);
CPP_SYNTAX_TEST(cpp_identifier, "a = std::min(a,b);", 4, Identifier);
CPP_SYNTAX_TEST(cpp_string, "a = \"hello\";", 4, String);
CPP_SYNTAX_TEST(cpp_number, "a = 1234;", 4, Number);
