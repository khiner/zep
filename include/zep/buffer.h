#pragma once

#include <functional>
#include <set>

#include "zep/mcommon/file/path.h"
#include "zep/mcommon/string/stringutils.h"
#include "zep/mcommon/logger.h"
#include "zep/mcommon/signals.h"

#include "zep/glyph_iterator.h"

#include "zep/editor.h"
#include "zep/range_markers.h"

namespace Zep {

struct ZepSyntax;
struct ZepTheme;
struct ZepMode;
enum class ThemeColor;

enum class Direction {
    Forward,
    Backward
};

namespace SearchType {
enum : uint32_t {
    WORD = (1 << 0),
    Begin = (1 << 1),
    End = (1 << 2),
    Word = (1 << 3),
};
};

namespace FileFlags {
enum : uint32_t {
    StrippedCR = (1 << 0),
    TerminatedWithZero = (1 << 1),
    ReadOnly = (1 << 2),
    Locked = (1 << 3), // Can this file path ever be written to?
    Dirty = (1 << 4), // Has the file been changed?
    HasWarnings = (1 << 6),
    HasErrors = (1 << 7),
    DefaultBuffer = (1 << 8), // Default startup buffer
    HasTabs = (1 << 9),
    HasSpaceTabs = (1 << 10),
    InsertTabs = (1 << 11)
};
}

// Ensure the character is >=0 and <=127 as in the ASCII standard,
// isalnum, for example will assert on debug build if not in this range.
inline int ToASCII(const char ch) {
    auto ret = (unsigned int) ch;
    ret = std::min(ret, 127u);
    return ret;
}

enum class BufferType {
    Normal,
    Search,
    Repl,
    Tree
};

// A really big cursor move; which will likely clamp
//static const iterator MaxCursorMove = iterator(0xFFFFFFF);
//const long InvalidByteIndex = -1;

enum class ExpressionType { Inner, Outer };

// The type of replacement that happens in the buffer
enum class ReplaceRangeMode { Fill, Replace };

struct ChangeRecord {
    std::string strDeleted;
    std::string strInserted;
    GlyphIterator itrStart;
    GlyphIterator itrEnd;

    void Clear() {
        strDeleted.clear();
        itrStart.Invalidate();
        itrEnd.Invalidate();
    }
};

using fnKeyNotifier = std::function<bool(uint32_t key, uint32_t modifier)>;
struct ZepBuffer : public ZepComponent {
    ZepBuffer(ZepEditor &editor, std::string strName);
    ZepBuffer(ZepEditor &editor, const ZepPath &path);

    void Clear();
    void SetText(const std::string &strText, bool initFromFile = false);
    void Load(const ZepPath &path);
    bool Save(int64_t &size);

    std::string GetFileExtension() const;
    void SetFilePath(const ZepPath &path);

    GlyphIterator GetLinePos(GlyphIterator bufferLocation, LineLocation lineLocation) const;
    bool GetLineOffsets(long line, ByteRange &range) const;
    GlyphIterator ClampToVisibleLine(GlyphIterator in) const;
    long GetBufferColumn(const GlyphIterator &location) const;
    using fnMatch = std::function<bool(const char)>;

    bool Move(GlyphIterator &loc, Direction dir) const;
    static void MotionBegin(GlyphIterator &start);
    bool Skip(const fnMatch &IsToken, GlyphIterator &start, Direction dir) const;
    bool SkipOne(const fnMatch &IsToken, GlyphIterator &start, Direction dir) const;
    bool SkipNot(const fnMatch &IsToken, GlyphIterator &start, Direction dir) const;

    GlyphIterator Find(const GlyphIterator &start, const uint8_t *pBegin, const uint8_t *pEnd) const;
    GlyphIterator FindFirstCharOf(GlyphIterator &start, const std::string &chars, int32_t &foundIndex, Direction dir) const;
    GlyphIterator FindOnLineMotion(GlyphIterator start, const uint8_t *pCh, Direction dir) const;
    std::pair<GlyphIterator, GlyphIterator> FindMatchingPair(GlyphIterator start, uint8_t ch) const;
    GlyphIterator WordMotion(GlyphIterator start, uint32_t searchType, Direction dir) const;
    GlyphIterator EndWordMotion(GlyphIterator start, uint32_t searchType, Direction dir) const;
    GlyphIterator ChangeWordMotion(GlyphIterator start, uint32_t searchType, Direction dir) const;
    GlyphRange AWordMotion(GlyphIterator start, uint32_t searchType) const;
    GlyphRange InnerWordMotion(GlyphIterator start, uint32_t searchType) const;
    GlyphRange StandardCtrlMotion(GlyphIterator cursor, Direction searchDir) const;

