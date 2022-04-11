#include "zep/mode_standard.h"

// Note:
// This is a version of the buffer that behaves like notepad.
// It is basic, but can easily be extended

// STANDARD:
//
// DONE:
// -----
// CTRLZ/Y Undo Redo
// Insert
// Delete/Backspace
// TAB
// Arrows - up,down, left, right
// Home (+Ctrl) move top/startline
// End (+Ctrl) move bottom/endline
// Shift == Select
// control+Shift == select word
// CTRL - CVX (copy paste, cut) + Delete Selection

namespace Zep {

ZepMode_Standard::ZepMode_Standard(ZepEditor &editor)
    : ZepMode(editor) {
}

ZepMode_Standard::~ZepMode_Standard() = default;

void ZepMode_Standard::Init() {
    // In standard mode, we always show the insert cursor type
    m_visualCursorType = CursorType::Insert;
    m_modeFlags |= ModeFlags::InsertModeGroupUndo | ModeFlags::StayInInsertMode;

    for (int i = 0; i <= 9; i++) {
        editor.SetRegister('0' + (const char) i, "");
    }
    editor.SetRegister('"', "");

    // Insert Mode
    keymap_add({&m_insertMap}, {"<Backspace>"}, id_Backspace);
    keymap_add({&m_insertMap}, {"<Return>"}, id_InsertCarriageReturn);
    keymap_add({&m_insertMap}, {"<Tab>"}, id_InsertTab);
    keymap_add({&m_insertMap, &m_visualMap}, {"<Del>"}, id_Delete);
    keymap_add({&m_insertMap, &m_visualMap}, {"<C-y>"}, id_Redo);
    keymap_add({&m_insertMap, &m_visualMap}, {"<C-z>"}, id_Undo);

    keymap_add({&m_insertMap, &m_visualMap}, {"<Left>"}, id_MotionStandardLeft);
    keymap_add({&m_insertMap, &m_visualMap}, {"<Right>"}, id_MotionStandardRight);
    keymap_add({&m_insertMap, &m_visualMap}, {"<Up>"}, id_MotionStandardUp);
    keymap_add({&m_insertMap, &m_visualMap}, {"<Down>"}, id_MotionStandardDown);
    keymap_add({&m_insertMap}, {"<End>"}, id_MotionLineBeyondEnd);
    keymap_add({&m_insertMap}, {"<Home>"}, id_MotionLineHomeToggle);
    keymap_add({&m_insertMap}, {"<C-Left>"}, id_MotionStandardLeftWord);
    keymap_add({&m_insertMap}, {"<C-Right>"}, id_MotionStandardRightWord);

    keymap_add({&m_insertMap, &m_visualMap}, {"<C-S-Left>"}, id_MotionStandardLeftWordSelect);
    keymap_add({&m_insertMap, &m_visualMap}, {"<C-S-Right>"}, id_MotionStandardRightWordSelect);

    keymap_add({&m_insertMap, &m_visualMap}, {"<S-Left>"}, id_MotionStandardLeftSelect);
    keymap_add({&m_insertMap, &m_visualMap}, {"<S-Right>"}, id_MotionStandardRightSelect);
    keymap_add({&m_insertMap, &m_visualMap}, {"<S-Up>"}, id_MotionStandardUpSelect);
    keymap_add({&m_insertMap, &m_visualMap}, {"<S-Down>"}, id_MotionStandardDownSelect);

    keymap_add({&m_visualMap}, {"<C-x>"}, id_Delete);
    keymap_add({&m_visualMap}, {"<Backspace>"}, id_Delete);

    keymap_add({&m_insertMap, &m_visualMap}, {"<C-v>"}, id_StandardPaste);
    keymap_add({&m_visualMap}, {"<C-c>"}, id_StandardCopy);

    keymap_add({&m_insertMap, &m_visualMap}, {"<C-a>"}, id_StandardSelectAll);

    keymap_add({&m_normalMap, &m_visualMap, &m_insertMap}, {"<Escape>"}, id_InsertMode);
    keymap_add({&m_normalMap}, {"<Backspace>"}, id_MotionStandardLeft);
}

void ZepMode_Standard::Begin(ZepWindow *pWindow) {
    ZepMode::Begin(pWindow);
    SwitchMode(EditorMode::Insert); // This will also set the cursor type
}

} // namespace Zep
