#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <regex>

#include "zep/buffer.h"
#include "zep/editor.h"
#include "zep/filesystem.h"

#include "zep/path.h"
#include "zep/stringutils.h"

namespace Zep {

namespace {

// A VIM-like definition of a word.  Actually, in Vim this can be changed, but this editor
// assumes a word is alphanumeric or underscore for consistency
inline bool IsWordChar(const char c) {
    auto ch = ToASCII(c);
    return std::isalnum(ch) || ch == '_';
}
inline bool IsWordOrSepChar(const char c) {
    auto ch = ToASCII(c);
    return std::isalnum(ch) || ch == '_' || ch == ' ' || ch == '\n' || ch == 0;
}
inline bool IsWORDChar(const char c) {
    auto ch = ToASCII(c);
    return std::isgraph(ch);
}
inline bool IsWORDOrSepChar(const char c) {
    auto ch = ToASCII(c);
    return std::isgraph(ch) || ch == ' ' || ch == '\n' || ch == 0;
}
inline bool IsSpace(const char c) {
    auto ch = ToASCII(c);
    return ch == ' ';
}
inline bool IsSpaceOrNewline(const char c) {
    auto ch = ToASCII(c);
    return ch == ' ' || ch == '\n';
}
inline bool IsSpaceOrTerminal(const char c) {
    auto ch = ToASCII(c);
    return ch == ' ' || ch == 0 || ch == '\n';
}

} // namespace

ZepBuffer::ZepBuffer(ZepEditor &editor, std::string strName) : ZepComponent(editor), name(std::move(strName)) {
    Clear();
}

ZepBuffer::ZepBuffer(ZepEditor &editor, const ZepPath &path) : ZepComponent(editor) {
    Load(path);
}

void ZepBuffer::Notify(const std::shared_ptr<ZepMessage> &message) {}

// Vertical column
long ZepBuffer::GetBufferColumn(const GlyphIterator &location) const {
    auto lineStart = GetLinePos(location, LineLocation::LineBegin);
    return CodePointDistance(lineStart, location);
}

// Find the location inside the list of line ends
long ZepBuffer::GetBufferLine(const GlyphIterator &location) const {
    auto itrLine = std::lower_bound(lineEnds.begin(), lineEnds.end(), location.index);
    if (itrLine != lineEnds.end() && location.index >= *itrLine) {
        itrLine++;
    }
    long line = std::min(std::max(0l, long(itrLine - lineEnds.begin())), long(lineEnds.size()) - 1);
    return line;
}

// Prepare for a motion
void ZepBuffer::MotionBegin(GlyphIterator &start) {
    GlyphIterator newStart = start;

    // Clamp to sensible, begin
    newStart.Clamp();

    bool change = newStart != start;
    if (change) {
        start = newStart;
    }
}

bool ZepBuffer::Move(GlyphIterator &loc, Direction dir) const {
    if ((dir == Direction::Backward && loc.index) == 0 || (dir == Direction::Forward && loc.index == End().index)) {
        return false;
    }

    if (dir == Direction::Backward) loc--;
    else loc++;
    return true;
}

bool ZepBuffer::Skip(const fnMatch &IsToken, GlyphIterator &start, Direction dir) const {
    if (!start.Valid()) return false;

    bool found = false;
    while (IsToken(start.Char())) {
        found = true;
        if (!Move(start, dir)) break;
    }
    return found;
}

bool ZepBuffer::SkipOne(const fnMatch &IsToken, GlyphIterator &start, Direction dir) const {
    if (!start.Valid()) return false;

    bool found = false;
    if (IsToken(start.Char())) {
        found = true;
        Move(start, dir);
    }
    return found;
}

bool ZepBuffer::SkipNot(const fnMatch &IsToken, GlyphIterator &start, Direction dir) const {
    if (!start.Valid()) return false;

    bool found = false;
    while (!IsToken(start.Char())) {
        found = true;
        if (!Move(start, dir)) break;
    }
    return found;
}

// This is the vim-like 'caw' rule; The motions and behaviour are based on how vim behaves.
// This is still quite complex behavior for this particular motion.  I'm open to better ways to express it!
GlyphRange ZepBuffer::AWordMotion(GlyphIterator start, uint32_t searchType) const {
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    GlyphRange r;
    r.first = start;

    MotionBegin(start);

    // Already on a word; find the limits, and include the space
    if (Skip(IsWord, start, Direction::Backward)) {
        SkipNot(IsWord, start, Direction::Forward);
        r.first = start;
        Skip(IsWord, start, Direction::Forward);
        Skip(IsSpace, start, Direction::Forward);
        r.second = start;
    }
        // ... or skip space
    else if (Skip(IsSpace, start, Direction::Forward)) {
        Skip(IsWord, start, Direction::Forward);
        r.second = start;
    }
        // On a non-word, find the beginning, remove including following spaces
    else if (SkipNot(IsWord, start, Direction::Backward)) {
        Skip(IsSpace, start, Direction::Forward);
        Skip(IsWord, start, Direction::Forward);
        r.first = start;
        SkipNot(IsWord, start, Direction::Forward);
        Skip(IsSpace, start, Direction::Forward);
        r.second = start;
    }

    return r;
}

// Implements the ctrl + motion of a standard editor.
// This is a little convoluted; perhaps the logic can be simpler!
// Playing around with CTRL+ arrows and shift in an app like notepad will teach you that the rules for how far to jump
// depend on what you are over, and which direction you are going.....
// The unit tests are designed to enforce the behavior here
GlyphRange ZepBuffer::StandardCtrlMotion(GlyphIterator cursor, Direction searchDir) const {
    MotionBegin(cursor);

    auto lineEnd = GetLinePos(cursor, LineLocation::LineCRBegin);
    auto current = std::min(lineEnd, cursor.Clamp());

    GlyphRange r;
    r.first = current;
    r.second = current;

    if (searchDir == Direction::Forward) {
        // TODO word-forward not working
        if (Skip(IsWORDChar, current, searchDir)) {
            // Skip space
            Skip(IsSpace, current, searchDir);
        } else {
            SkipNot(IsWORDChar, current, searchDir);
        }
    } else {
        // Always skip back a char (iterator will clamp)
        current--;

        // Stop on the newline, or continue
        if (current.Char() != '\n') {
            SkipNot(IsWORDChar, current, searchDir);
            Skip(IsWORDChar, current, searchDir);
            SkipNot(IsWORDChar, current, Direction::Forward);
        }
    }
    r.second = current.Clamp();

    return r;
}

// Note:
// In general I'm unhappy with these word motion functions.  They are _hard_ ; especially if you
// want them to conform to the quirks of Vim.  I haven't found a cleaner way than this to make them
// work.
GlyphRange ZepBuffer::InnerWordMotion(GlyphIterator start, uint32_t searchType) const {
    auto IsWordOrSpace = searchType == SearchType::Word ? IsWordOrSepChar : IsWORDOrSepChar;
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;
    MotionBegin(start);

    GlyphRange r;

    // Special case; change inner word on a newline, stay put, don't delete anything
    if (start.Char() == '\n') {
        r.first = r.second = start;
    } else if (SkipNot(IsWordOrSpace, start, Direction::Forward)) {
        r.second = start;
        start--;
        SkipNot(IsWordOrSpace, start, Direction::Backward);
        Skip(IsWordOrSpace, start, Direction::Forward);
        r.first = start;
    } else if (Skip(IsSpace, start, Direction::Forward)) {
        r.second = start;
        start--;
        Skip(IsSpace, start, Direction::Backward);
        SkipNot(IsSpace, start, Direction::Forward);
        r.first = start;
    } else {
        Skip(IsWord, start, Direction::Forward);
        r.second = start;
        start--;
        Skip(IsWord, start, Direction::Backward);
        SkipNot(IsWord, start, Direction::Forward);
        r.first = start;
    }
    return r;
}

GlyphIterator ZepBuffer::Find(const GlyphIterator &start, const uint8_t *pBeginString, const uint8_t *pEndString) const {
    // Should be a valid start
    assert(start.Valid());
    assert(pBeginString != nullptr);

    if (!start.Valid() && pBeginString) return start;

    // Find the end of the test string
    if (pEndString == nullptr) {
        pEndString = pBeginString;
        while (*pEndString != 0) pEndString++;
    }

    auto itrBuffer = start;
    auto itrEnd = End();
    while (itrBuffer != itrEnd) {
        auto itrNext = itrBuffer;

        // Loop the string and match it to the buffer
        auto pCurrent = pBeginString;
        while (pCurrent != pEndString && itrNext != itrEnd) {
            if (*pCurrent != itrNext.Char()) break;
            pCurrent++;
            itrNext++;
        };

        // We successfully got to the end
        if (pCurrent == pEndString) return itrBuffer;

        itrBuffer++;
    };

    return GlyphIterator();
}

GlyphIterator ZepBuffer::FindOnLineMotion(GlyphIterator start, const uint8_t *pCh, Direction dir) const {
    auto entry = start;
    auto IsMatch = [pCh](const char ch) { return *pCh == ch; };
    auto NotMatchNotEnd = [pCh](const char ch) { return *pCh != ch && ch != '\n'; };

    // if (dir == Direction::Forward) ?
    // Ignore char under cursor, as behavior
    SkipOne(IsMatch, start, dir);
    // Find to the end of the line
    Skip(NotMatchNotEnd, start, dir);

    if (start.Valid() && *pCh == start.Char()) return start;
    return entry;
}

std::pair<GlyphIterator, GlyphIterator> ZepBuffer::FindMatchingPair(GlyphIterator start, const uint8_t ch) const {
    std::string delims;
    switch (ch) {
        case '(':
        case ')':delims = "()";
            break;
        case '[':
        case ']':delims = "[]";
            break;
        case '{':
        case '}':delims = "{}";
            break;
        default:
            // Matching same char at both ends
            delims = std::string(2, ch);
            break;
    }

    std::pair<GlyphIterator, GlyphIterator> ret;
    Direction dir = Direction::Backward;

    auto search = [&](GlyphIterator loc, Direction dir) {
        int openCount = 1;
        for (;;) {
            // Find the previous open for the current delim type
            int newIndex;
            loc = FindFirstCharOf(loc, delims, newIndex, dir);

            // Fell off beginning, no find
            if (newIndex < 0) return GlyphIterator();

            if (dir == Direction::Forward) newIndex = 1 - newIndex;

            // Match immediately for ""
            if (delims[0] == delims[1]) newIndex = 0;

            // Found another opener
            if (newIndex == 0) {
                openCount--;
                if (openCount == 0) return loc;
            } else if (newIndex == 1) {
                // Found a closer
                openCount++;
            }

            if (dir == Direction::Forward) {
                if (loc == End()) return GlyphIterator();
                loc++;
            } else {
                if (loc == Begin()) return GlyphIterator();
                loc--;
            }
        }
    };

    // If on the end bracket, start before it.
    if (start.Char() == delims[1] && delims[0] != delims[1]) {
        start--;
    }

    // Search for the begin
    ret.first = search(start, Direction::Backward);
    if (ret.first.Valid() && ret.first != End()) {
        // Search for the end
        ret.second = search(ret.first + 1, Direction::Forward);
    }
    return ret;
}

// Only works on searches of ascii characters (but navigates unicode buffer); useful for some vim operations
// Returns the index of the first found char and its location
GlyphIterator ZepBuffer::FindFirstCharOf(GlyphIterator &start, const std::string &chars, int32_t &found_index, Direction dir) const {
    GlyphIterator itr = start;
    if (!itr.Valid()) {
        found_index = -1;
        return itr;
    }

    for (;;) {
        for (int i = 0; i < chars.length(); i++) {
            if (itr.Char() == chars[i]) {
                found_index = i;
                return itr;
            }
        }

        if (dir == Direction::Forward) {
            if (itr == End()) break;
            itr++;
        } else if (dir == Direction::Backward) {
            if (itr == Begin()) break;
            itr--;
        }
    }
    found_index = -1;
    return itr;
}

GlyphIterator ZepBuffer::WordMotion(GlyphIterator start, uint32_t searchType, Direction dir) const {
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    MotionBegin(start);

    if (dir == Direction::Forward) {
        if (Skip(IsWord, start, dir)) Skip(IsSpaceOrTerminal, start, dir); // Skipped a word, skip spaces then done
        else SkipNot(IsWord, start, dir);
    } else { // Backward
        auto startSearch = start;

        // Jump back to the beginning of a word if on it
        if (Skip(IsWord, start, dir)) {
            // If we weren't already on the first char of the word, then we have gone back a word!
            if (startSearch != (start + 1)) {
                SkipNot(IsWord, start, Direction::Forward);
                return start;
            }
        } else {
            SkipNot(IsWord, start, dir);
        }

        // Skip any spaces
        Skip(IsSpace, start, dir);

        // Go back to the beginning of the word
        if (Skip(IsWord, start, dir)) {
            SkipNot(IsWord, start, Direction::Forward);
        }
    }
    return start;
}

GlyphIterator ZepBuffer::EndWordMotion(GlyphIterator start, uint32_t searchType, Direction dir) const {
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    MotionBegin(start);

    if (dir == Direction::Forward) {
        auto startSearch = start;

        // Skip to the end
        if (Skip(IsWord, start, dir)) {
            // We moved a bit, so we found the end of the current word
            if (startSearch != start - 1) {
                SkipNot(IsWord, start, Direction::Backward);
                return start;
            }
        } else {
            SkipNot(IsWord, start, dir);
        }

        // Skip any spaces
        Skip(IsSpaceOrNewline, start, dir);

        // Go back to the beginning of the word
        if (Skip(IsWord, start, dir)) {
            SkipNot(IsWord, start, Direction::Backward);
        }
    } else {
        // Backward
        // Note this is the same as the Next word code, in 'forward' mode
        if (Skip(IsWord, start, dir)) Skip(IsSpace, start, dir); // Skipped a word, skip spaces then done
        else SkipNot(IsWord, start, dir);
    }
    return start;
}

GlyphIterator ZepBuffer::ChangeWordMotion(GlyphIterator start, uint32_t searchType, Direction dir) const {
    // Change word is different to work skipping; it will change a string of spaces, for example.
    // Essentially it changes 'what you are over', based on the word rule
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;
    MotionBegin(start);
    if (Skip(IsWord, start, dir)) return start;
    SkipNot(IsWord, start, dir);
    return start;
}

GlyphIterator ZepBuffer::ClampToVisibleLine(GlyphIterator in) const {
    in = in.Clamp();
    auto loc = GetLinePos(in, LineLocation::LineLastNonCR);
    in = std::min(loc, in);
    return in;
}

// Method for querying the beginning and end of a line
bool ZepBuffer::GetLineOffsets(const long line, ByteRange &range) const {
    // Not valid
    if (long(lineEnds.size()) <= line) {
        range.first = 0;
        range.second = 0;
        return false;
    }

    // Find the line bounds - we know the end, find the start from the previous
    range.second = lineEnds[line];
    range.first = line == 0 ? 0 : lineEnds[line - 1];
    return true;
}

std::string ZepBuffer::GetFileExtension() const {
    std::string ext;
    if (filePath.has_filename() && filePath.filename().has_extension()) {
        ext = string_tolower(filePath.filename().extension().string());
    } else {
        size_t dot_pos = name.find_last_of('.');
        if (dot_pos != std::string::npos) {
            ext = string_tolower(name.substr(dot_pos, name.length() - dot_pos));
        }
    }
    return ext;
}

// Basic load support; read a file if it's present, but keep
// the file path in case you want to write later
void ZepBuffer::Load(const ZepPath &path) {
    // Set the name from the path
    name = path.has_filename() ? path.filename().string() : filePath.string();

    // Must set the syntax before the first buffer change messages
    // TODO: I believe that some of this buffer config should move to Editor.cpp
    editor.SetBufferSyntax(*this);

    if (Zep::ZepFileSystem::Exists(path)) {
        filePath = Zep::ZepFileSystem::Canonical(path);
        auto read = Zep::ZepFileSystem::Read(path);

        // Always set text, to ensure we prepare the buffer with 0 terminator,
        // even if string is empty
        SetText(read, true);
    } else {
        // Can't canonicalize a non-existent path.
        // But we may have a path we haven't save to yet!
        Clear();
        filePath = path;
    }
}

bool ZepBuffer::Save(int64_t &size) {
    if ((fileFlags & FileFlags::Locked) || (fileFlags & FileFlags::ReadOnly)) return false;

    auto str = workingBuffer.string();

    // Put back /r/n if necessary while writing the file
    // At the moment, Zep removes /r/n and just uses /n while modifying text.
    // It replaces the /r on files that had it afterwards
    // Alternatively we could manage them 'in place', but that would make parsing more complex.
    // And then what do you do if there are 2 different styles in the file.
    if (fileFlags & FileFlags::StrippedCR) {
        // TODO: faster way to replace newlines
        string_replace_in_place(str, "\n", "\r\n");
    }

    // Remove the appended 0 if necessary
    size = (int64_t) str.size();
    if (fileFlags & FileFlags::TerminatedWithZero) size--;

    if (size <= 0) return true;

    if (Zep::ZepFileSystem::Write(filePath, &str[0], (size_t) size)) {
        fileFlags = ZClearFlags(fileFlags, FileFlags::Dirty);
        if (Zep::ZepFileSystem::Exists(filePath)) {
            filePath = Zep::ZepFileSystem::Canonical(filePath);
        }
        return true;
    }

    return false;
}

std::string ZepBuffer::GetDisplayName() const {
    return filePath.empty() ? name : filePath.string();
}

void ZepBuffer::SetFilePath(const ZepPath &path) {
    auto testPath = path;
    if (Zep::ZepFileSystem::Exists(testPath)) {
        testPath = Zep::ZepFileSystem::Canonical(testPath);
    } else {
        auto full = editor.fileSystem->workingDirectory / path;
        testPath = Zep::ZepFileSystem::Exists(full) ? Zep::ZepFileSystem::Canonical(full) : full;
    }
    if (!Zep::ZepFileSystem::Equivalent(testPath, filePath)) {
        filePath = testPath;
    }
    editor.SetBufferSyntax(*this);
}

// Remember that we updated the buffer and dirty the state
// Clients can use these values to figure out update times and dirty state
void ZepBuffer::MarkUpdate() {
    updateCount++;
    fileFlags = ZSetFlags(fileFlags, FileFlags::Dirty);
}

// Clear this buffer.  If it was previously not clear, it has been updated.
// Otherwise it is just reset to default state.  A new buffer is always initially cleared.
void ZepBuffer::Clear() {
    // A buffer that is empty is brand new; just make it 0 chars and return
    if (workingBuffer.size() <= 1) {
        workingBuffer.clear();
        workingBuffer.push_back(0);
        lineEnds.clear();
        fileFlags = ZSetFlags(fileFlags, FileFlags::TerminatedWithZero);
        lineEnds.push_back(End().index + 1);
        return;
    }

    // Inform clients we are about to change the buffer
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, GlyphIterator(this), End()));

    workingBuffer.clear();
    workingBuffer.push_back(0);
    lineEnds.clear();
    fileFlags = ZSetFlags(fileFlags, FileFlags::TerminatedWithZero);
    lineEnds.push_back(End().index + 1);

    {
        MarkUpdate();
        editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextDeleted, GlyphIterator(this), End()));
    }
}

