#include "config_app.h"
#include "zep/logger.h"

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/editor.h"
#include "zep/mode_standard.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include <gtest/gtest.h>

using namespace Zep;
struct StandardTest : public testing::Test {
    StandardTest() {
        // Disable threads for consistent tests, at the expense of not catching thread errors!
        // TODO : Fix/understand test failures with threading
        editor = std::make_shared<ZepEditor>(new ZepDisplayNull(), ZEP_ROOT, ZepEditorFlags::DisableThreads);
        pBuffer = editor->InitWithText("Test Buffer", "");

        pTabWindow = editor->activeTabWindow;
        pWindow = editor->activeTabWindow->GetActiveWindow();

        // Setup editor with a default fixed_size so that text doesn't wrap and confuse the tests!
        editor->SetDisplayRegion({0.0f, 0.0f, 1024.0f, 1024.0f});

        pWindow->SetBufferCursor(pBuffer->Begin());

        editor->SetGlobalMode(Zep::ZepMode_Standard::StaticName());
        mode = editor->GetGlobalMode();
    }

    ~StandardTest() {}

    std::shared_ptr<ZepEditor> editor;
    ZepBuffer *pBuffer;
    ZepWindow *pWindow;
    ZepTabWindow *pTabWindow;
    ZepMode *mode;
};

TEST_F(StandardTest, CheckDisplaySucceeds
)
{
pBuffer->SetText("Some text to display\nThis is a test.");
editor->
SetDisplayRegion({0.0f, 0.0f, 1024.0f, 1024.0f})
);
ASSERT_NO_FATAL_FAILURE(editor
->
Display()
);
ASSERT_FALSE(pTabWindow
->
GetWindows()
.
empty()
);
}

#define PARSE_COMMAND(command)                                \
    bool mod_marker = false;                                  \
    int mod = 0;                                              \
    for (auto& ch : command)                                  \
    {                                                         \
        if (ch == 0)                                          \
            continue;                                         \
        if (ch == '%')                                        \
        {                                                     \
            mod_marker = true;                                \
            continue;                                         \
        }                                                     \
        if (mod_marker)                                       \
        {                                                     \
            mod_marker = false;                               \
            if (ch == 's')                                    \
            {                                                 \
                mod |= ModifierKey::Shift;                    \
                continue;                                     \
            }                                                 \
            else if (ch == 'c')                               \
            {                                                 \
                mod |= ModifierKey::Ctrl;                     \
                continue;                                     \
            }                                                 \
            else if (ch == 'r')                               \
            {                                                 \
                mode->AddKeyPress(ExtKeys::RIGHT, mod);     \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'l')                               \
            {                                                 \
                mode->AddKeyPress(ExtKeys::LEFT, mod);      \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'u')                               \
            {                                                 \
                mode->AddKeyPress(ExtKeys::UP, mod);        \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'd')                               \
            {                                                 \
                mode->AddKeyPress(ExtKeys::DOWN, mod);      \
                mod = 0;                                      \
            }                                                 \
            else if (ch == 'x')                               \
            {                                                 \
                mode->AddKeyPress(ExtKeys::BACKSPACE, mod); \
                mod = 0;                                      \
            }                                                 \
        }                                                     \
        else if (ch == '\n')                                  \
        {                                                     \
            mode->AddKeyPress(ExtKeys::RETURN, mod);        \
            mod = 0;                                          \
        }                                                     \
        else                                                  \
        {                                                     \
            mode->AddKeyPress(ch, mod);                     \
            mod = 0;                                          \
        }                                                     \
    }

// TODO
// These tests were written before i added conversion of modifiers from Ext to <C-f>, etc.
// So this odd syntax probably isn't necessary, and we could call the mapped function with the
// decoded keystrokes
// Given a sample text, a keystroke list and a target text, check the test returns the right thing
#define COMMAND_TEST(name, source, command, target)                     \
    TEST_F(StandardTest, name)                                          \
    {                                                                   \
        pBuffer->SetText(source);                                       \
        PARSE_COMMAND(command)                                          \
        ASSERT_STREQ(pBuffer->workingBuffer.string().c_str(), target); \
    };

#define CURSOR_TEST(name, source, command, xcoord, ycoord) \
    TEST_F(StandardTest, name)                             \
    {                                                      \
        pBuffer->SetText(source);                          \
        PARSE_COMMAND(command);                            \
        ASSERT_EQ(pWindow->BufferToDisplay().x, xcoord);   \
        ASSERT_EQ(pWindow->BufferToDisplay().y, ycoord);   \
    }

#define VISUAL_TEST(name, source, command, start, end)                      \
    TEST_F(StandardTest, name)                                              \
    {                                                                       \
        pBuffer->SetText(source);                                           \
        PARSE_COMMAND(command)                                              \
        ASSERT_EQ(mode->GetInclusiveVisualRange().first.index, start); \
        ASSERT_EQ(mode->GetInclusiveVisualRange().second.index, end);  \
    }

TEST_F(StandardTest, UndoRedo
)
{
// The issue here is that setting the text _should_ update the buffer!
pBuffer->SetText("Hello");
mode->AddCommandText(" ");
mode->
Undo();
mode->
Redo();
mode->
Undo();
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"Hello");

mode->AddCommandText("iYo, ");
mode->
Undo();
mode->
Redo();
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"iYo, Hello");
}

TEST_F(StandardTest, copy_pasteover_paste
)
{
// The issue here is that setting the text _should_ update the buffer!
pBuffer->SetText("Hello Goodbye");
mode->
AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift
);
mode->
AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift
);
mode->
AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift
);
mode->
AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift
);
mode->
AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift
);
mode->AddKeyPress('c', ModifierKey::Ctrl);

