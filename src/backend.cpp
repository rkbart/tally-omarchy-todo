#include "backend.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

static QString uid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

static bool normalizeTaskIds(QJsonArray &tasks, QSet<QString> &ids) {
    bool changed = false;
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        QString id = task.value("id").toString();
        if (id.isEmpty() || ids.contains(id)) {
            id = uid();
            task["id"] = id;
            changed = true;
        }
        ids.insert(id);
        auto children = task.value("children").toArray();
        if (normalizeTaskIds(children, ids)) {
            task["children"] = children;
            changed = true;
        }
        if (changed) tasks[i] = task;
    }
    return changed;
}

static bool appendChild(QJsonArray &tasks, const QString &parentId, const QString &text,
                        int depth, QString &error) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        if (task.value("id").toString() == parentId) {
            const auto children = task.value("children").toArray();
            if (depth >= 2) { error = QObject::tr("Tasks can only be nested two levels deep"); return false; }
            auto updated = children;
            updated.append(QJsonObject{{"id", uid()}, {"text", text}, {"done", false}, {"children", QJsonArray{}}});
            task["children"] = updated;
            tasks[i] = task;
            return true;
        }
        auto children = task.value("children").toArray();
        if (appendChild(children, parentId, text, depth + 1, error)) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
        if (!error.isEmpty()) return false;
    }
    return false;
}

static void setDoneRecursive(QJsonArray &tasks, bool done) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        task["done"] = done;
        auto children = task.value("children").toArray();
        setDoneRecursive(children, done);
        task["children"] = children;
        tasks[i] = task;
    }
}

static bool extractTask(QJsonArray &tasks, const QString &id, QJsonObject &result,
                        QString &parentId, int &index, const QString &parent = {}) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            result = task;
            parentId = parent;
            index = i;
            tasks.removeAt(i);
            return true;
        }
        auto children = task.value("children").toArray();
        if (extractTask(children, id, result, parentId, index, task.value("id").toString())) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
    }
    return false;
}

static bool insertTaskAtParent(QJsonArray &tasks, const QString &parentId, int index,
                               const QJsonObject &task) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto parent = tasks[i].toObject();
        if (parent.value("id").toString() == parentId) {
            auto children = parent.value("children").toArray();
            children.insert(qBound(0, index, children.size()), task);
            parent["children"] = children;
            tasks[i] = parent;
            return true;
        }
        auto children = parent.value("children").toArray();
        if (insertTaskAtParent(children, parentId, index, task)) {
            parent["children"] = children;
            tasks[i] = parent;
            return true;
        }
    }
    return false;
}

static bool canMoveOrder(const QJsonArray &tasks, const QString &id, int direction) {
    for (int i = 0; i < tasks.size(); ++i) {
        const auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            const bool done = task.value("done").toBool();
            for (int j = i + direction; j >= 0 && j < tasks.size(); j += direction)
                if (tasks[j].toObject().value("done").toBool() == done) return true;
            return false;
        }
        if (canMoveOrder(task.value("children").toArray(), id, direction)) return true;
    }
    return false;
}

static bool moveOrder(QJsonArray &tasks, const QString &id, int direction) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            const bool done = task.value("done").toBool();
            int target = i + direction;
            while (target >= 0 && target < tasks.size()
                   && tasks[target].toObject().value("done").toBool() != done)
                target += direction;
            if (target < 0 || target >= tasks.size()) return false;
            const QJsonValue moving = tasks[i];
            const QJsonValue adjacent = tasks[target];
            tasks[i] = adjacent;
            tasks[target] = moving;
            return true;
        }
        auto children = task.value("children").toArray();
        if (moveOrder(children, id, direction)) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
    }
    return false;
}

static QString adjacentSiblingId(const QJsonArray &tasks, const QString &id, int direction) {
    for (int i = 0; i < tasks.size(); ++i) {
        const auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            const bool done = task.value("done").toBool();
            for (int j = i + direction; j >= 0 && j < tasks.size(); j += direction)
                if (tasks[j].toObject().value("done").toBool() == done)
                    return tasks[j].toObject().value("id").toString();
            return {};
        }
        const QString nested = adjacentSiblingId(task.value("children").toArray(), id, direction);
        if (!nested.isEmpty()) return nested;
    }
    return {};
}

static bool idInList(const QVariantList &ids, const QString &id) {
    for (const auto &value : ids) if (value.toString() == id) return true;
    return false;
}