// Replace the buffer with the text
void ZepBuffer::SetText(const std::string &text, bool initFromFile) {
    // First, clear it
    Clear();

    bool lastWasSpace = false;
    if (!text.empty()) {
        // Since incremental insertion of a big file into a gap buffer gives us worst case performance,
        // We build the buffer in a separate array and assign it.  Much faster.
        std::vector<uint8_t> input;

        lineEnds.clear();

        // Update the gap buffer with the text
        // We remove \r, we only care about \n
        for (auto &ch: text) {
            if (ch == '\r') {
                fileFlags |= FileFlags::StrippedCR;
            } else {
                input.push_back(ch);
                if (ch == '\n') {
                    lineEnds.push_back(ByteIndex(input.size()));
                    lastWasSpace = false;
                } else if (ch == '\t') {
                    fileFlags |= FileFlags::HasTabs;
                    lastWasSpace = false;
                } else if (ch == ' ') {
                    if (lastWasSpace) fileFlags |= FileFlags::HasSpaceTabs;
                    lastWasSpace = true;
                } else {
                    lastWasSpace = false;
                }
            }
        }
        workingBuffer.assign(input.begin(), input.end());
    }

    // If file is only tabs, then force tab mode
    if (HasFileFlags(FileFlags::HasTabs) && !HasFileFlags(FileFlags::HasSpaceTabs)) {
        fileFlags = ZSetFlags(fileFlags, FileFlags::InsertTabs);
    }

    if (workingBuffer[workingBuffer.size() - 1] != 0) {
        fileFlags |= FileFlags::TerminatedWithZero;
        workingBuffer.push_back(0);
    }

    // TODO: Why is a line end needed always?
    // TODO: Line ends 1 beyond, or just for end?  Can't remember this detail:
    // understand it, then write a unit test to ensure it.
    lineEnds.push_back(End().index + 1);

    MarkUpdate();

    // When loading a file, send the Loaded message to distinguish it from adding to a buffer, and remember that the buffer is not dirty in this case
    if (initFromFile) {
        editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::Loaded, Begin(), End()));

        // Doc is not dirty
        fileFlags = ZClearFlags(fileFlags, FileFlags::Dirty);
    } else {
        editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextAdded, Begin(), End()));
    }
}

