#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QColor>
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

class QComboBox;
class QCheckBox;
class QFontComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTextEdit;
class QVBoxLayout;
class QWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(const QString &projectFolder,
                   const QString &romFilePath,
                   const QJsonObject &projectSettings,
                   const QJsonObject &generalSettings,
                   const QJsonArray &macros,
                   QWidget *parent = nullptr);

    QJsonObject generalSettings() const;
    QJsonObject projectSettings() const;
    QJsonArray macros() const;

private:
    struct MacroEditorRow
    {
        QWidget *container = nullptr;
        QKeySequenceEdit *shortcutEdit = nullptr;
        QTextEdit *textEdit = nullptr;
    };

    QString projectFolder;
    QJsonObject originalProjectSettings;
    QJsonObject originalGeneralSettings;
    QFontComboBox *applicationFontComboBox = nullptr;
    QFontComboBox *editorFontComboBox = nullptr;
    QSpinBox *applicationFontSizeSpinBox = nullptr;
    QSpinBox *editorFontSizeSpinBox = nullptr;
    QSpinBox *tabWidthSpinBox = nullptr;
    QCheckBox *createTemporaryAsmCheckBox = nullptr;
    QCheckBox *useArmipsRootOptionCheckBox = nullptr;
    QCheckBox *promptToRecoverSessionCheckBox = nullptr;
    QComboBox *themeComboBox = nullptr;
    QPushButton *accentColorButton = nullptr;
    QLineEdit *romPathEdit = nullptr;
    QLineEdit *backupDirectoryEdit = nullptr;
    QLineEdit *additionalArmipsArgumentsEdit = nullptr;
    QMap<QString, QKeySequenceEdit *> keySequenceEdits;
    QMap<QString, QColor> syntaxColors;
    QMap<QString, QPushButton *> syntaxColorButtons;
    QSet<QString> customSyntaxColorKeys;
    QList<MacroEditorRow> macroEditorRows;
    QVBoxLayout *macroListLayout = nullptr;
    QWidget *macroListContainer = nullptr;
    QScrollArea *macroScrollArea = nullptr;
    QColor accentColor;

    void chooseAccentColor();
    void chooseSyntaxColor(const QString &key);
    void browseRomFile();
    void browseBackupDirectory();
    void updateAccentColorButton();
    void updateSyntaxColorButton(const QString &key);
    void acceptSettings();
    void addMacroEditor(const QString &shortcut, const QString &text);
};

#endif // SETTINGSDIALOG_H
