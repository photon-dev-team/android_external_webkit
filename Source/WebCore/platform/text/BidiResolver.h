/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008 Apple Inc.  All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef BidiResolver_h
#define BidiResolver_h

#include "BidiContext.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

template <class Iterator> struct MidpointState {
    MidpointState()
    {
        reset();
    }
    
    void reset()
    {
        numMidpoints = 0;
        currentMidpoint = 0;
        betweenMidpoints = false;
    }
    
    // The goal is to reuse the line state across multiple
    // lines so we just keep an array around for midpoints and never clear it across multiple
    // lines.  We track the number of items and position using the two other variables.
    Vector<Iterator> midpoints;
    unsigned numMidpoints;
    unsigned currentMidpoint;
    bool betweenMidpoints;
};

// The BidiStatus at a given position (typically the end of a line) can
// be cached and then used to restart bidi resolution at that position.
struct BidiStatus {
    BidiStatus()
        : eor(WTF::Unicode::OtherNeutral)
        , lastStrong(WTF::Unicode::OtherNeutral)
        , last(WTF::Unicode::OtherNeutral)
    {
    }

    BidiStatus(WTF::Unicode::Direction eorDir, WTF::Unicode::Direction lastStrongDir, WTF::Unicode::Direction lastDir, PassRefPtr<BidiContext> bidiContext)
        : eor(eorDir)
        , lastStrong(lastStrongDir)
        , last(lastDir)
        , context(bidiContext)
    {
    }

    WTF::Unicode::Direction eor;
    WTF::Unicode::Direction lastStrong;
    WTF::Unicode::Direction last;
    RefPtr<BidiContext> context;
};

class BidiEmbedding {
public:
    BidiEmbedding(WTF::Unicode::Direction direction, BidiEmbeddingSource source)
    : m_direction(direction)
    , m_source(source)
    {
    }

    WTF::Unicode::Direction direction() const { return m_direction; }
    BidiEmbeddingSource source() const { return m_source; }
private:
    WTF::Unicode::Direction m_direction;
    BidiEmbeddingSource m_source;
};

inline bool operator==(const BidiStatus& status1, const BidiStatus& status2)
{
    return status1.eor == status2.eor && status1.last == status2.last && status1.lastStrong == status2.lastStrong && *(status1.context) == *(status2.context);
}

inline bool operator!=(const BidiStatus& status1, const BidiStatus& status2)
{
    return !(status1 == status2);
}

struct BidiCharacterRun {
    BidiCharacterRun(int start, int stop, BidiContext* context, WTF::Unicode::Direction dir)
        : m_start(start)
        , m_stop(stop)
        , m_override(context->override())
        , m_next(0)
    {
        if (dir == WTF::Unicode::OtherNeutral)
            dir = context->dir();

        m_level = context->level();

        // add level of run (cases I1 & I2)
        if (m_level % 2) {
            if (dir == WTF::Unicode::LeftToRight || dir == WTF::Unicode::ArabicNumber || dir == WTF::Unicode::EuropeanNumber)
                m_level++;
        } else {
            if (dir == WTF::Unicode::RightToLeft)
                m_level++;
            else if (dir == WTF::Unicode::ArabicNumber || dir == WTF::Unicode::EuropeanNumber)
                m_level += 2;
        }
    }

    void destroy() { delete this; }

    int start() const { return m_start; }
    int stop() const { return m_stop; }
    unsigned char level() const { return m_level; }
    bool reversed(bool visuallyOrdered) { return m_level % 2 && !visuallyOrdered; }
    bool dirOverride(bool visuallyOrdered) { return m_override || visuallyOrdered; }

    BidiCharacterRun* next() const { return m_next; }

    unsigned char m_level;
    int m_start;
    int m_stop;
    bool m_override;
    BidiCharacterRun* m_next;
};

enum VisualDirectionOverride {
    NoVisualOverride,
    VisualLeftToRightOverride,
    VisualRightToLeftOverride
};