// TODO: This can be cleaner
// The function needs to find the point on the line which bufferLocation is on.
// It needs to account for empty lines or the last line, zero terminated.
// It shouldn't walk away to another line!
GlyphIterator ZepBuffer::GetLinePos(GlyphIterator bufferLocation, LineLocation lineLocation) const {
    if (lineLocation == LineLocation::None) {
        assert(!"Invalid");
        return GlyphIterator();
    }

    bufferLocation.Clamp();
    if (workingBuffer.empty()) return bufferLocation;

    GlyphIterator itr = bufferLocation;
    GlyphIterator itrBegin = Begin();
    GlyphIterator itrEnd = End();

    GlyphIterator itrLineStart(itr);

    // If we are on the CR, move back 1, unless the \n is all that is on the line
    if (itrLineStart != itrBegin) {
        if (itrLineStart.Char() == '\n') itrLineStart.Move(-1);

        // Find the end of the previous line
        while (itrLineStart > itrBegin && itrLineStart.Char() != '\n') {
            itrLineStart.Move(-1);
        }

        if (itrLineStart.Char() == '\n') itrLineStart.Move(1);

        itr = itrLineStart;
    }

    switch (lineLocation) {
        default:
            // We are on the first bit of the line anyway
        case LineLocation::LineBegin: {
            return itr.Clamped();
        }
            break;
            // The point just after the line end
        case LineLocation::BeyondLineEnd: {
            while (itr < itrEnd && itr.Char() != '\n' && itr.Char() != 0) {
                itr.Move(1);
            }
            itr.Move(1);
            return itr.Clamped();
        }
            break;

        case LineLocation::LineCRBegin: {
            while (itr < itrEnd
                && itr.Char() != '\n'
                && itr.Char() != 0) {
                itr.Move(1);
            }
            return itr;
        }
            break;

        case LineLocation::LineFirstGraphChar: {
            while (itr < itrEnd && !std::isgraph(ToASCII(itr.Char())) && itr.Char() != '\n') {
                itr.Move(1);
            }
            return itr.Clamped();
        }
            break;

        case LineLocation::LineLastNonCR: {
            auto itrFirst = itr;

            while (itr < itrEnd
                && itr.Char() != '\n'
                && itr.Char() != 0) {
                itr.Move(1);
            }

            if (itrFirst != itr) {
                itr.Move(-1);
            }

            return itr.Clamped();
        }
            break;

        case LineLocation::LineLastGraphChar: {
            while (itr < itrEnd
                && itr.Char() != '\n'
                && itr.Char() != 0) {
                itr.Move(1);
            }

            while (itr > itrBegin && itr < itrEnd
                && !std::isgraph(ToASCII(itr.Char()))) {
                itr.Move(-1);
            }

            if (itr < itrLineStart) {
                itr = itrLineStart;
            }

            return itr.Clamped();
        }
            break;
    }
}

