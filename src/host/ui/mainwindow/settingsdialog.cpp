#include "settingsdialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
struct ShortcutDefinition
{
    const char *key;
    const char *label;
};

const ShortcutDefinition shortcutDefinitions[] = {
    {"openFileAction", QT_TR_NOOP("Open File")},
    {"saveFileAction", QT_TR_NOOP("Save File")},
    {"saveFileAsAction", QT_TR_NOOP("Save File As")},
    {"closeTabAction", QT_TR_NOOP("Close Tab")},
    {"previousTabAction", QT_TR_NOOP("Previous Tab")},
    {"nextTabAction", QT_TR_NOOP("Next Tab")},
    {"newTabAction", QT_TR_NOOP("New Tab")},
    {"backupRomAction", QT_TR_NOOP("Back Up ROM")},
    {"restoreRomVersionAction", QT_TR_NOOP("Restore ROM Version")},
    {"openFolderAction", QT_TR_NOOP("Open Folder")},
    {"openProjectAction", QT_TR_NOOP("Open Project")},
    {"openSettingsAction", QT_TR_NOOP("Open Settings")},
    {"findAction", QT_TR_NOOP("Find")},
    {"replaceAction", QT_TR_NOOP("Find and Replace")},
    {"searchLabelsAction", QT_TR_NOOP("Search Labels")},
    {"fileTreeSearchAction", QT_TR_NOOP("Search File Tree")},
    {"assembleAction", QT_TR_NOOP("Assemble and Update Checksum")},
    {"assembleUpdateChecksumOpenProject64Action", QT_TR_NOOP("Assemble, UC, Open PJ64")},
    {"undoAction", QT_TR_NOOP("Undo")},
    {"redoAction", QT_TR_NOOP("Redo")},
    {"cutAction", QT_TR_NOOP("Cut")},
    {"copyAction", QT_TR_NOOP("Copy")},
    {"pasteAction", QT_TR_NOOP("Paste")},
    {"selectAllAction", QT_TR_NOOP("Select All")}
};
}

SettingsDialog::SettingsDialog(const QString &projectFolder,
                               const QString &romFilePath,
                               const QJsonObject &projectSettings,
                               const QJsonObject &generalSettings,
                               QWidget *parent)
    : QDialog(parent),
      projectFolder(projectFolder),
      originalProjectSettings(projectSettings),
      originalGeneralSettings(generalSettings)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(640, 620);

    auto *rootLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    rootLayout->addWidget(tabs, 1);

    auto *generalScrollArea = new QScrollArea(tabs);
    generalScrollArea->setWidgetResizable(true);
    auto *generalPage = new QWidget(generalScrollArea);
    auto *generalLayout = new QVBoxLayout(generalPage);
    auto *generalForm = new QFormLayout;

    applicationFontComboBox = new QFontComboBox(generalPage);
    applicationFontComboBox->setCurrentFont(QFont(generalSettings.value(
        QStringLiteral("applicationFontFamily")).toString(QStringLiteral("JetBrainsMono Nerd Font"))));
    generalForm->addRow(tr("Application font:"), applicationFontComboBox);
    applicationFontSizeSpinBox = new QSpinBox(generalPage);
    applicationFontSizeSpinBox->setRange(8, 40);
    applicationFontSizeSpinBox->setSuffix(tr(" pt"));
    applicationFontSizeSpinBox->setValue(generalSettings.value(QStringLiteral("applicationFontSize")).toInt(12));
    generalForm->addRow(tr("Application font size:"), applicationFontSizeSpinBox);
    editorFontComboBox = new QFontComboBox(generalPage);
    editorFontComboBox->setCurrentFont(QFont(generalSettings.value(
        QStringLiteral("editorFontFamily")).toString(QStringLiteral("JetBrainsMono Nerd Font"))));
    generalForm->addRow(tr("Editor font:"), editorFontComboBox);
    editorFontSizeSpinBox = new QSpinBox(generalPage);
    editorFontSizeSpinBox->setRange(8, 40);
    editorFontSizeSpinBox->setSuffix(tr(" pt"));
    editorFontSizeSpinBox->setValue(generalSettings.value(QStringLiteral("editorFontSize")).toInt(12));
    generalForm->addRow(tr("Editor font size:"), editorFontSizeSpinBox);

    tabWidthSpinBox = new QSpinBox(generalPage);
    tabWidthSpinBox->setRange(1, 16);
    tabWidthSpinBox->setSuffix(tr(" spaces"));
    tabWidthSpinBox->setValue(generalSettings.value(QStringLiteral("tabWidth")).toInt(4));
    generalForm->addRow(tr("Editor tab width:"), tabWidthSpinBox);

    themeComboBox = new QComboBox(generalPage);
    themeComboBox->addItem(tr("System"), QStringLiteral("System"));
    themeComboBox->addItem(tr("Light"), QStringLiteral("Light"));
    themeComboBox->addItem(tr("Dark"), QStringLiteral("Dark"));
    const QString savedTheme = generalSettings.value(QStringLiteral("theme"))
                                   .toString(QStringLiteral("System"));
    const int themeIndex = themeComboBox->findData(savedTheme);
    themeComboBox->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    generalForm->addRow(tr("Theme:"), themeComboBox);

    accentColor = QColor(generalSettings.value(QStringLiteral("accentColor"))
                             .toString(QStringLiteral("#ff4da6")));
    if (!accentColor.isValid())
        accentColor = QColor(QStringLiteral("#ff4da6"));
    accentColorButton = new QPushButton(generalPage);
    updateAccentColorButton();
    connect(accentColorButton, &QPushButton::clicked,
            this, &SettingsDialog::chooseAccentColor);
    generalForm->addRow(tr("Accent color:"), accentColorButton);
    generalLayout->addLayout(generalForm);

    auto *shortcutGroup = new QGroupBox(tr("Keyboard Shortcuts"), generalPage);
    auto *shortcutForm = new QFormLayout(shortcutGroup);
    const QJsonObject keybinds = generalSettings.value(QStringLiteral("keybinds")).toObject();
    for (const ShortcutDefinition &definition : shortcutDefinitions) {
        const QString key = QString::fromLatin1(definition.key);
        auto *sequenceEdit = new QKeySequenceEdit(shortcutGroup);
        sequenceEdit->setKeySequence(QKeySequence::fromString(
            keybinds.value(key).toString(), QKeySequence::PortableText));
        keySequenceEdits.insert(key, sequenceEdit);
        shortcutForm->addRow(tr(definition.label), sequenceEdit);
    }
    generalLayout->addWidget(shortcutGroup);
    generalLayout->addStretch(1);
    generalScrollArea->setWidget(generalPage);
    tabs->addTab(generalScrollArea, tr("General"));

    auto *projectPage = new QWidget(tabs);
    auto *projectLayout = new QVBoxLayout(projectPage);
    if (projectFolder.isEmpty()) {
        projectLayout->addWidget(new QLabel(
            tr("Open a project to edit its project settings."), projectPage));
        projectLayout->addStretch(1);
    } else {
        auto *projectForm = new QFormLayout;
        auto *folderLabel = new QLabel(QDir::toNativeSeparators(projectFolder), projectPage);
        folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        projectForm->addRow(tr("Project folder:"), folderLabel);

        romPathEdit = new QLineEdit(projectPage);
        romPathEdit->setText(QDir::toNativeSeparators(romFilePath));
        auto *romRow = new QWidget(projectPage);
        auto *romRowLayout = new QHBoxLayout(romRow);
        romRowLayout->setContentsMargins(0, 0, 0, 0);
        auto *browseRomButton = new QPushButton(tr("Browse..."), romRow);
        romRowLayout->addWidget(romPathEdit, 1);
        romRowLayout->addWidget(browseRomButton);
        connect(browseRomButton, &QPushButton::clicked,
                this, &SettingsDialog::browseRomFile);
        projectForm->addRow(tr("ROM file:"), romRow);
        projectLayout->addLayout(projectForm);
        projectLayout->addStretch(1);
    }
    tabs->addTab(projectPage, tr("Current Project"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *saveButton = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    rootLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::acceptSettings);
    saveButton->setDefault(true);
}

