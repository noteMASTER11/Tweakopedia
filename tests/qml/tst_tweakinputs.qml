import QtQuick
import QtQuick.Controls
import QtTest
import "../../apps/tweakopedia/qml/components"

TestCase {
    name: "TweakInputs"
    when: windowShown
    visible: true
    width: 760
    height: 520

    Component {
        id: editorComponent
        TweakInputEditor { width: 620 }
    }

    function test_textConfirmationAndEnter() {
        const editor = createTemporaryObject(editorComponent, this, {
            definition: {id: "manufacturer", label: "Производитель", type: "text", required: true}
        })
        verify(editor)
        const field = findChild(editor, "textInputField")
        const confirm = findChild(editor, "confirmTextInput")
        verify(field)
        verify(confirm)
        const spy = signalSpy.createObject(editor, {target: editor, signalName: "valueConfirmed"})
        field.text = "Tweakopedia"
        field.forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "manufacturer")
        compare(spy.signalArguments[0][1], "Tweakopedia")
    }

    function test_integerErrorAndRange() {
        const editor = createTemporaryObject(editorComponent, this, {
            definition: {id: "size", label: "Размер", type: "integer", required: true,
                         minimum: 1, maximum: 10}
        })
        const field = findChild(editor, "integerInputField")
        field.text = "99"
        findChild(editor, "confirmIntegerInput").clicked()
        compare(findChild(editor, "integerInputError").visible, true)
    }

    function test_choiceAndBooleanEditors() {
        const choice = createTemporaryObject(editorComponent, this, {
            definition: {id: "mode", label: "Режим", type: "choice", required: true,
                         choices: [{value: "auto", label: "Автоматически"},
                                   {value: "manual", label: "Вручную"}]}
        })
        const choiceSpy = signalSpy.createObject(choice, {target: choice, signalName: "valueConfirmed"})
        const combo = findChild(choice, "inputChoiceSelector")
        combo.currentIndex = 1
        combo.activated(1)
        compare(choiceSpy.signalArguments[0][1], "manual")

        const booleanEditor = createTemporaryObject(editorComponent, this, {
            definition: {id: "enabled", label: "Включить", type: "boolean", required: true}
        })
        const booleanSpy = signalSpy.createObject(booleanEditor, {target: booleanEditor, signalName: "valueConfirmed"})
        findChild(booleanEditor, "inputBooleanToggle").toggledByUser(true)
        compare(booleanSpy.signalArguments[0][1], true)
    }

    function test_fileSelectionCallback() {
        const editor = createTemporaryObject(editorComponent, this, {
            definition: {id: "logo", label: "Логотип", type: "file", required: true,
                         extensions: ["bmp"]}
        })
        const spy = signalSpy.createObject(editor, {target: editor, signalName: "valueConfirmed"})
        verify(findChild(editor, "fileChooseButton"))
        editor.acceptFile("D:/Temp/logo.bmp")
        compare(spy.signalArguments[0][0], "logo")
        compare(spy.signalArguments[0][1], "D:/Temp/logo.bmp")
    }

    Component { id: signalSpy; SignalSpy {} }
}
