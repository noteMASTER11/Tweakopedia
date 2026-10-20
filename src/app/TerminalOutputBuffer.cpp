#include "app/TerminalOutputBuffer.h"

#include <QByteArray>

namespace tweakopedia::app {
namespace {

const auto truncationMarker = QByteArray("[Начало вывода удалено из экранного буфера]\n");

} // namespace

TerminalOutputBuffer::TerminalOutputBuffer(const qsizetype maximumBytes)
    : maximumBytes_(maximumBytes)
{
}

void TerminalOutputBuffer::clear()
{
    committed_.clear();
    currentLine_.clear();
    cursor_ = 0;
    carriageReturnPending_ = false;
}

void TerminalOutputBuffer::append(const QStringView chunk)
{
    for (const auto character : chunk) {
        if (character == u'\r') {
            cursor_ = 0;
            carriageReturnPending_ = true;
            continue;
        }
        if (character == u'\n') {
            commitLine();
            continue;
        }
        if (carriageReturnPending_) {
            currentLine_.clear();
            cursor_ = 0;
            carriageReturnPending_ = false;
        }
        if (character == u'\b') {
            if (cursor_ > 0) --cursor_;
            continue;
        }
        if (character.category() == QChar::Other_Control && character != u'\t')
            continue;

        if (cursor_ < currentLine_.size()) currentLine_[cursor_] = character;
        else currentLine_.append(character);
        ++cursor_;
    }
    trim();
}

QString TerminalOutputBuffer::text() const
{
    return committed_ + currentLine_;
}

void TerminalOutputBuffer::commitLine()
{
    committed_ += currentLine_;
    committed_ += u'\n';
    currentLine_.clear();
    cursor_ = 0;
    carriageReturnPending_ = false;
}

void TerminalOutputBuffer::trim()
{
    auto bytes = text().toUtf8();
    if (bytes.size() <= maximumBytes_) return;

    const auto tailBytes = qMax<qsizetype>(0, maximumBytes_ - truncationMarker.size());
    bytes = truncationMarker + bytes.right(tailBytes);
    const auto trimmed = QString::fromUtf8(bytes);
    const auto finalNewline = trimmed.lastIndexOf(u'\n');
    if (finalNewline < 0) {
        committed_.clear();
        currentLine_ = trimmed;
    } else {
        committed_ = trimmed.first(finalNewline + 1);
        currentLine_ = trimmed.sliced(finalNewline + 1);
    }
    cursor_ = currentLine_.size();
}

} // namespace tweakopedia::app