std::string ZepBuffer::GetBufferText(const GlyphIterator &start, const GlyphIterator &end) const {
    return {workingBuffer.begin() + start.index, workingBuffer.begin() + end.index};
}

bool ZepBuffer::Insert(const GlyphIterator &startIndex, const std::string &str, ChangeRecord &changeRecord) {
    if (!startIndex.Valid()) return false;

    GlyphIterator endIndex(this, startIndex.index + long(str.length()));

    sigPreInsert(*this, startIndex, str);

    // We are about to modify this range
    // TODO: Is this correct, and/or useful in any way??
    // We aren't changing this range at all; we are shifting those characters forward and replacing the area
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startIndex, endIndex));

    // abcdef\r\nabc<insert>dfdf\r\n
    auto itrLine = std::lower_bound(lineEnds.begin(), lineEnds.end(), startIndex.index);;
    if (itrLine != lineEnds.end() && *itrLine <= startIndex.index) {
        itrLine++;
    }

    auto itrEnd = str.end();
    auto itrBegin = str.begin();
    auto itr = str.begin();

    // Make a list of lines to 'insert'
    std::vector<long> lines;
    std::string lineEndSymbols("\n");
    while (itr != itrEnd) {
        // Get to first point after "\n"
        // That's the point just after the end of the current line
        itr = std::find_first_of(itr, itrEnd, lineEndSymbols.begin(), lineEndSymbols.end());
        if (itr != itrEnd) {
            if (itr != itrEnd && *itr == '\n') itr++;
            lines.push_back(long(itr - itrBegin) + startIndex.index);
        }
    }

    // Increment the rest of the line ends
    // We make all the remaning line ends bigger by the fixed_size of the insertion
    auto itrAdd = itrLine;
    while (itrAdd != lineEnds.end()) {
        *itrAdd += long(str.length());
        itrAdd++;
    }

    if (!lines.empty()) {
        // Update the atomic line counter so clients can see where we are up to.
        lineEnds.insert(itrLine, lines.begin(), lines.end());
    }

    changeRecord.strInserted = str;
    workingBuffer.insert(workingBuffer.begin() + startIndex.index, str.begin(), str.end());

    MarkUpdate();

    // This is the range we added (not valid any more in the buffer)
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextAdded, startIndex, endIndex));

    return true;
}