mode->AddKeyPress('v', ModifierKey::Ctrl);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"Hello Goodbye");

// Note this is incorrect for what we expect, but a side effect of the test: Fix it.
// The actual behavior in the editor is correct!
mode->AddKeyPress('v', ModifierKey::Ctrl);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"HelloHello Goodbye");

ASSERT_EQ(pWindow
->
GetBufferCursor()
.
Index(),
10);
}

TEST_F(StandardTest, BackToInsertIfShiftReleased
)
{
// The issue here is that setting the text _should_ update the buffer!
pBuffer->SetText("abc");
mode->
AddKeyPress(ExtKeys::RIGHT, ModifierKey::Shift
);
ASSERT_EQ(mode
->currentMode, EditorMode::Visual
);
mode->
AddKeyPress(ExtKeys::RIGHT);
ASSERT_EQ(mode
->currentMode, EditorMode::Insert
);
}
TEST_F(StandardTest, down_a_shorter_line
)
{
// The issue here is that setting the text _should_ update the buffer!
pBuffer->SetText("Hello Goodbye\nF");
mode->
AddKeyPress(ExtKeys::RIGHT);
mode->
AddKeyPress(ExtKeys::RIGHT);
mode->
AddKeyPress(ExtKeys::RIGHT);
mode->
AddKeyPress(ExtKeys::RIGHT);
mode->
AddKeyPress(ExtKeys::DOWN);
mode->AddKeyPress('o');
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"Hello Goodbye\nFo");
}

TEST_F(StandardTest, DELETE
)
{
pBuffer->SetText("Hello");
mode->
AddKeyPress(ExtKeys::DEL);
mode->
AddKeyPress(ExtKeys::DEL);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"llo");

mode->AddCommandText("vll");
mode->
AddKeyPress(ExtKeys::DEL);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"vlllo");

// Doesn't delete H because the cursor was previously at the end?
// Is this a behavior expectation or a bug?  Should the cursor clamp to the previously
// set text end, or reset to 0??
pBuffer->SetText("H");
mode->
AddKeyPress(ExtKeys::DEL);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"H");

mode->
AddKeyPress(ExtKeys::BACKSPACE);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"");
}

TEST_F(StandardTest, BACKSPACE
)
{
pBuffer->SetText("Hello");
mode->AddCommandText("ll");
mode->
AddKeyPress(ExtKeys::BACKSPACE);
mode->
AddKeyPress(ExtKeys::BACKSPACE);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"Hello");
ASSERT_EQ(pWindow
->
GetBufferCursor()
.
Index(),
0);

mode->AddCommandText("lli");
mode->
AddKeyPress(ExtKeys::BACKSPACE);
ASSERT_STREQ(pBuffer
->
workingBuffer
.
string()
.
c_str(),
"llHello");
}

CURSOR_TEST(motion_right, "one two", "%r", 1, 0);
CURSOR_TEST(motion_left, "one two", "%r%r%l", 1, 0);
CURSOR_TEST(motion_left_over_newline, "one\ntwo", "%d%r%r%l%l%l", 3, 0);
CURSOR_TEST(motion_right_over_newline, "one\ntwo", "%r%r%r%r%r", 1, 1);
CURSOR_TEST(motion_down, "one\ntwo", "%d", 0, 1);
CURSOR_TEST(motion_up, "one\ntwo", "%d%u", 0, 0);

// NOTE: Cursor lands on the character after the shift select - i.e. the next 'Word'
// These are CTRL+-> -< movements, tested for comparison with notepad behavior.
CURSOR_TEST(motion_right_word, "one two", "%c%r", 4, 0);
CURSOR_TEST(motion_right_twice_word, "one two", "%c%r%c%r", 7, 0);
CURSOR_TEST(motion_right_twice_back_word, "one two", "%c%r%c%r%c%l", 4, 0);
CURSOR_TEST(motion_left_word, "one two", "%r%r%r%r%c%l", 0, 0);
CURSOR_TEST(motion_right_newline, "one\ntwo", "%c%r", 3, 0);
CURSOR_TEST(motion_right_newline_twice, "one\ntwo", "%c%r%c%r", 0, 1);
CURSOR_TEST(motion_right_newline_twice_back, "one\ntwo", "%c%r%c%r%c%l", 3, 0);
CURSOR_TEST(motion_right_newline_twice_back_back, "one\ntwo", "%c%r%c%r%c%l%c%l", 0, 0);

CURSOR_TEST(paste_over_cursor_after, "one", "%c%s%r%cc%cv", 3, 0);

// Visual Range selection
VISUAL_TEST(visual_shift_right, "one two", "%c%s%r", 0, 3);
VISUAL_TEST(visual_shift_right_right, "one two three", "%c%s%r%c%s%r", 0, 7);
VISUAL_TEST(visual_shift_right_right_back, "one two three", "%c%s%r%c%s%r%c%s%l", 0, 3);

COMMAND_TEST(paste_over, "one", "%s%r%s%r%s%r%cc%cv", "one");
COMMAND_TEST(paste_over_paste, "one", "%s%r%s%r%s%r%cc%cv%cv", "oneone");
COMMAND_TEST(paste_over_paste_paste_undo, "one", "%s%r%s%r%s%r%cc%cv%cv%cv%cz", "oneone");

COMMAND_TEST(delete_back_to_previous_line, "one\n\ntwo", "%d%d%x", "one\ntwo");
