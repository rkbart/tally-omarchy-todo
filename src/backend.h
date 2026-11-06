#pragma once
#include <QClipboard>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList lists READ lists NOTIFY changed)
    Q_PROPERTY(QString currentListId READ currentListId NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool undoAvailable READ undoAvailable NOTIFY undoChanged)
public:
    explicit Backend(QObject *parent = nullptr);
    QVariantList lists() const;
    QString currentListId() const { return m_current; }
    QString error() const { return m_error; }
    bool undoAvailable() const { return !m_undo.isEmpty(); }
    Q_INVOKABLE void selectList(const QString &id);
    Q_INVOKABLE void addList(const QString &name);
    Q_INVOKABLE void renameList(const QString &id, const QString &name);
    Q_INVOKABLE void removeList(const QString &id);
    Q_INVOKABLE void addTask(const QString &text, const QString &parentId = {});
    Q_INVOKABLE void addClipboard(const QString &parentId = {});
    Q_INVOKABLE void setDone(const QString &id, bool done);
    Q_INVOKABLE void setTaskText(const QString &id, const QString &text);
    Q_INVOKABLE void removeTask(const QString &id);
    Q_INVOKABLE bool canMoveTaskOrder(const QString &id, int direction) const;
    Q_INVOKABLE bool moveTaskOrder(const QString &id, int direction);
    Q_INVOKABLE bool canMoveTaskUp(const QString &id) const;
    Q_INVOKABLE bool moveTaskUp(const QString &id);
    Q_INVOKABLE bool canPromoteTask(const QString &id) const;
    Q_INVOKABLE bool promoteTask(const QString &id, bool beforeParent);
    Q_INVOKABLE bool canIndentTask(const QString &id) const;
    Q_INVOKABLE bool indentTask(const QString &id);
    Q_INVOKABLE bool canIndentTaskUnderPrevious(const QString &id) const;
    Q_INVOKABLE bool indentTaskUnderPrevious(const QString &id);
    Q_INVOKABLE bool canMoveTaskDown(const QString &id, const QVariantList &collapsedIds) const;
    Q_INVOKABLE bool moveTaskDown(const QString &id, const QVariantList &collapsedIds);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
signals:
    void changed();
    void errorChanged();
    void undoChanged();
private:
    QJsonObject *findList(const QString &id);
    bool editTasks(QJsonArray &tasks, const QString &id, const QString &op, const QVariant &value = {});
    void persist();
    QString m_path, m_current, m_error;
    QJsonArray m_lists;
    QJsonObject m_undo;
};