QJsonObject SettingsDialog::generalSettings() const
{
    QJsonObject settings = originalGeneralSettings;
    settings.insert(QStringLiteral("applicationFontFamily"), applicationFontComboBox->currentFont().family());
    settings.insert(QStringLiteral("applicationFontSize"), applicationFontSizeSpinBox->value());
    settings.insert(QStringLiteral("editorFontFamily"), editorFontComboBox->currentFont().family());
    settings.insert(QStringLiteral("editorFontSize"), editorFontSizeSpinBox->value());
    settings.insert(QStringLiteral("tabWidth"), tabWidthSpinBox->value());
    settings.insert(QStringLiteral("theme"), themeComboBox->currentData().toString());
    settings.insert(QStringLiteral("accentColor"), accentColor.name(QColor::HexRgb));

    QJsonObject keybinds = settings.value(QStringLiteral("keybinds")).toObject();
    for (auto it = keySequenceEdits.cbegin(); it != keySequenceEdits.cend(); ++it) {
        keybinds.insert(it.key(), it.value()->keySequence().toString(QKeySequence::PortableText));
    }
    settings.insert(QStringLiteral("keybinds"), keybinds);
    return settings;
}

QJsonObject SettingsDialog::projectSettings() const
{
    QJsonObject settings = originalProjectSettings;
    if (!romPathEdit)
        return settings;

    const QString enteredPath = romPathEdit->text().trimmed();
    const QString absolutePath = QFileInfo(
        QDir::isAbsolutePath(enteredPath)
            ? enteredPath
            : QDir(projectFolder).absoluteFilePath(enteredPath)).absoluteFilePath();
    settings.insert(QStringLiteral("romFilePath"), QDir::fromNativeSeparators(absolutePath));
    return settings;
}

void SettingsDialog::chooseAccentColor()
{
    const QColor selectedColor = QColorDialog::getColor(
        accentColor, this, tr("Choose Accent Color"));
    if (!selectedColor.isValid())
        return;
    accentColor = selectedColor;
    updateAccentColorButton();
}

void SettingsDialog::browseRomFile()
{
    const QString currentPath = romPathEdit->text().trimmed();
    QString initialDirectory = projectFolder;
    if (!currentPath.isEmpty()) {
        const QString resolvedPath = QDir::isAbsolutePath(currentPath)
            ? currentPath
            : QDir(projectFolder).absoluteFilePath(currentPath);
        initialDirectory = QFileInfo(resolvedPath).absolutePath();
    }
    const QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select ROM File"),
        initialDirectory,
        tr("Nintendo 64 ROMs (*.z64 *.n64 *.v64);;All files (*)"));
    if (!selectedPath.isEmpty())
        romPathEdit->setText(QDir::toNativeSeparators(selectedPath));
}

void SettingsDialog::updateAccentColorButton()
{
    accentColorButton->setText(accentColor.name(QColor::HexRgb).toUpper());
    accentColorButton->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; }").arg(accentColor.name()));
}

void SettingsDialog::acceptSettings()
{
    if (romPathEdit && romPathEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("ROM Path Required"),
                             tr("Enter a ROM file path for the current project."));
        return;
    }
    accept();
}
