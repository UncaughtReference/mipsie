#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <QWidget>
#include <QPointer>
#include <QString>
#include <QTextEdit>
#include <QLabel>
#include "../editor/editor.h"

class QListView;
class QProcess;
class QStandardItemModel;
class QModelIndex;

class Assembler : public QWidget
{
    Q_OBJECT

public:
    explicit Assembler(QWidget *parent = nullptr);
    void setRomFilePath(const QString &filePath);
    void assemble(bool openProject64OnSuccess = false);
    bool isBusy() const;

private:
    enum class ProcessStage
    {
        Idle,
        Armips,
        Checksum
    };

    QListView *ArmipsOutputView;
    QStandardItemModel *outputModel;
    QProcess *process;
    ProcessStage stage = ProcessStage::Idle;
    bool openProject64OnSuccess = false;
    QString configuredRomFilePath;
    QString activeRomFilePath;
    QString temporaryAsmPath;
    int sourceLineOffset = 0;
    QString originalSourcePath;
    QString originalSourceLabel;
    QPointer<Editor> sourceEditor;
    QPointer<QTextEdit> sourceBuffer;
    QString standardOutputBuffer;
    QString standardErrorBuffer;
    QLabel* romLabel;

    void startArmips(const QString &assemblyText, const QString &rootDirectory);
    void startChecksumUpdate();
    void appendProcessOutput(bool standardError);
    void flushProcessOutput();
    void appendOutputLine(const QString &line, bool isError = false);
    int sourceLineForOutput(const QString &line) const;
    void navigateToOutputLine(const QModelIndex &index);
    void finishProcess(int exitCode, bool crashed);
    void handleFailedToStart();
    void removeTemporaryAsm();
    bool focusProject64();
};

#endif // ASSEMBLER_H