static bool canPromoteOneLevel(const QJsonArray &tasks, const QString &id, int depth = 0) {
    for (const auto &value : tasks) {
        const auto task = value.toObject();
        const auto children = task.value("children").toArray();
        for (const auto &child : children)
            if (child.toObject().value("id").toString() == id)
                return depth == 0 || tasks.size() < 2;
        if (canPromoteOneLevel(children, id, depth + 1)) return true;
    }
    return false;
}

static bool promoteOneLevel(QJsonArray &tasks, const QString &id, bool beforeParent) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        auto children = task.value("children").toArray();
        for (int j = 0; j < children.size(); ++j) {
            if (children[j].toObject().value("id").toString() != id) continue;
            const QJsonValue moving = children[j];
            children.removeAt(j);
            task["children"] = children;
            tasks[i] = task;
            tasks.insert(i + (beforeParent ? 0 : 1), moving);
            return true;
        }
        if (promoteOneLevel(children, id, beforeParent)) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
    }
    return false;
}

static int taskSubtreeHeight(const QJsonObject &task) {
    int height = 0;
    for (const auto &child : task.value("children").toArray())
        height = qMax(height, 1 + taskSubtreeHeight(child.toObject()));
    return height;
}

static bool canIndentUnderNext(const QJsonArray &tasks, const QString &id, int depth = 0) {
    for (int i = 0; i < tasks.size(); ++i) {
        const auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            if (i + 1 >= tasks.size() || depth + 1 + taskSubtreeHeight(task) > 2) return false;
            return true;
        }
        if (canIndentUnderNext(task.value("children").toArray(), id, depth + 1)) return true;
    }
    return false;
}

static bool indentUnderNext(QJsonArray &tasks, const QString &id) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            if (i + 1 >= tasks.size()) return false;
            const QJsonValue moving = tasks[i];
            auto target = tasks[i + 1].toObject();
            auto children = target.value("children").toArray();
            children.append(moving);
            target["children"] = children;
            tasks[i + 1] = target;
            tasks.removeAt(i);
            return true;
        }
        auto children = task.value("children").toArray();
        if (indentUnderNext(children, id)) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
    }
    return false;
}

static bool canIndentUnderPrevious(const QJsonArray &tasks, const QString &id, int depth = 0) {
    for (int i = 0; i < tasks.size(); ++i) {
        const auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            if (i == 0 || depth + 1 + taskSubtreeHeight(task) > 2) return false;
            return true;
        }
        if (canIndentUnderPrevious(task.value("children").toArray(), id, depth + 1)) return true;
    }
    return false;
}

static bool indentUnderPrevious(QJsonArray &tasks, const QString &id) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        if (task.value("id").toString() == id) {
            if (i == 0) return false;
            const QJsonValue moving = tasks[i];
            auto target = tasks[i - 1].toObject();
            auto children = target.value("children").toArray();
            children.append(moving);
            target["children"] = children;
            tasks[i - 1] = target;
            tasks.removeAt(i);
            return true;
        }
        auto children = task.value("children").toArray();
        if (indentUnderPrevious(children, id)) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
    }
    return false;
}

static int taskDepthIn(const QJsonArray &tasks, const QString &id, int depth = 0) {
    for (const auto &value : tasks) {
        const auto task = value.toObject();
        if (task.value("id").toString() == id) return depth;
        const int found = taskDepthIn(task.value("children").toArray(), id, depth + 1);
        if (found >= 0) return found;
    }
    return -1;
}

static bool extractForRootMove(QJsonArray &tasks, const QString &id, QJsonValue &moving,
                               QString &rootAncestor, const QString &topId = {}) {
    for (int i = 0; i < tasks.size(); ++i) {
        auto task = tasks[i].toObject();
        const QString top = topId.isEmpty() ? task.value("id").toString() : topId;
        if (task.value("id").toString() == id) {
            moving = tasks[i];
            rootAncestor = top;
            tasks.removeAt(i);
            return true;
        }
        auto children = task.value("children").toArray();
        if (extractForRootMove(children, id, moving, rootAncestor, top)) {
            task["children"] = children;
            tasks[i] = task;
            return true;
        }
    }
    return false;
}

static bool insertAfterRoot(QJsonArray &tasks, const QString &rootId, const QJsonValue &moving) {
    for (int i = 0; i < tasks.size(); ++i) {
        if (tasks[i].toObject().value("id").toString() == rootId) {
            tasks.insert(i + 1, moving);
            return true;
        }
    }
    return false;
}