bool ZepBuffer::Replace(const GlyphIterator &startIndex, const GlyphIterator &endIndex, std::string str, ReplaceRangeMode replaceRangeMode, ChangeRecord &changeRecord) {
    if (!startIndex.Valid() || !endIndex.Valid()) return false;

    if (replaceRangeMode == ReplaceRangeMode::Replace) {
        // A replace is really 2 steps; remove the current, insert the new
        Delete(startIndex, endIndex, changeRecord);

        ChangeRecord tempRecord;
        Insert(startIndex, str, tempRecord);
        return true;
    }

    // This is what we effectively delete when we do the replace
    changeRecord.strDeleted = GetBufferText(startIndex, endIndex);

    // We are about to modify this range
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startIndex, endIndex));

    // Perform a fill
    for (auto loc = startIndex; loc < endIndex; loc++) {
        // Note we don't support utf8 yet
        // TODO: (0) Broken now we support utf8
        workingBuffer[loc.index] = str[0];
    }

    MarkUpdate();

    // This is the range we added (not valid any more in the buffer)
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextChanged, startIndex, endIndex));

    return true;
}

// A fundamental operation - delete a range of characters
// Need to update:
// - m_lineEnds
// - m_processedLine
// - m_pBuffer (i.e remove chars)
// We also need to inform clients before we change the buffer, and after we delete text with the range we removed.
// This helps them to fix up their data structures without rebuilding.
// Assumption: The buffer always is at least a single line/character of '0', representing file end.
// This makes a few things fall out more easily
bool ZepBuffer::Delete(const GlyphIterator &startIndex, const GlyphIterator &endIndex, ChangeRecord &changeRecord) {
    assert(startIndex.Valid());
    assert(endIndex.Valid());

    // We are about to modify this range
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startIndex, endIndex));

    changeRecord.strDeleted = GetBufferText(startIndex, endIndex);

    sigPreDelete(*this, startIndex, endIndex);

    auto itrLine = std::lower_bound(lineEnds.begin(), lineEnds.end(), startIndex.index);
    if (itrLine == lineEnds.end()) return false;

    auto itrLastLine = std::upper_bound(itrLine, lineEnds.end(), endIndex.index);
    auto offsetDiff = endIndex.index - startIndex.index;

    if (*itrLine <= startIndex.index) {
        itrLine++;
    }

    // Adjust all line offsets beyond us
    for (auto itr = itrLastLine; itr != lineEnds.end(); itr++) {
        *itr -= offsetDiff;
    }

    if (itrLine != itrLastLine) {
        lineEnds.erase(itrLine, itrLastLine);
    }

    workingBuffer.erase(workingBuffer.begin() + startIndex.index, workingBuffer.begin() + endIndex.index);
    assert(!workingBuffer.empty() && workingBuffer[workingBuffer.size() - 1] == 0);

    MarkUpdate();

    // This is the range we deleted (not valid any more in the buffer)
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextDeleted, startIndex, endIndex));

    return true;
}

