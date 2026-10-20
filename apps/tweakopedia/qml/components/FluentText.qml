import QtQuick
import QtQuick.Controls

Text {
    font.family: Application.font.family
    font.weight: Font.Medium
    font.hintingPreference: Font.PreferVerticalHinting
    font.preferTypoLineMetrics: true
    font.kerning: true
    renderType: Text.NativeRendering
    renderTypeQuality: Text.HighRenderTypeQuality
    antialiasing: true
}
