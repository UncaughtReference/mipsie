#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QColor>
#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <QString>

class QComboBox;
class QFontComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(const QString &projectFolder,
                   const QString &romFilePath,
                   const QJsonObject &projectSettings,
                   const QJsonObject &generalSettings,
                   QWidget *parent = nullptr);

    QJsonObject generalSettings() const;
    QJsonObject projectSettings() const;

private:
    QString projectFolder;
    QJsonObject originalProjectSettings;
    QJsonObject originalGeneralSettings;
    QFontComboBox *applicationFontComboBox = nullptr;
    QFontComboBox *editorFontComboBox = nullptr;
    QSpinBox *applicationFontSizeSpinBox = nullptr;
    QSpinBox *editorFontSizeSpinBox = nullptr;
    QSpinBox *tabWidthSpinBox = nullptr;
    QComboBox *themeComboBox = nullptr;
    QPushButton *accentColorButton = nullptr;
    QLineEdit *romPathEdit = nullptr;
    QMap<QString, QKeySequenceEdit *> keySequenceEdits;
    QColor accentColor;

    void chooseAccentColor();
    void browseRomFile();
    void updateAccentColorButton();
    void acceptSettings();
};

#endif // SETTINGSDIALOG_H