ZepTheme &ZepBuffer::GetTheme() const { return *editor.theme; }

bool ZepBuffer::HasSelection() const { return selection.first != selection.second; }

void ZepBuffer::ClearSelection() {
    selection.first = Begin();
    selection.second = Begin();
}

void ZepBuffer::SetSelection(const GlyphRange &sel) {
    selection = sel;
    if (selection.first > selection.second) {
        std::swap(selection.first, selection.second);
    }
}

void ZepBuffer::AddRangeMarker(const std::shared_ptr<RangeMarker> &marker) {
    rangeMarkers[marker->range.first].insert(marker);

    // TODO: Why is this necessary; marks the whole buffer
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::MarkersChanged, Begin(), End()));
}

void ZepBuffer::ClearRangeMarker(const std::shared_ptr<RangeMarker> &marker) {
    auto itr = rangeMarkers.find(marker->range.first);
    if (itr != rangeMarkers.end()) {
        itr->second.erase(marker);
        if (itr->second.empty()) {
            rangeMarkers.erase(marker->range.first);
        }
    }

    // TODO: Why is this necessary; marks the whole buffer
    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::MarkersChanged, Begin(), End()));
}

void ZepBuffer::ClearRangeMarkers(uint32_t markerType) {
    std::set<std::shared_ptr<RangeMarker>> markers;
    ForEachMarker(markerType, Direction::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker> &pMarker) {
        // Timed ones will expire on their own; TODO: Add a cleaner mechanism for selection here
        if (!(pMarker->displayType & RangeMarkerDisplayType::Timed)) {
            markers.insert(pMarker);
        }
        return true;
    });

    for (auto &victim: markers) {
        ClearRangeMarker(victim);
    }

    editor.Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::MarkersChanged, Begin(), End()));
}