template <class Iterator, class Run> class BidiResolver {
    WTF_MAKE_NONCOPYABLE(BidiResolver);
public:
    BidiResolver()
        : m_direction(WTF::Unicode::OtherNeutral)
        , m_reachedEndOfLine(false)
        , emptyRun(true)
        , m_firstRun(0)
        , m_lastRun(0)
        , m_logicallyLastRun(0)
        , m_runCount(0)
    {
    }

    const Iterator& position() const { return m_current; }
    void setPosition(const Iterator& position) { m_current = position; }

    void increment() { m_current.increment(); }

    BidiContext* context() const { return m_status.context.get(); }
    void setContext(PassRefPtr<BidiContext> c) { m_status.context = c; }

    void setLastDir(WTF::Unicode::Direction lastDir) { m_status.last = lastDir; }
    void setLastStrongDir(WTF::Unicode::Direction lastStrongDir) { m_status.lastStrong = lastStrongDir; }
    void setEorDir(WTF::Unicode::Direction eorDir) { m_status.eor = eorDir; }

    WTF::Unicode::Direction dir() const { return m_direction; }
    void setDir(WTF::Unicode::Direction d) { m_direction = d; }

    const BidiStatus& status() const { return m_status; }
    void setStatus(const BidiStatus s) { m_status = s; }

    MidpointState<Iterator>& midpointState() { return m_midpointState; }

    void embed(WTF::Unicode::Direction, BidiEmbeddingSource);
    bool commitExplicitEmbedding();

    void createBidiRunsForLine(const Iterator& end, VisualDirectionOverride = NoVisualOverride, bool hardLineBreak = false);

    Run* firstRun() const { return m_firstRun; }
    Run* lastRun() const { return m_lastRun; }
    Run* logicallyLastRun() const { return m_logicallyLastRun; }
    unsigned runCount() const { return m_runCount; }

    void addRun(Run*);
    void prependRun(Run*);

    void moveRunToEnd(Run*);
    void moveRunToBeginning(Run*);

    void deleteRuns();

protected:
    void appendRun();
    void reverseRuns(unsigned start, unsigned end);

    Iterator m_current;
    // sor and eor are "start of run" and "end of run" respectively and correpond
    // to abreviations used in UBA spec: http://unicode.org/reports/tr9/#BD7
    Iterator m_sor;
    Iterator m_eor;
    Iterator m_last;
    BidiStatus m_status;
    WTF::Unicode::Direction m_direction;
    Iterator endOfLine;
    bool m_reachedEndOfLine;
    Iterator lastBeforeET;
    bool emptyRun;

    Run* m_firstRun;
    Run* m_lastRun;
    Run* m_logicallyLastRun;
    unsigned m_runCount;
    MidpointState<Iterator> m_midpointState;

private:
    void raiseExplicitEmbeddingLevel(WTF::Unicode::Direction from, WTF::Unicode::Direction to);
    void lowerExplicitEmbeddingLevel(WTF::Unicode::Direction from);
    void checkDirectionInLowerRaiseEmbeddingLevel();

    void updateStatusLastFromCurrentDirection(WTF::Unicode::Direction);
    void reorderRunsFromLevels();

    Vector<BidiEmbedding, 8> m_currentExplicitEmbeddingSequence;
};

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::addRun(Run* run)
{
    if (!m_firstRun)
        m_firstRun = run;
    else
        m_lastRun->m_next = run;
    m_lastRun = run;
    m_runCount++;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::prependRun(Run* run)
{
    ASSERT(!run->m_next);

    if (!m_lastRun)
        m_lastRun = run;
    else
        run->m_next = m_firstRun;
    m_firstRun = run;
    m_runCount++;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::moveRunToEnd(Run* run)
{
    ASSERT(m_firstRun);
    ASSERT(m_lastRun);
    ASSERT(run->m_next);

    Run* current = 0;
    Run* next = m_firstRun;
    while (next != run) {
        current = next;
        next = current->next();
    }

    if (!current)
        m_firstRun = run->next();
    else
        current->m_next = run->m_next;

    run->m_next = 0;
    m_lastRun->m_next = run;
    m_lastRun = run;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::moveRunToBeginning(Run* run)
{
    ASSERT(m_firstRun);
    ASSERT(m_lastRun);
    ASSERT(run != m_firstRun);

    Run* current = m_firstRun;
    Run* next = current->next();
    while (next != run) {
        current = next;
        next = current->next();
    }

    current->m_next = run->m_next;
    if (run == m_lastRun)
        m_lastRun = current;

    run->m_next = m_firstRun;
    m_firstRun = run;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::appendRun()
{
    if (!emptyRun && !m_eor.atEnd()) {
        unsigned startOffset = m_sor.offset();
        unsigned endOffset = m_eor.offset();

        if (!endOfLine.atEnd() && endOffset >= endOfLine.offset()) {
            m_reachedEndOfLine = true;
            endOffset = endOfLine.offset();
        }

        if (endOffset >= startOffset)
            addRun(new Run(startOffset, endOffset + 1, context(), m_direction));

        m_eor.increment();
        m_sor = m_eor;
    }

    m_direction = WTF::Unicode::OtherNeutral;
    m_status.eor = WTF::Unicode::OtherNeutral;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::embed(WTF::Unicode::Direction dir, BidiEmbeddingSource source)
{
    using namespace WTF::Unicode;

    ASSERT(dir == PopDirectionalFormat || dir == LeftToRightEmbedding || dir == LeftToRightOverride || dir == RightToLeftEmbedding || dir == RightToLeftOverride);
    m_currentExplicitEmbeddingSequence.append(BidiEmbedding(dir, source));
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::checkDirectionInLowerRaiseEmbeddingLevel()
{
    using namespace WTF::Unicode;

    ASSERT(m_status.eor != OtherNeutral || m_eor.atEnd());
    ASSERT(m_status.last != NonSpacingMark
        && m_status.last != BoundaryNeutral
        && m_status.last != RightToLeftEmbedding
        && m_status.last != LeftToRightEmbedding
        && m_status.last != RightToLeftOverride 
        && m_status.last != LeftToRightOverride 
        && m_status.last != PopDirectionalFormat);
    if (m_direction == OtherNeutral)
        m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : RightToLeft;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::lowerExplicitEmbeddingLevel(WTF::Unicode::Direction from)
{
    using namespace WTF::Unicode;

    if (!emptyRun && m_eor != m_last) {
        checkDirectionInLowerRaiseEmbeddingLevel();
        // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
        if (from == LeftToRight) {
            // bidi.sor ... bidi.eor ... bidi.last L
            if (m_status.eor == EuropeanNumber) {
                if (m_status.lastStrong != LeftToRight) {
                    m_direction = EuropeanNumber;
                    appendRun();
                }
            } else if (m_status.eor == ArabicNumber) {
                m_direction = ArabicNumber;
                appendRun();
            } else if (m_status.lastStrong != LeftToRight) {
                appendRun();
                m_direction = LeftToRight;
            }
        } else if (m_status.eor == EuropeanNumber || m_status.eor == ArabicNumber || m_status.lastStrong == LeftToRight) {
            appendRun();
            m_direction = RightToLeft;
        }
        m_eor = m_last;
    }
    appendRun();
    emptyRun = true;
    // sor for the new run is determined by the higher level (rule X10)
    setLastDir(from);
    setLastStrongDir(from);
    m_eor = Iterator();
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::raiseExplicitEmbeddingLevel(WTF::Unicode::Direction from, WTF::Unicode::Direction to)
{
    using namespace WTF::Unicode;

    if (!emptyRun && m_eor != m_last) {
        checkDirectionInLowerRaiseEmbeddingLevel();
        // bidi.sor ... bidi.eor ... bidi.last eor; need to append the bidi.sor-bidi.eor run or extend it through bidi.last
        if (to == LeftToRight) {
            // bidi.sor ... bidi.eor ... bidi.last L
            if (m_status.eor == EuropeanNumber) {
                if (m_status.lastStrong != LeftToRight) {
                    m_direction = EuropeanNumber;
                    appendRun();
                }
            } else if (m_status.eor == ArabicNumber) {
                m_direction = ArabicNumber;
                appendRun();
            } else if (m_status.lastStrong != LeftToRight && from == LeftToRight) {
                appendRun();
                m_direction = LeftToRight;
            }
        } else if (m_status.eor == ArabicNumber
            || (m_status.eor == EuropeanNumber && (m_status.lastStrong != LeftToRight || from == RightToLeft))
            || (m_status.eor != EuropeanNumber && m_status.lastStrong == LeftToRight && from == RightToLeft)) {
            appendRun();
            m_direction = RightToLeft;
        }
        m_eor = m_last;
    }
    appendRun();
    emptyRun = true;
    setLastDir(to);
    setLastStrongDir(to);
    m_eor = Iterator();
}

template <class Iterator, class Run>
bool BidiResolver<Iterator, Run>::commitExplicitEmbedding()
{
    using namespace WTF::Unicode;

    unsigned char fromLevel = context()->level();
    RefPtr<BidiContext> toContext = context();

    for (size_t i = 0; i < m_currentExplicitEmbeddingSequence.size(); ++i) {
        BidiEmbedding embedding = m_currentExplicitEmbeddingSequence[i];
        if (embedding.direction() == PopDirectionalFormat) {
            if (BidiContext* parentContext = toContext->parent())
                toContext = parentContext;
        } else {
            Direction direction = (embedding.direction() == RightToLeftEmbedding || embedding.direction() == RightToLeftOverride) ? RightToLeft : LeftToRight;
            bool override = embedding.direction() == LeftToRightOverride || embedding.direction() == RightToLeftOverride;
            unsigned char level = toContext->level();
            if (direction == RightToLeft)
                level = nextGreaterOddLevel(level);
            else
                level = nextGreaterEvenLevel(level);
            if (level < 61)
                toContext = BidiContext::create(level, direction, override, embedding.source(), toContext.get());
        }
    }

    unsigned char toLevel = toContext->level();

    if (toLevel > fromLevel)
        raiseExplicitEmbeddingLevel(fromLevel % 2 ? RightToLeft : LeftToRight, toLevel % 2 ? RightToLeft : LeftToRight);
    else if (toLevel < fromLevel)
        lowerExplicitEmbeddingLevel(fromLevel % 2 ? RightToLeft : LeftToRight);

    setContext(toContext);

    m_currentExplicitEmbeddingSequence.clear();

    return fromLevel != toLevel;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::deleteRuns()
{
    emptyRun = true;
    if (!m_firstRun)
        return;

    Run* curr = m_firstRun;
    while (curr) {
        Run* s = curr->next();
        curr->destroy();
        curr = s;
    }

    m_firstRun = 0;
    m_lastRun = 0;
    m_runCount = 0;
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::reverseRuns(unsigned start, unsigned end)
{
    if (start >= end)
        return;

    ASSERT(end < m_runCount);
    
    // Get the item before the start of the runs to reverse and put it in
    // |beforeStart|.  |curr| should point to the first run to reverse.
    Run* curr = m_firstRun;
    Run* beforeStart = 0;
    unsigned i = 0;
    while (i < start) {
        i++;
        beforeStart = curr;
        curr = curr->next();
    }

    Run* startRun = curr;
    while (i < end) {
        i++;
        curr = curr->next();
    }
    Run* endRun = curr;
    Run* afterEnd = curr->next();

    i = start;
    curr = startRun;
    Run* newNext = afterEnd;
    while (i <= end) {
        // Do the reversal.
        Run* next = curr->next();
        curr->m_next = newNext;
        newNext = curr;
        curr = next;
        i++;
    }

    // Now hook up beforeStart and afterEnd to the startRun and endRun.
    if (beforeStart)
        beforeStart->m_next = endRun;
    else
        m_firstRun = endRun;

    startRun->m_next = afterEnd;
    if (!afterEnd)
        m_lastRun = startRun;
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::updateStatusLastFromCurrentDirection(WTF::Unicode::Direction dirCurrent)
{
    using namespace WTF::Unicode;
    switch (dirCurrent) {
    case EuropeanNumberTerminator:
        if (m_status.last != EuropeanNumber)
            m_status.last = EuropeanNumberTerminator;
        break;
    case EuropeanNumberSeparator:
    case CommonNumberSeparator:
    case SegmentSeparator:
    case WhiteSpaceNeutral:
    case OtherNeutral:
        switch (m_status.last) {
        case LeftToRight:
        case RightToLeft:
        case RightToLeftArabic:
        case EuropeanNumber:
        case ArabicNumber:
            m_status.last = dirCurrent;
            break;
        default:
            m_status.last = OtherNeutral;
        }
        break;
    case NonSpacingMark:
    case BoundaryNeutral:
    case RightToLeftEmbedding:
    case LeftToRightEmbedding:
    case RightToLeftOverride:
    case LeftToRightOverride:
    case PopDirectionalFormat:
        // ignore these
        break;
    case EuropeanNumber:
        // fall through
    default:
        m_status.last = dirCurrent;
    }
}

template <class Iterator, class Run>
inline void BidiResolver<Iterator, Run>::reorderRunsFromLevels()
{
    unsigned char levelLow = 128;
    unsigned char levelHigh = 0;
    for (Run* run = firstRun(); run; run = run->next()) {
        levelHigh = std::max(run->level(), levelHigh);
        levelLow = std::min(run->level(), levelLow);
    }

    // This implements reordering of the line (L2 according to Bidi spec):
    // http://unicode.org/reports/tr9/#L2
    // L2. From the highest level found in the text to the lowest odd level on each line,
    // reverse any contiguous sequence of characters that are at that level or higher.

    // Reversing is only done up to the lowest odd level.
    if (!(levelLow % 2))
        levelLow++;

    unsigned count = runCount() - 1;

    while (levelHigh >= levelLow) {
        unsigned i = 0;
        Run* run = firstRun();
        while (i < count) {
            for (;i < count && run && run->level() < levelHigh; i++)
                run = run->next();
            unsigned start = i;
            for (;i <= count && run && run->level() >= levelHigh; i++)
                run = run->next();
            unsigned end = i - 1;
            reverseRuns(start, end);
        }
        levelHigh--;
    }
}

template <class Iterator, class Run>
void BidiResolver<Iterator, Run>::createBidiRunsForLine(const Iterator& end, VisualDirectionOverride override, bool hardLineBreak)
{
    using namespace WTF::Unicode;

    ASSERT(m_direction == OtherNeutral);

    if (override != NoVisualOverride) {
        emptyRun = false;
        m_sor = m_current;
        m_eor = Iterator();
        while (m_current != end && !m_current.atEnd()) {
            m_eor = m_current;
            increment();
        }
        m_direction = override == VisualLeftToRightOverride ? LeftToRight : RightToLeft;
        appendRun();
        m_logicallyLastRun = m_lastRun;
        if (override == VisualRightToLeftOverride)
            reverseRuns(0, runCount() - 1);
        return;
    }

    emptyRun = true;

    m_eor = Iterator();

    m_last = m_current;
    bool pastEnd = false;
    BidiResolver<Iterator, Run> stateAtEnd;

    while (true) {
        Direction dirCurrent;
        if (pastEnd && (hardLineBreak || m_current.atEnd())) {
            BidiContext* c = context();
            if (hardLineBreak) {
                // A deviation from the Unicode Bidi Algorithm in order to match
                // WinIE and user expectations: hard line breaks reset bidi state
                // coming from unicode bidi control characters, but not those from
                // DOM nodes with specified directionality
                stateAtEnd.setContext(c->copyStackRemovingUnicodeEmbeddingContexts());

                dirCurrent = stateAtEnd.context()->dir();
                stateAtEnd.setEorDir(dirCurrent);
                stateAtEnd.setLastDir(dirCurrent);
                stateAtEnd.setLastStrongDir(dirCurrent);
            } else {
                while (c->parent())
                    c = c->parent();
                dirCurrent = c->dir();
            }
        } else {
            dirCurrent = m_current.direction();
            if (context()->override()
                    && dirCurrent != RightToLeftEmbedding
                    && dirCurrent != LeftToRightEmbedding
                    && dirCurrent != RightToLeftOverride
                    && dirCurrent != LeftToRightOverride
                    && dirCurrent != PopDirectionalFormat)
                dirCurrent = context()->dir();
            else if (dirCurrent == NonSpacingMark)
                dirCurrent = m_status.last;
        }

        ASSERT(m_status.eor != OtherNeutral || m_eor.atEnd());
        switch (dirCurrent) {

        // embedding and overrides (X1-X9 in the Bidi specs)
        case RightToLeftEmbedding:
        case LeftToRightEmbedding:
        case RightToLeftOverride:
        case LeftToRightOverride:
        case PopDirectionalFormat:
            embed(dirCurrent, FromUnicode);
            commitExplicitEmbedding();
            break;

        // strong types
        case LeftToRight:
            switch(m_status.last) {
                case RightToLeft:
                case RightToLeftArabic:
                case EuropeanNumber:
                case ArabicNumber:
                    if (m_status.last != EuropeanNumber || m_status.lastStrong != LeftToRight)
                        appendRun();
                    break;
                case LeftToRight:
                    break;
                case EuropeanNumberSeparator:
                case EuropeanNumberTerminator:
                case CommonNumberSeparator:
                case BoundaryNeutral:
                case BlockSeparator:
                case SegmentSeparator:
                case WhiteSpaceNeutral:
                case OtherNeutral:
                    if (m_status.eor == EuropeanNumber) {
                        if (m_status.lastStrong != LeftToRight) {
                            // the numbers need to be on a higher embedding level, so let's close that run
                            m_direction = EuropeanNumber;
                            appendRun();
                            if (context()->dir() != LeftToRight) {
                                // the neutrals take the embedding direction, which is R
                                m_eor = m_last;
                                m_direction = RightToLeft;
                                appendRun();
                            }
                        }
                    } else if (m_status.eor == ArabicNumber) {
                        // Arabic numbers are always on a higher embedding level, so let's close that run
                        m_direction = ArabicNumber;
                        appendRun();
                        if (context()->dir() != LeftToRight) {
                            // the neutrals take the embedding direction, which is R
                            m_eor = m_last;
                            m_direction = RightToLeft;
                            appendRun();
                        }
                    } else if (m_status.lastStrong != LeftToRight) {
                        //last stuff takes embedding dir
                        if (context()->dir() == RightToLeft) {
                            m_eor = m_last; 
                            m_direction = RightToLeft;
                        }
                        appendRun();
                    }
                default:
                    break;
            }
            m_eor = m_current;
            m_status.eor = LeftToRight;
            m_status.lastStrong = LeftToRight;
            m_direction = LeftToRight;
            break;
        case RightToLeftArabic:
        case RightToLeft:
            switch (m_status.last) {
                case LeftToRight:
                case EuropeanNumber:
                case ArabicNumber:
                    appendRun();
                case RightToLeft:
                case RightToLeftArabic:
                    break;
                case EuropeanNumberSeparator:
                case EuropeanNumberTerminator:
                case CommonNumberSeparator:
                case BoundaryNeutral:
                case BlockSeparator:
                case SegmentSeparator:
                case WhiteSpaceNeutral:
                case OtherNeutral:
                    if (m_status.eor == EuropeanNumber) {
                        if (m_status.lastStrong == LeftToRight && context()->dir() == LeftToRight)
                            m_eor = m_last;
                        appendRun();
                    } else if (m_status.eor == ArabicNumber)
                        appendRun();
                    else if (m_status.lastStrong == LeftToRight) {
                        if (context()->dir() == LeftToRight)
                            m_eor = m_last;
                        appendRun();
                    }
                default:
                    break;
            }
            m_eor = m_current;
            m_status.eor = RightToLeft;
            m_status.lastStrong = dirCurrent;
            m_direction = RightToLeft;
            break;

            // weak types:

        case EuropeanNumber:
            if (m_status.lastStrong != RightToLeftArabic) {
                // if last strong was AL change EN to AN
                switch (m_status.last) {
                    case EuropeanNumber:
                    case LeftToRight:
                        break;
                    case RightToLeft:
                    case RightToLeftArabic:
                    case ArabicNumber:
                        m_eor = m_last;
                        appendRun();
                        m_direction = EuropeanNumber;
                        break;
                    case EuropeanNumberSeparator:
                    case CommonNumberSeparator:
                        if (m_status.eor == EuropeanNumber)
                            break;
                    case EuropeanNumberTerminator:
                    case BoundaryNeutral:
                    case BlockSeparator:
                    case SegmentSeparator:
                    case WhiteSpaceNeutral:
                    case OtherNeutral:
                        if (m_status.eor == EuropeanNumber) {
                            if (m_status.lastStrong == RightToLeft) {
                                // ENs on both sides behave like Rs, so the neutrals should be R.
                                // Terminate the EN run.
                                appendRun();
                                // Make an R run.
                                m_eor = m_status.last == EuropeanNumberTerminator ? lastBeforeET : m_last;
                                m_direction = RightToLeft;
                                appendRun();
                                // Begin a new EN run.
                                m_direction = EuropeanNumber;
                            }
                        } else if (m_status.eor == ArabicNumber) {
                            // Terminate the AN run.
                            appendRun();
                            if (m_status.lastStrong == RightToLeft || context()->dir() == RightToLeft) {
                                // Make an R run.
                                m_eor = m_status.last == EuropeanNumberTerminator ? lastBeforeET : m_last;
                                m_direction = RightToLeft;
                                appendRun();
                                // Begin a new EN run.
                                m_direction = EuropeanNumber;
                            }
                        } else if (m_status.lastStrong == RightToLeft) {
                            // Extend the R run to include the neutrals.
                            m_eor = m_status.last == EuropeanNumberTerminator ? lastBeforeET : m_last;
                            m_direction = RightToLeft;
                            appendRun();
                            // Begin a new EN run.
                            m_direction = EuropeanNumber;
                        }
                    default:
                        break;
                }
                m_eor = m_current;
                m_status.eor = EuropeanNumber;
                if (m_direction == OtherNeutral)
                    m_direction = LeftToRight;
                break;
            }
        case ArabicNumber:
            dirCurrent = ArabicNumber;
            switch (m_status.last) {
                case LeftToRight:
                    if (context()->dir() == LeftToRight)
                        appendRun();
                    break;
                case ArabicNumber:
                    break;
                case RightToLeft:
                case RightToLeftArabic:
                case EuropeanNumber:
                    m_eor = m_last;
                    appendRun();
                    break;
                case CommonNumberSeparator:
                    if (m_status.eor == ArabicNumber)
                        break;
                case EuropeanNumberSeparator:
                case EuropeanNumberTerminator:
                case BoundaryNeutral:
                case BlockSeparator:
                case SegmentSeparator:
                case WhiteSpaceNeutral:
                case OtherNeutral:
                    if (m_status.eor == ArabicNumber
                        || (m_status.eor == EuropeanNumber && (m_status.lastStrong == RightToLeft || context()->dir() == RightToLeft))
                        || (m_status.eor != EuropeanNumber && m_status.lastStrong == LeftToRight && context()->dir() == RightToLeft)) {
                        // Terminate the run before the neutrals.
                        appendRun();
                        // Begin an R run for the neutrals.
                        m_direction = RightToLeft;
                    } else if (m_direction == OtherNeutral)
                        m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : RightToLeft;
                    m_eor = m_last;
                    appendRun();
                default:
                    break;
            }
            m_eor = m_current;
            m_status.eor = ArabicNumber;
            if (m_direction == OtherNeutral)
                m_direction = ArabicNumber;
            break;
        case EuropeanNumberSeparator:
        case CommonNumberSeparator:
            break;
        case EuropeanNumberTerminator:
            if (m_status.last == EuropeanNumber) {
                dirCurrent = EuropeanNumber;
                m_eor = m_current;
                m_status.eor = dirCurrent;
            } else if (m_status.last != EuropeanNumberTerminator)
                lastBeforeET = emptyRun ? m_eor : m_last;
            break;

        // boundary neutrals should be ignored
        case BoundaryNeutral:
            if (m_eor == m_last)
                m_eor = m_current;
            break;
            // neutrals
        case BlockSeparator:
            // ### what do we do with newline and paragraph seperators that come to here?
            break;
        case SegmentSeparator:
            // ### implement rule L1
            break;
        case WhiteSpaceNeutral:
            break;
        case OtherNeutral:
            break;
        default:
            break;
        }

        if (pastEnd && m_eor == m_current) {
            if (!m_reachedEndOfLine) {
                m_eor = endOfLine;
                switch (m_status.eor) {
                    case LeftToRight:
                    case RightToLeft:
                    case ArabicNumber:
                        m_direction = m_status.eor;
                        break;
                    case EuropeanNumber:
                        m_direction = m_status.lastStrong == LeftToRight ? LeftToRight : EuropeanNumber;
                        break;
                    default:
                        ASSERT(false);
                }
                appendRun();
            }
            m_current = end;
            m_status = stateAtEnd.m_status;
            m_sor = stateAtEnd.m_sor; 
            m_eor = stateAtEnd.m_eor;
            m_last = stateAtEnd.m_last;
            m_reachedEndOfLine = stateAtEnd.m_reachedEndOfLine;
            lastBeforeET = stateAtEnd.lastBeforeET;
            emptyRun = stateAtEnd.emptyRun;
            m_direction = OtherNeutral;
            break;
        }

        updateStatusLastFromCurrentDirection(dirCurrent);
        m_last = m_current;

        if (emptyRun) {
            m_sor = m_current;
            emptyRun = false;
        }

        increment();
        if (!m_currentExplicitEmbeddingSequence.isEmpty()) {
            bool committed = commitExplicitEmbedding();
            if (committed && pastEnd) {
                m_current = end;
                m_status = stateAtEnd.m_status;
                m_sor = stateAtEnd.m_sor; 
                m_eor = stateAtEnd.m_eor;
                m_last = stateAtEnd.m_last;
                m_reachedEndOfLine = stateAtEnd.m_reachedEndOfLine;
                lastBeforeET = stateAtEnd.lastBeforeET;
                emptyRun = stateAtEnd.emptyRun;
                m_direction = OtherNeutral;
                break;
            }
        }

        if (!pastEnd && (m_current == end || m_current.atEnd())) {
            if (emptyRun)
                break;
            stateAtEnd.m_status = m_status;
            stateAtEnd.m_sor = m_sor;
            stateAtEnd.m_eor = m_eor;
            stateAtEnd.m_last = m_last;
            stateAtEnd.m_reachedEndOfLine = m_reachedEndOfLine;
            stateAtEnd.lastBeforeET = lastBeforeET;
            stateAtEnd.emptyRun = emptyRun;
            endOfLine = m_last;
            pastEnd = true;
        }
    }

    m_logicallyLastRun = m_lastRun;
    reorderRunsFromLevels();
    endOfLine = Iterator();
}

} // namespace WebCore

#endif // BidiResolver_h