Backend::Backend(QObject *parent) : QObject(parent) {
    m_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/lists.json";
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        auto root = QJsonDocument::fromJson(f.readAll()).object();
        m_lists = root.value("lists").toArray(); m_current = root.value("current").toString();
    }
    QSet<QString> taskIds;
    bool repairedTaskIds = false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        auto tasks = list.value("tasks").toArray();
        if (normalizeTaskIds(tasks, taskIds)) {
            list["tasks"] = tasks;
            m_lists[i] = list;
            repairedTaskIds = true;
        }
    }
    if (repairedTaskIds) {
        const QString backupPath = m_path + ".before-id-repair";
        if (!QFileInfo::exists(backupPath)) QFile::copy(m_path, backupPath);
        persist();
    }
    if (m_lists.isEmpty()) {
        QJsonObject list{{"id", uid()}, {"name", "Inbox"}, {"tasks", QJsonArray{}}};
        m_lists.append(list); m_current = list.value("id").toString(); persist();
    }
    bool found = false;
    for (const auto &v : m_lists) if (v.toObject().value("id").toString() == m_current) found = true;
    if (!found) m_current = m_lists.first().toObject().value("id").toString();
}
QVariantList Backend::lists() const {
    QVariantList out;
    for (const auto &v : m_lists) { auto o=v.toObject(); auto tasks=o.value("tasks").toArray(); int open=0;
        for (const auto &t:tasks) if (!t.toObject().value("done").toBool()) ++open;
        o.insert("count", open); out << o.toVariantMap(); }
    return out;
}
QJsonObject *Backend::findList(const QString &) { return nullptr; }
void Backend::selectList(const QString &id) { for (const auto &v:m_lists) if(v.toObject().value("id").toString()==id){m_current=id;persist();emit changed();return;} }
void Backend::addList(const QString &name) { auto n=name.trimmed(); if(n.isEmpty())return; QJsonObject o{{"id",uid()},{"name",n},{"tasks",QJsonArray{}}};m_lists.append(o);m_current=o.value("id").toString();persist();emit changed(); }
void Backend::renameList(const QString &id,const QString &name) { auto n=name.trimmed();if(n.isEmpty())return;for(int i=0;i<m_lists.size();++i){auto o=m_lists[i].toObject();if(o.value("id").toString()==id){o["name"]=n;m_lists[i]=o;persist();emit changed();return;}} }
void Backend::removeList(const QString &id) { if(m_lists.size()<2)return;for(int i=0;i<m_lists.size();++i)if(m_lists[i].toObject().value("id").toString()==id){m_lists.removeAt(i);if(m_current==id)m_current=m_lists.first().toObject().value("id").toString();persist();emit changed();return;} }
bool Backend::editTasks(QJsonArray &tasks,const QString &id,const QString &op,const QVariant &value) {
    for(int i=0;i<tasks.size();++i){auto o=tasks[i].toObject();if(o.value("id").toString()==id){
        if(op=="done") { o["done"]=value.toBool();auto children=o.value("children").toArray();setDoneRecursive(children,value.toBool());o["children"]=children; }
        else if(op=="text")o["text"]=value.toString();else if(op=="remove"){tasks.removeAt(i);return true;}else if(op=="add"){auto a=o.value("children").toArray();a.append(QJsonObject{{"id",uid()},{"text",value.toString()},{"done",false},{"children",QJsonArray{}}});o["children"]=a;}
        tasks[i]=o;return true;}
        auto children=o.value("children").toArray(); if(editTasks(children,id,op,value)){o["children"]=children;tasks[i]=o;return true;}}
    return false;
}
void Backend::addTask(const QString &text, const QString &parentId) {
    const QString value = text.trimmed();
    if (value.isEmpty()) return;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        QString error;
        bool added = false;
        if (parentId.isEmpty()) {
            tasks.append(QJsonObject{{"id", uid()}, {"text", value}, {"done", false}, {"children", QJsonArray{}}});
            added = true;
        } else {
            added = appendChild(tasks, parentId, value, 0, error);
        }
        if (!added) {
            if (error.isEmpty()) error = tr("That task is no longer available");
            if (m_error != error) { m_error = error; emit errorChanged(); }
            return;
        }
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return;
    }
}
void Backend::addClipboard(const QString &parentId) { const QString text=QGuiApplication::clipboard()->text().trimmed();if(text.isEmpty()){m_error=tr("Clipboard is empty");emit errorChanged();return;}addTask(text,parentId); }
void Backend::setDone(const QString &id, bool done) {
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        if (!done && taskDepthIn(tasks, id) > 0) {
            QJsonValue moving;
            QString rootAncestor;
            if (!extractForRootMove(tasks, id, moving, rootAncestor)) return;
            auto task = moving.toObject();
            task["done"] = false;
            auto children = task.value("children").toArray();
            setDoneRecursive(children, false);
            task["children"] = children;
            if (!insertAfterRoot(tasks, rootAncestor, task)) return;
        } else if (!editTasks(tasks, id, "done", done)) {
            return;
        }
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return;
    }
}
void Backend::setTaskText(const QString &id,const QString &text) {if(text.trimmed().isEmpty())return;for(int i=0;i<m_lists.size();++i){auto l=m_lists[i].toObject();if(l.value("id").toString()==m_current){auto a=l.value("tasks").toArray();editTasks(a,id,"text",text.trimmed());l["tasks"]=a;m_lists[i]=l;persist();emit changed();return;}}}
void Backend::removeTask(const QString &id) {
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        QJsonObject removed;
        QString parentId;
        int index = -1;
        if (!extractTask(tasks, id, removed, parentId, index)) return;
        m_undo = QJsonObject{{"listId", m_current}, {"parentId", parentId},
                             {"index", index}, {"task", removed}};
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        emit undoChanged();
        return;
    }
}
bool Backend::canMoveTaskOrder(const QString &id, int direction) const {
    if (id.isEmpty() || (direction != -1 && direction != 1)) return false;
    for (const auto &value : m_lists) {
        const auto list = value.toObject();
        if (list.value("id").toString() == m_current)
            return canMoveOrder(list.value("tasks").toArray(), id, direction);
    }
    return false;
}
bool Backend::moveTaskOrder(const QString &id, int direction) {
    if (!canMoveTaskOrder(id, direction)) return false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        if (!moveOrder(tasks, id, direction)) return false;
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return true;
    }
    return false;
}
bool Backend::canMoveTaskUp(const QString &id) const {
    if (id.isEmpty()) return false;
    for (const auto &value : m_lists) {
        const auto list = value.toObject();
        if (list.value("id").toString() != m_current) continue;
        const auto tasks = list.value("tasks").toArray();
        const int depth = taskDepthIn(tasks, id);
        if (depth < 0) return false;
        if (depth > 0) return canMoveOrder(tasks, id, -1) || canPromoteOneLevel(tasks, id);
        return canIndentUnderPrevious(tasks, id) || canMoveOrder(tasks, id, -1);
    }
    return false;
}
bool Backend::moveTaskUp(const QString &id) {
    if (!canMoveTaskUp(id)) return false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        const int depth = taskDepthIn(tasks, id);
        const bool moved = depth > 0 ? (canMoveOrder(tasks, id, -1)
                                           ? moveOrder(tasks, id, -1)
                                           : promoteOneLevel(tasks, id, true))
                          : canIndentUnderPrevious(tasks, id) ? indentUnderPrevious(tasks, id)
                                                            : moveOrder(tasks, id, -1);
        if (!moved) return false;
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return true;
    }
    return false;
}
bool Backend::canPromoteTask(const QString &id) const {
    if (id.isEmpty()) return false;
    for (const auto &value : m_lists) {
        const auto list = value.toObject();
        if (list.value("id").toString() == m_current)
            return canPromoteOneLevel(list.value("tasks").toArray(), id);
    }
    return false;
}
bool Backend::promoteTask(const QString &id, bool beforeParent) {
    if (!canPromoteTask(id)) return false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        if (!promoteOneLevel(tasks, id, beforeParent)) return false;
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return true;
    }
    return false;
}
bool Backend::canIndentTask(const QString &id) const {
    if (id.isEmpty()) return false;
    for (const auto &value : m_lists) {
        const auto list = value.toObject();
        if (list.value("id").toString() == m_current)
            return canIndentUnderNext(list.value("tasks").toArray(), id);
    }
    return false;
}
bool Backend::indentTask(const QString &id) {
    if (!canIndentTask(id)) return false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        if (!indentUnderNext(tasks, id)) return false;
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return true;
    }
    return false;
}
bool Backend::canIndentTaskUnderPrevious(const QString &id) const {
    if (id.isEmpty()) return false;
    for (const auto &value : m_lists) {
        const auto list = value.toObject();
        if (list.value("id").toString() == m_current)
            return canIndentUnderPrevious(list.value("tasks").toArray(), id);
    }
    return false;
}
bool Backend::indentTaskUnderPrevious(const QString &id) {
    if (!canIndentTaskUnderPrevious(id)) return false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        if (!indentUnderPrevious(tasks, id)) return false;
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return true;
    }
    return false;
}
bool Backend::canMoveTaskDown(const QString &id, const QVariantList &collapsedIds) const {
    if (id.isEmpty()) return false;
    for (const auto &value : m_lists) {
        const auto list = value.toObject();
        if (list.value("id").toString() != m_current) continue;
        const auto tasks = list.value("tasks").toArray();
        const int depth = taskDepthIn(tasks, id);
        if (depth < 0) return false;
        if (depth == 0) {
            const QString next = adjacentSiblingId(tasks, id, 1);
            if (!next.isEmpty() && idInList(collapsedIds, next)) return canMoveOrder(tasks, id, 1);
            return canIndentUnderNext(tasks, id) || canMoveOrder(tasks, id, 1);
        }
        if (canMoveOrder(tasks, id, 1)) return true;
        if (depth == 1) {
            const QString previous = adjacentSiblingId(tasks, id, -1);
            const bool previousCollapsed = !previous.isEmpty() && idInList(collapsedIds, previous);
            return (!previousCollapsed && canIndentUnderPrevious(tasks, id)) || canPromoteOneLevel(tasks, id);
        }
        return depth == 2;
    }
    return false;
}
bool Backend::moveTaskDown(const QString &id, const QVariantList &collapsedIds) {
    if (!canMoveTaskDown(id, collapsedIds)) return false;
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != m_current) continue;
        auto tasks = list.value("tasks").toArray();
        const int depth = taskDepthIn(tasks, id);
        if (depth == 0) {
            const QString next = adjacentSiblingId(tasks, id, 1);
            const bool nextCollapsed = !next.isEmpty() && idInList(collapsedIds, next);
            if (!nextCollapsed && canIndentUnderNext(tasks, id)) {
                if (!indentUnderNext(tasks, id)) return false;
            } else if (!moveOrder(tasks, id, 1)) return false;
        } else if (canMoveOrder(tasks, id, 1)) {
            if (!moveOrder(tasks, id, 1)) return false;
        } else if (depth == 1
                   && !idInList(collapsedIds, adjacentSiblingId(tasks, id, -1))
                   && canIndentUnderPrevious(tasks, id)) {
            if (!indentUnderPrevious(tasks, id)) return false;
        } else if (depth == 1) {
            if (!promoteOneLevel(tasks, id, false)) return false;
        } else {
            QJsonValue moving;
            QString rootAncestor;
            if (!extractForRootMove(tasks, id, moving, rootAncestor)
                || !insertAfterRoot(tasks, rootAncestor, moving)) return false;
        }
        list["tasks"] = tasks;
        m_lists[i] = list;
        persist();
        emit changed();
        return true;
    }
    return false;
}
void Backend::persist(){QDir().mkpath(QFileInfo(m_path).absolutePath());QSaveFile f(m_path);if(!f.open(QIODevice::WriteOnly)){m_error=tr("Could not save your lists");emit errorChanged();return;}f.write(QJsonDocument(QJsonObject{{"current",m_current},{"lists",m_lists}}).toJson());if(!f.commit()){m_error=tr("Could not save your lists");emit errorChanged();}else if(!m_error.isEmpty()){m_error.clear();emit errorChanged();}}
void Backend::undo() {
    if (m_undo.isEmpty()) return;
    const QString listId = m_undo.value("listId").toString();
    const QString parentId = m_undo.value("parentId").toString();
    const int index = m_undo.value("index").toInt();
    const QJsonObject task = m_undo.value("task").toObject();
    for (int i = 0; i < m_lists.size(); ++i) {
        auto list = m_lists[i].toObject();
        if (list.value("id").toString() != listId) continue;
        auto tasks = list.value("tasks").toArray();
        bool restored = parentId.isEmpty() ? false : insertTaskAtParent(tasks, parentId, index, task);
        if (!restored) tasks.insert(qBound(0, index, tasks.size()), task);
        list["tasks"] = tasks;
        m_lists[i] = list;
        m_undo = {};
        persist();
        emit changed();
        emit undoChanged();
        return;
    }
    m_undo = {};
    emit undoChanged();
}
void Backend::redo(){}