bool OverlapInclusive(ByteRange r1, ByteRange r2) {
    // -----aaaaa----
    // ---bbbbbbbbb-------
    return r1.first <= r2.second && r2.first <= r1.second;
}

void ZepBuffer::ForEachMarker(uint32_t markerType, Direction dir, const GlyphIterator &begin, const GlyphIterator &end, const std::function<bool(const std::shared_ptr<RangeMarker> &)> &fnCB) const {
    ByteRange inclusive = ByteRange(begin.index, end.Peek(-1).index);
    if (dir == Direction::Forward) {
        for (const auto &m_rangeMarker: rangeMarkers) {
            for (int pass = 0; pass < 2; pass++) {
                for (auto &markerItem: m_rangeMarker.second) {
                    if ((markerItem->markerType & markerType) == 0) continue;

                    // Enumerate timed markers after all others, because these are effects that should happen last
                    if (pass == 0) {
                        if (markerItem->displayType & RangeMarkerDisplayType::Timed) continue;
                    } else if (!(markerItem->displayType & RangeMarkerDisplayType::Timed)) continue;

                    auto markerInclusive = ByteRange(markerItem->range.first, std::max(0l, markerItem->range.second - 1));
                    if (!OverlapInclusive(inclusive, markerInclusive)) continue;

                    if (!fnCB(markerItem)) return;
                }
            }
        }
    } else {
        auto itrREnd = std::make_reverse_iterator(rangeMarkers.begin());
        auto itrRStart = std::make_reverse_iterator(rangeMarkers.end());

        for (auto itr = itrRStart; itr != itrREnd; itr++) {
            for (auto &markerItem: itr->second) {
                if ((markerItem->markerType & markerType) == 0) continue;
                if (!fnCB(markerItem)) return;
            }
        }
    }
}

void ZepBuffer::HideMarkers(uint32_t markerType) const {
    ForEachMarker(markerType, Direction::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker> &marker) {
        if ((marker->markerType & markerType) != 0) {
            marker->displayType = RangeMarkerDisplayType::Hidden;
        }
        return true;
    });
}

void ZepBuffer::ShowMarkers(uint32_t markerType, uint32_t displayType) const {
    ForEachMarker(markerType, Direction::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker> &marker) {
        if ((marker->markerType & markerType) != 0) {
            marker->displayType = displayType;
        }
        return true;
    });
}

tRangeMarkers ZepBuffer::GetRangeMarkers(uint32_t markerType) const {
    tRangeMarkers markers;
    ForEachMarker(markerType, Direction::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker> &marker) {
        if ((marker->markerType & markerType) != 0) {
            markers[marker->range.first].insert(marker);
        }
        return true;
    });
    return markers;
}

std::shared_ptr<RangeMarker> ZepBuffer::FindNextMarker(GlyphIterator start, Direction dir, uint32_t markerType) const {
    start.Clamp();

    std::shared_ptr<RangeMarker> found;
    auto search = [&]() {
        ForEachMarker(markerType, dir, Begin(), End(), [&](const std::shared_ptr<RangeMarker> &marker) {
            if (dir == Direction::Forward) {
                if (marker->range.first <= start.index) return true;
            } else if (marker->range.first >= start.index) return true;

            found = marker;
            return false;
        });
    };

    search();
    if (found == nullptr) {
        // Wrap
        start = (dir == Direction::Forward ? Begin() : End());
        search();
    }
    return found;
}