    // Things that change
    bool Delete(const GlyphIterator &startOffset, const GlyphIterator &endOffset, ChangeRecord &changeRecord);
    bool Insert(const GlyphIterator &startOffset, const std::string &str, ChangeRecord &changeRecord);
    bool Replace(const GlyphIterator &startOffset, const GlyphIterator &endOffset, /*note; not ref*/ std::string str, ReplaceRangeMode mode, ChangeRecord &changeRecord);

    long GetLineCount() const { return long(lineEnds.size()); }
    long GetBufferLine(const GlyphIterator &offset) const;

    GlyphIterator End() const;
    GlyphIterator Begin() const;

    void SetSyntaxProvider(const SyntaxProvider &provider) {
        if (provider.syntaxID != syntaxProvider.syntaxID) {
            if (provider.factory) syntax = provider.factory(this);
            else syntax.reset();

            syntaxProvider = provider;
        }
    }

    std::string GetDisplayName() const;
    void Notify(const std::shared_ptr<ZepMessage> &message) override;

    ZepTheme &GetTheme() const;

    void SetSelection(const GlyphRange &sel);
    bool HasSelection() const;
    void ClearSelection();

    void AddRangeMarker(const std::shared_ptr<RangeMarker> &marker);
    void ClearRangeMarker(const std::shared_ptr<RangeMarker> &marker);
    void ClearRangeMarkers(uint32_t types);
    tRangeMarkers GetRangeMarkers(uint32_t types) const;
    tRangeMarkers GetRangeMarkersOnLine(uint32_t types, long line) const;
    void HideMarkers(uint32_t markerType) const;
    void ShowMarkers(uint32_t markerType, uint32_t displayType) const;

    void ForEachMarker(uint32_t types, Direction dir, const GlyphIterator &begin, const GlyphIterator &end, std::function<bool(const std::shared_ptr<RangeMarker> &)> fnCB) const;
    std::shared_ptr<RangeMarker> FindNextMarker(GlyphIterator start, Direction dir, uint32_t markerType) const;

    GlyphIterator GetLastEditLocation();

    ZepMode *GetMode() const;
    void SetMode(std::shared_ptr<ZepMode> mode);

    bool IsHidden() const;

    bool HasFileFlags(uint32_t flags) const;
    void SetFileFlags(uint32_t flags, bool set = true);
    void ToggleFileFlag(uint32_t flags);

    GlyphRange GetExpression(ExpressionType expressionType, const GlyphIterator &location, const std::vector<char> &beginExpression, const std::vector<char> &endExpression) const;
    std::string GetBufferText(const GlyphIterator &start, const GlyphIterator &end) const;

    void EndFlash() const;
    void BeginFlash(float seconds, FlashType flashType, const GlyphRange &range);

    Zep::signal<void(ZepBuffer &buffer, const GlyphIterator &, const std::string &)> sigPreInsert;
    Zep::signal<void(ZepBuffer &buffer, const GlyphIterator &, const GlyphIterator &)> sigPreDelete;

    std::string name;
    GapBuffer<uint8_t> workingBuffer; // Buffer & record of the line end locations
    std::vector<ByteIndex> lineEnds;

    ZepPath filePath;
    uint64_t updateCount = 0;
    GlyphIterator lastEditLocation;

    std::shared_ptr<ZepSyntax> syntax;

    GlyphRange selection;

    fnKeyNotifier postKeyNotifier;
    BufferType type = BufferType::Normal;

    uint32_t fileFlags = 0;
    SyntaxProvider syntaxProvider;
    tRangeMarkers rangeMarkers;
    std::shared_ptr<ZepMode> mode;

private:
    void MarkUpdate();
};

// Notification payload
enum class BufferMessageType {
    // Inform clients that we are about to mess with the buffer
    PreBufferChange = 0,
    TextChanged,
    TextDeleted,
    TextAdded,
    Loaded,
    MarkersChanged
};

struct BufferMessage : public ZepMessage {
    BufferMessage(ZepBuffer *pBuff, BufferMessageType messageType, const GlyphIterator &startLoc, const GlyphIterator &endLoc)
        : ZepMessage(Msg::Buffer), buffer(pBuff), type(messageType), startLocation(startLoc), endLocation(endLoc) {}

    ZepBuffer *buffer;
    BufferMessageType type;
    GlyphIterator startLocation;
    GlyphIterator endLocation;
};


} // namespace Zep
