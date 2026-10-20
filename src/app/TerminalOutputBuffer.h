#pragma once

#include <QString>
#include <QStringView>

namespace tweakopedia::app {

class TerminalOutputBuffer final
{
public:
    explicit TerminalOutputBuffer(qsizetype maximumBytes = 2 * 1024 * 1024);

    void clear();
    void append(QStringView chunk);
    [[nodiscard]] QString text() const;

private:
    void commitLine();
    void trim();

    qsizetype maximumBytes_{};
    QString committed_;
    QString currentLine_;
    qsizetype cursor_{};
};

} // namespace tweakopedia::app