GlyphIterator ZepBuffer::GetLastEditLocation() {
    if (!lastEditLocation.Valid()) lastEditLocation = GlyphIterator(this, 0);
    return lastEditLocation;
}

ZepMode *ZepBuffer::GetMode() const { return mode ? mode.get() : editor.GetGlobalMode(); }

tRangeMarkers ZepBuffer::GetRangeMarkersOnLine(uint32_t markerTypes, long line) const {
    ByteRange range;
    GetLineOffsets(line, range);

    tRangeMarkers rangeMarkersOnLine;
    ForEachMarker(markerTypes,
        Direction::Forward,
        GlyphIterator(this, range.first), GlyphIterator(this, range.second),
        [&](const std::shared_ptr<RangeMarker> &marker) {
            rangeMarkersOnLine[marker->range.first].insert(marker);
            return true;
        });
    return rangeMarkersOnLine;
}

bool ZepBuffer::IsHidden() const {
    auto windows = editor.FindBufferWindows(this);
    return windows.empty();
}

void ZepBuffer::SetFileFlags(uint32_t flags, bool set) {
    fileFlags = ZSetFlags(fileFlags, flags, set);
    editor.UpdateTabs();
}

bool ZepBuffer::HasFileFlags(uint32_t flags) const { return fileFlags & flags; }

void ZepBuffer::ToggleFileFlag(uint32_t flags) {
    fileFlags = ZSetFlags(fileFlags, flags, !(fileFlags & flags));
}

GlyphRange ZepBuffer::GetExpression(ExpressionType expressionType, const GlyphIterator &location, const std::vector<char> &beginExpression, const std::vector<char> &endExpression) const {
    GlyphIterator itr = Begin();
    GlyphIterator itrEnd = End();

    int maxDepth = -1;
    struct Expression {
        int depth = 0;
        GlyphRange range;
        std::vector<std::shared_ptr<Expression>> children;
        Expression *pParent = nullptr;
    };

    std::vector<std::shared_ptr<Expression>> topLevel;

    Expression *pCurrent = nullptr;
    Expression *pInner = nullptr;

    while (itr != itrEnd) {
        auto ch = itr.Char();

        for (auto &exp: beginExpression) {
            if (exp == ch) {
                auto child = std::make_shared<Expression>();
                if (pCurrent) {
                    pCurrent->children.push_back(child);
                    child->pParent = pCurrent;
                    child->depth = pCurrent->depth + 1;
                } else {
                    topLevel.push_back(child);
                }
                pCurrent = child.get();
                pCurrent->range.first = itr;
            }
        }

        for (auto &exp: endExpression) {
            if (exp == ch) {
                if (pCurrent) {
                    pCurrent->range.second = itr.Peek(1);

                    // Check the sub exp
                    if ((pCurrent->range.first <= location) && (pCurrent->range.second > location)) {
                        if (pCurrent->depth > maxDepth) {
                            maxDepth = pCurrent->depth;
                            pInner = pCurrent;
                        }
                    }
                    pCurrent = pCurrent->pParent;
                }
            }
        }

        itr.Move(1);
    }

    if (expressionType == ExpressionType::Inner) {
        if (pInner) return pInner->range;
        return {Begin(), Begin()};
    }

    Expression *pBest = nullptr;
    int dist = std::numeric_limits<int>::max();

    for (auto &outer: topLevel) {
        if (location >= outer->range.first && location < outer->range.second) return outer->range;

        auto leftDist = std::abs(outer->range.first.index - location.index);
        auto rightDist = std::abs(location.index - outer->range.second.index);
        if (leftDist < dist) {
            pBest = outer.get();
            dist = leftDist;
        }
        if (rightDist < dist) {
            pBest = outer.get();
            dist = rightDist;
        }
    }

    if (pBest) return pBest->range;
    return {Begin(), Begin()};
}

GlyphIterator ZepBuffer::End() const {
    return GlyphIterator(this, std::max(0l, long(workingBuffer.size() - 1)));
}

GlyphIterator ZepBuffer::Begin() const {
    return GlyphIterator(this);
}

void ZepBuffer::EndFlash() const {
    editor.SetFlags(ZClearFlags(editor.flags, ZepEditorFlags::FastUpdate));
}

void ZepBuffer::BeginFlash(float seconds, FlashType flashType, const GlyphRange &range) {
    if (range.first == range.second) return;

    auto marker = std::make_shared<RangeMarker>(*this);
    marker->SetRange(ByteRange(range.first.index, range.second.index));
    marker->SetBackgroundColor(ThemeColor::FlashColor);
    marker->displayType = RangeMarkerDisplayType::Timed | RangeMarkerDisplayType::Background;
    marker->markerType = RangeMarkerType::Mark;
    marker->duration = seconds;
    timer_restart(marker->timer);

    EndFlash();
}

const std::string &ZepBuffer::GetName() {
    if (!filePath.empty() && filePath.has_filename()) {
        name = filePath.filename().string();
    }
    return name;
}

} // namespace Zep
